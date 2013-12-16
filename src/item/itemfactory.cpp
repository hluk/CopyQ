/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "common/common.h"
#include "common/contenttype.h"
#include "item/itemwidget.h"
#include "item/serialize.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QModelIndex>
#include <QPluginLoader>

namespace {

const int dummyItemMaxChars = 4096;

bool findPluginDir(QDir *pluginsDir)
{
#if defined(COPYQ_WS_X11)
    pluginsDir->setPath( QCoreApplication::instance()->applicationDirPath() );
    if ( pluginsDir->dirName() == QString("bin")
         && pluginsDir->cdUp()
         && (pluginsDir->cd("lib64") || pluginsDir->cd("lib"))
         && pluginsDir->cd("copyq") )
    {
        // OK, installed in /usr/local/bin or /usr/bin.
    } else {
        pluginsDir->setPath( QCoreApplication::instance()->applicationDirPath() );
        if ( pluginsDir->cd("plugins") ) {
            // OK, plugins in same directory as executable.
            pluginsDir->cd("copyq");
        } else {
            return false;
        }
    }

#elif defined(Q_OS_MAC)
    pluginsDir->setPath( QCoreApplication::instance()->applicationDirPath() );
    if (pluginsDir->dirName() != "MacOS") {
        return false;
    }

    if ( pluginsDir->cdUp() // Contents
            && pluginsDir->cd("Plugins")
            && pluginsDir->cd("copyq"))
    {
        // OK, found it in the bundle
        COPYQ_LOG("Found plugins in application bundle");
    } else if (
            pluginsDir->setPath( QCoreApplication::instance()->applicationDirPath() ),
            pluginsDir->cdUp() // Contents
            && pluginsDir->cdUp() // copyq.app
            && pluginsDir->cdUp() // repo root
            && pluginsDir->cd("plugins")) {
        COPYQ_LOG("Found plugins in build tree");
    } else {
        return false;
    }

#else
    pluginsDir->setPath( QCoreApplication::instance()->applicationDirPath() );
    if ( !pluginsDir->cd("plugins") )
        return false;
#endif

    return pluginsDir->isReadable();
}

bool priorityLessThan(const ItemLoaderInterfacePtr &lhs, const ItemLoaderInterfacePtr &rhs)
{
    return lhs->priority() > rhs->priority();
}

/** Sort plugins by prioritized list of names. */
class PluginSorter {
public:
    PluginSorter(const QStringList &pluginNames) : m_order(pluginNames) {}

    int value(const ItemLoaderInterfacePtr &item) const
    {
        return m_order.indexOf( item->name() );
    }

    bool operator()(const ItemLoaderInterfacePtr &lhs, const ItemLoaderInterfacePtr &rhs) const
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
    DummyItem(const QModelIndex &index, QWidget *parent)
        : QLabel(parent)
        , ItemWidget(this)
        , m_hasText(false)
    {
        if ( index.data(contentType::hasText).toBool() ) {
            m_hasText = true;
            setMargin(0);
            setWordWrap(true);
            setTextFormat(Qt::PlainText);
            setText( index.data(contentType::text).toString().left(dummyItemMaxChars) );
            setTextInteractionFlags(Qt::TextSelectableByMouse);
            setFocusPolicy(Qt::NoFocus);
        } else {
            setMaximumSize(0, 0);
        }
    }

    QWidget *createEditor(QWidget *parent) const
    {
        return m_hasText ? ItemWidget::createEditor(parent) : NULL;
    }

    virtual void updateSize(const QSize &maximumSize)
    {
        if (m_hasText)
            ItemWidget::updateSize(maximumSize);
    }

private:
    bool m_hasText;
};

class DummyLoader : public ItemLoaderInterface
{
public:
    QString id() const { return QString(); }
    QString name() const { return QString(); }
    QString author() const { return QString(); }
    QString description() const { return QString(); }

    ItemWidget *create(const QModelIndex &index, QWidget *parent) const
    {
        return new DummyItem(index, parent);
    }

    bool canLoadItems(QFile *) { return true; }

    bool canSaveItems(const QAbstractItemModel &) { return true; }

    bool loadItems(QAbstractItemModel *model, QFile *file)
    {
        if ( file->size() > 0 ) {
            if ( !deserializeData(model, file) ) {
                model->removeRows(0, model->rowCount());
                log( QObject::tr("Item file %1 is corrupted or some CopyQ plugins are missing!")
                     .arg( quoteString(file->fileName()) ),
                     LogError );
                return false;
            }
        }

        return true;
    }

    bool saveItems(const QAbstractItemModel &model, QFile *file)
    {
        return serializeData(model, file);
    }

    bool initializeTab(QAbstractItemModel *)
    {
        return true;
    }

    bool matches(const QModelIndex &index, const QRegExp &re) const
    {
        const QString text = index.data(contentType::text).toString();
        return re.indexIn(text) != -1;
    }
};

} // namespace

ItemFactory::ItemFactory(QObject *parent)
    : QObject(parent)
    , m_loaders()
    , m_dummyLoader(new DummyLoader)
    , m_disabledLoaders()
    , m_loaderChildren()
{ 
    loadPlugins();

    if ( m_loaders.isEmpty() )
        log( QObject::tr("No plugins loaded"), LogNote );
}

ItemFactory::~ItemFactory()
{
}

ItemWidget *ItemFactory::createItem(const ItemLoaderInterfacePtr &loader,
                                    const QModelIndex &index, QWidget *parent)
{
    ItemWidget *item = loader->create(index, parent);

    if (item != NULL) {
        item = transformItem(item, index);
        QWidget *w = item->widget();
        QString notes = index.data(contentType::notes).toString();
        if (!notes.isEmpty())
            w->setToolTip(notes);

        m_loaderChildren[w] = loader;
        connect(w, SIGNAL(destroyed(QObject*)), SLOT(loaderChildDestroyed(QObject*)));
        return item;
    }

    return NULL;
}

ItemWidget *ItemFactory::createItem(const QModelIndex &index, QWidget *parent)
{
    foreach ( const ItemLoaderInterfacePtr &loader, enabledLoaders() ) {
        ItemWidget *item = createItem(loader, index, parent);
        if (item != NULL)
            return item;
    }

    return NULL;
}

ItemWidget *ItemFactory::nextItemLoader(const QModelIndex &index, ItemWidget *current)
{
    return otherItemLoader(index, current, 1);
}

ItemWidget *ItemFactory::previousItemLoader(const QModelIndex &index, ItemWidget *current)
{
    return otherItemLoader(index, current, -1);
}

QStringList ItemFactory::formatsToSave() const
{
    QStringList formats;

    foreach ( const ItemLoaderInterfacePtr &loader, enabledLoaders() ) {
        foreach ( const QString &format, loader->formatsToSave() ) {
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
    qSort( m_loaders.begin(), m_loaders.end(), PluginSorter(pluginNames) );
}

void ItemFactory::setLoaderEnabled(const ItemLoaderInterfacePtr &loader, bool enabled)
{
    if (enabled)
        m_disabledLoaders.remove(loader);
    else
        m_disabledLoaders.insert(loader);
}

bool ItemFactory::isLoaderEnabled(const ItemLoaderInterfacePtr &loader) const
{
    return !m_disabledLoaders.contains(loader);
}

ItemLoaderInterfacePtr ItemFactory::loadItems(QAbstractItemModel *model, QFile *file)
{
    foreach ( const ItemLoaderInterfacePtr &loader, enabledLoaders() ) {
        file->seek(0);
        if ( loader->canLoadItems(file) ) {
            file->seek(0);
            return loader->loadItems(model, file) ? loader : ItemLoaderInterfacePtr();
        }
    }

    return ItemLoaderInterfacePtr();
}

ItemLoaderInterfacePtr ItemFactory::initializeTab(QAbstractItemModel *model)
{
    foreach ( const ItemLoaderInterfacePtr &loader, enabledLoaders() ) {
        if ( loader->canSaveItems(*model) )
            return loader->initializeTab(model) ? loader : ItemLoaderInterfacePtr();
    }

    return ItemLoaderInterfacePtr();
}

bool ItemFactory::matches(const QModelIndex &index, const QRegExp &re) const
{
    foreach (const ItemLoaderInterfacePtr &loader, m_loaders) {
        if ( isLoaderEnabled(loader) && loader->matches(index, re) )
            return true;
    }

    return m_dummyLoader->matches(index, re);
}

void ItemFactory::loaderChildDestroyed(QObject *obj)
{
    m_loaderChildren.remove(obj);
}

ItemWidget *ItemFactory::otherItemLoader(const QModelIndex &index, ItemWidget *current, int dir)
{
    Q_ASSERT(dir == -1 || dir == 1);
    Q_ASSERT(current->widget() != NULL);

    QWidget *w = current->widget();
    ItemLoaderInterfacePtr currentLoader = m_loaderChildren[w];
    if ( currentLoader.isNull() )
        return NULL;


    const QList<ItemLoaderInterfacePtr> loaders = enabledLoaders();

    const int currentIndex = loaders.indexOf(currentLoader);
    Q_ASSERT(currentIndex != -1);

    const int size = loaders.size();
    for (int i = currentIndex + dir; i != currentIndex; i = i + dir) {
        if (i >= size)
            i = i % size;
        else if (i < 0)
            i = size - 1;

        ItemWidget *item = createItem(loaders[i], index, w->parentWidget());
        if (item != NULL)
            return item;
    }

    return NULL;
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

    foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
        if ( QLibrary::isLibrary(fileName) ) {
            const QString path = pluginsDir.absoluteFilePath(fileName);
            QPluginLoader pluginLoader(path);
            QObject *plugin = pluginLoader.instance();
            log( QObject::tr("Loading plugin: %1").arg(path), LogNote );
            if (plugin == NULL) {
                log( pluginLoader.errorString(), LogError );
            } else {
                ItemLoaderInterface *loader = qobject_cast<ItemLoaderInterface *>(plugin);
                if (loader == NULL)
                    pluginLoader.unload();
                else
                    m_loaders.append( ItemLoaderInterfacePtr(loader) );
            }
        }
    }

    qSort(m_loaders.begin(), m_loaders.end(), priorityLessThan);

    return true;
}

QList<ItemLoaderInterfacePtr> ItemFactory::enabledLoaders() const
{
    QList<ItemLoaderInterfacePtr> enabledLoaders;

    foreach (const ItemLoaderInterfacePtr &loader, m_loaders) {
        if ( isLoaderEnabled(loader) )
            enabledLoaders.append(loader);
    }

    enabledLoaders.append(m_dummyLoader);

    return enabledLoaders;
}

ItemWidget *ItemFactory::transformItem(ItemWidget *item, const QModelIndex &index)
{
    for ( int i = 0; i < m_loaders.size(); ++i ) {
        const ItemLoaderInterfacePtr &loader = m_loaders[i];
        if ( isLoaderEnabled(loader) ) {
            ItemWidget *newItem = loader->transform(item, index);
            if (newItem != NULL)
                item = newItem;
        }
    }

    return item;
}
