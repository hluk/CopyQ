/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "itemfactory.h"

#include "common/command.h"
#include "common/common.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "item/itemwidget.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QModelIndex>
#include <QPluginLoader>

#include <algorithm>

namespace {

bool findPluginDir(QDir *pluginsDir)
{
    return createPlatformNativeInterface()->findPluginDir(pluginsDir)
            && pluginsDir->isReadable();
}

bool priorityLessThan(const ItemLoaderInterface *lhs, const ItemLoaderInterface *rhs)
{
    return lhs->priority() > rhs->priority();
}

void trySetPixmap(QLabel *label, const QVariantMap &data, int height)
{
    static const QStringList imageFormats = QStringList()
            << QString("image/svg+xml")
            << QString("image/png")
            << QString("image/bmp")
            << QString("image/jpeg")
            << QString("image/gif");

    for (const auto &format : imageFormats) {
        QPixmap pixmap;
        if (pixmap.loadFromData(data.value(format).toByteArray())) {
            if (height > 0)
                pixmap = pixmap.scaledToHeight(height, Qt::SmoothTransformation);
            label->setPixmap(pixmap);
            break;
        }
    }
}


/** Sort plugins by prioritized list of names. */
class PluginSorter {
public:
    PluginSorter(const QStringList &pluginNames) : m_order(pluginNames) {}

    int value(const ItemLoaderInterface *item) const
    {
        const int i = m_order.indexOf( item->id() );
        return i == -1 ? m_order.indexOf( item->name() ) : i;
    }

    bool operator()(const ItemLoaderInterface *lhs, const ItemLoaderInterface *rhs) const
    {
        const int l = value(lhs);
        const int r = value(rhs);

        if (l == -1)
            return (r == -1) && lhs->priority() > rhs->priority();

        if (r == -1)
            return true;

        return l < r;
    }

private:
    const QStringList &m_order;
};

class DummyItem : public QLabel, public ItemWidget {
public:
    DummyItem(const QModelIndex &index, QWidget *parent, bool preview)
        : QLabel(parent)
        , ItemWidget(this)
        , m_hasText(false)
        , m_data(index.data(contentType::data).toMap())
    {
        m_hasText = index.data(contentType::hasText).toBool();
        setMargin(0);
        setWordWrap(true);
        setTextFormat(Qt::PlainText);
        setTextInteractionFlags(Qt::TextSelectableByMouse);
        setFocusPolicy(Qt::NoFocus);
        setContextMenuPolicy(Qt::NoContextMenu);

        if (!preview)
            setFixedHeight(sizeHint().height());

        if ( !index.data(contentType::isHidden).toBool() ) {
            const int height = preview ? -1 : contentsRect().height();
            trySetPixmap(this, m_data, height);
        }

        if (preview && !pixmap()) {
            setAlignment(Qt::AlignLeft | Qt::AlignTop);
            QString label = getTextData(m_data);
            if (label.isEmpty())
                label = textLabelForData(m_data);
            setText(label);
        }
    }

    QWidget *createEditor(QWidget *parent) const override
    {
        return m_hasText ? ItemWidget::createEditor(parent) : nullptr;
    }

    void updateSize(const QSize &, int idealWidth) override
    {
        setFixedWidth(idealWidth);

        if (!pixmap()) {
            const int width = contentsRect().width();
            const QString label =
                    textLabelForData(m_data, font(), QString(), false, width, 1);
            setText(label);
        }
    }

    void setTagged(bool tagged) override
    {
        setVisible( !tagged || (m_hasText && !m_data.contains(mimeHidden)) );
    }

private:
    bool m_hasText;
    QVariantMap m_data;
    QString m_imageFormat;
};

class DummyLoader : public ItemLoaderInterface
{
public:
    explicit DummyLoader(ItemFactory *factory) : m_factory(factory) {}

    QString id() const override { return QString(); }
    QString name() const override { return QString(); }
    QString author() const override { return QString(); }
    QString description() const override { return QString(); }

    ItemWidget *create(const QModelIndex &index, QWidget *parent, bool preview) const override
    {
        return new DummyItem(index, parent, preview);
    }

    bool canLoadItems(QFile *) const override { return true; }

    bool canSaveItems(const QAbstractItemModel &) const override { return true; }

    bool loadItems(QAbstractItemModel *model, QFile *file) override
    {
        if ( file->size() > 0 ) {
            if ( !deserializeData(model, file) ) {
                model->removeRows(0, model->rowCount());
                const QString errorString =
                        QObject::tr("Item file %1 is corrupted or some CopyQ plugins are missing!")
                        .arg(quoteString(file->fileName()) );
                m_factory->emitError(errorString);
                return false;
            }
        }

        return true;
    }

    bool saveItems(const QAbstractItemModel &model, QFile *file) override
    {
        return serializeData(model, file);
    }

    bool initializeTab(QAbstractItemModel *) override
    {
        return true;
    }

    bool matches(const QModelIndex &index, const QRegExp &re) const override
    {
        const QString text = index.data(contentType::text).toString();
        return re.indexIn(text) != -1;
    }

private:
    ItemFactory *m_factory;
};

} // namespace

ItemFactory::ItemFactory(QObject *parent)
    : QObject(parent)
    , m_loaders()
    , m_dummyLoader(new DummyLoader(this))
    , m_disabledLoaders()
    , m_loaderChildren()
{
    loadPlugins();

    if ( m_loaders.isEmpty() )
        log( QObject::tr("No plugins loaded"), LogNote );
}

ItemFactory::~ItemFactory()
{
    // Plugins are unloaded at application exit.
}

ItemWidget *ItemFactory::createItem(
        ItemLoaderInterface *loader, const QModelIndex &index,
        QWidget *parent, bool antialiasing, bool transform, bool preview)
{
    ItemWidget *item = loader->create(index, parent, preview);

    if (item != nullptr) {
        if (transform)
            item = transformItem(item, index);
        QWidget *w = item->widget();
        QString notes = index.data(contentType::notes).toString();
        if (!notes.isEmpty())
            w->setToolTip(notes);

        if (!antialiasing) {
            QFont f = w->font();
            f.setStyleStrategy(QFont::NoAntialias);
            w->setFont(f);
            for (auto child : w->findChildren<QWidget *>("item_child"))
                child->setFont(f);
        }

        m_loaderChildren[w] = loader;
        connect(w, SIGNAL(destroyed(QObject*)), SLOT(loaderChildDestroyed(QObject*)));
        return item;
    }

    return nullptr;
}

ItemWidget *ItemFactory::createItem(
        const QModelIndex &index, QWidget *parent, bool antialiasing, bool transform, bool preview)
{
    for ( auto loader : enabledLoaders() ) {
        ItemWidget *item = createItem(loader, index, parent, antialiasing, transform, preview);
        if (item != nullptr)
            return item;
    }

    return nullptr;
}

ItemWidget *ItemFactory::createSimpleItem(
        const QModelIndex &index, QWidget *parent, bool antialiasing)
{
    return createItem(m_dummyLoader, index, parent, antialiasing);
}

QStringList ItemFactory::formatsToSave() const
{
    QStringList formats;

    for ( const auto loader : enabledLoaders() ) {
        for ( const auto &format : loader->formatsToSave() ) {
            if ( !formats.contains(format) )
                formats.append(format);
        }
    }

    if ( !formats.contains(mimeText) )
        formats.prepend(mimeText);

    if ( !formats.contains(mimeItemNotes) )
        formats.append(mimeItemNotes);
    if ( !formats.contains(mimeItems) )
        formats.append(mimeItems);

    return formats;
}

void ItemFactory::setPluginPriority(const QStringList &pluginNames)
{
    std::sort( m_loaders.begin(), m_loaders.end(), PluginSorter(pluginNames) );
}

void ItemFactory::setLoaderEnabled(ItemLoaderInterface *loader, bool enabled)
{
    if (enabled)
        m_disabledLoaders.remove(loader);
    else
        m_disabledLoaders.insert(loader);
}

bool ItemFactory::isLoaderEnabled(const ItemLoaderInterface *loader) const
{
    return !m_disabledLoaders.contains(loader);
}

ItemLoaderInterface *ItemFactory::loadItems(QAbstractItemModel *model, QFile *file)
{
    for ( auto loader : enabledLoaders() ) {
        file->seek(0);
        if ( loader->canLoadItems(file) ) {
            file->seek(0);
            return loader->loadItems(model, file) ? loader : nullptr;
        }
    }

    return nullptr;
}

ItemLoaderInterface *ItemFactory::initializeTab(QAbstractItemModel *model)
{
    for ( auto loader : enabledLoaders() ) {
        if ( loader->canSaveItems(*model) )
            return loader->initializeTab(model) ? loader : nullptr;
    }

    return nullptr;
}

bool ItemFactory::matches(const QModelIndex &index, const QRegExp &re) const
{
    // Match formats if the filter expression contains single '/'.
    if (re.pattern().count('/') == 1) {
        const QVariantMap data = index.data(contentType::data).toMap();
        for (const auto &format : data.keys()) {
            if (re.exactMatch(format))
                return true;
        }
    }

    for ( const auto loader : enabledLoaders() ) {
        if ( isLoaderEnabled(loader) && loader->matches(index, re) )
            return true;
    }

    return false;
}

QString ItemFactory::scripts() const
{
    QString script = "var plugins = {}\n";

    for ( const auto loader : enabledLoaders() )
        script.append( loader->script() + '\n' );

    return script;
}

QList<Command> ItemFactory::commands() const
{
    QList<Command> commands;

    for ( const auto loader : enabledLoaders() ) {
        QList <Command> subCommands = loader->commands();

        for (auto &subCommand : subCommands)
            subCommand.name.prepend(loader->name() + '|');

        commands << subCommands;
    }

    return commands;
}

void ItemFactory::emitError(const QString &errorString)
{
    log(errorString, LogError);
    emit error(errorString);
}

void ItemFactory::loaderChildDestroyed(QObject *obj)
{
    m_loaderChildren.remove(obj);
}

ItemWidget *ItemFactory::otherItemLoader(
        const QModelIndex &index, ItemWidget *current, bool next, bool antialiasing)
{
    Q_ASSERT(current->widget() != nullptr);

    QWidget *w = current->widget();
    ItemLoaderInterface *currentLoader = m_loaderChildren[w];
    if (!currentLoader)
        return nullptr;


    const ItemLoaderList loaders = enabledLoaders();

    const int currentIndex = loaders.indexOf(currentLoader);
    Q_ASSERT(currentIndex != -1);

    const int size = loaders.size();
    const int dir = next ? 1 : -1;
    for (int i = currentIndex + dir; i != currentIndex; i = i + dir) {
        if (i >= size)
            i = i % size;
        else if (i < 0)
            i = size - 1;

        ItemWidget *item = createItem(loaders[i], index, w->parentWidget(), antialiasing);
        if (item != nullptr)
            return item;
    }

    return nullptr;
}

bool ItemFactory::loadPlugins()
{
#ifdef COPYQ_PLUGIN_PREFIX
    QDir pluginsDir(COPYQ_PLUGIN_PREFIX);
    if ( !pluginsDir.isReadable() && !findPluginDir(&pluginsDir))
        return false;
#else
    QDir pluginsDir;
    if ( !findPluginDir(&pluginsDir))
        return false;
#endif

    for (const auto &fileName : pluginsDir.entryList(QDir::Files)) {
        if ( QLibrary::isLibrary(fileName) ) {
            const QString path = pluginsDir.absoluteFilePath(fileName);
            QPluginLoader pluginLoader(path);
            QObject *plugin = pluginLoader.instance();
            log( QObject::tr("Loading plugin: %1").arg(path), LogNote );
            if (plugin == nullptr) {
                log( pluginLoader.errorString(), LogError );
            } else {
                ItemLoaderInterface *loader = qobject_cast<ItemLoaderInterface *>(plugin);
                if (loader == nullptr)
                    pluginLoader.unload();
                else
                    addLoader(loader);
            }
        }
    }

    std::sort(m_loaders.begin(), m_loaders.end(), priorityLessThan);

    return true;
}

ItemLoaderList ItemFactory::enabledLoaders() const
{
    ItemLoaderList enabledLoaders;

    for (auto loader : m_loaders) {
        if ( isLoaderEnabled(loader) )
            enabledLoaders.append(loader);
    }

    enabledLoaders.append(m_dummyLoader);

    return enabledLoaders;
}

ItemWidget *ItemFactory::transformItem(ItemWidget *item, const QModelIndex &index)
{
    for (auto loader : m_loaders) {
        if ( isLoaderEnabled(loader) ) {
            ItemWidget *newItem = loader->transform(item, index);
            if (newItem != nullptr)
                item = newItem;
        }
    }

    return item;
}

void ItemFactory::addLoader(ItemLoaderInterface *loader)
{
    m_loaders.append(loader);
    const QObject *signaler = loader->signaler();
    if (signaler)
        connect( signaler, SIGNAL(error(QString)), this, SIGNAL(error(QString)) );
}
