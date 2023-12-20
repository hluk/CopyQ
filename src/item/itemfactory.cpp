// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemfactory.h"

#include "common/appconfig.h"
#include "common/command.h"
#include "common/common.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/itemfilter.h"
#include "item/itemstore.h"
#include "item/itemwidget.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"

#include <QCoreApplication>
#include <QDir>
#include <QIODevice>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QModelIndex>
#include <QSettings>
#include <QPluginLoader>

#include <algorithm>

namespace {

bool findPluginDir(QDir *pluginsDir)
{
#ifdef COPYQ_PLUGIN_PREFIX
    pluginsDir->setPath(COPYQ_PLUGIN_PREFIX);
    if ( pluginsDir->isReadable() )
        return true;
#endif

    return platformNativeInterface()->findPluginDir(pluginsDir)
            && pluginsDir->isReadable();
}

bool priorityLessThan(const ItemLoaderPtr &lhs, const ItemLoaderPtr &rhs)
{
    return lhs->priority() > rhs->priority();
}

void trySetPixmap(QLabel *label, const QVariantMap &data, int height)
{
    const auto imageFormats = {
        "image/svg+xml",
        "image/png",
        "image/bmp",
        "image/jpeg",
        "image/gif"
    };

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

bool askToKeepCorruptedTab(const QString &tabName)
{
    const int answer = QMessageBox::question(
        nullptr,
        ItemFactory::tr("Corrupted Tab"),
        ItemFactory::tr(
            "Not all items in the tab <strong>%1</strong> were loaded successfully. "
            "Do you still want to load the tab and potentially lose some items?")
            .arg(quoteString(tabName)),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    return answer == QMessageBox::Yes;
}

/** Sort plugins by prioritized list of names. */
class PluginSorter final {
public:
    explicit PluginSorter(const QStringList &pluginNames) : m_order(pluginNames) {}

    int value(const ItemLoaderPtr &item) const
    {
        const int i = m_order.indexOf( item->id() );
        return i == -1 ? m_order.indexOf( item->name() ) : i;
    }

    bool operator()(const ItemLoaderPtr &lhs, const ItemLoaderPtr &rhs) const
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

class DummyItem final : public QLabel, public ItemWidget {
public:
    DummyItem(const QVariantMap &data, QWidget *parent, bool preview)
        : QLabel(parent)
        , ItemWidget(this)
        , m_hasText( data.contains(mimeText)
                  || data.contains(mimeTextUtf8)
                  || data.contains(mimeUriList) )
        , m_data(data)
        , m_preview(preview)
    {
        setMargin(0);
        setWordWrap(true);
        setTextFormat(Qt::PlainText);
        setFocusPolicy(Qt::NoFocus);
        setContextMenuPolicy(Qt::NoContextMenu);

        if (!preview)
            setFixedHeight(sizeHint().height());

        if ( !data.value(mimeHidden).toBool() ) {
            const int height = preview ? -1 : contentsRect().height();
            trySetPixmap(this, m_data, height);
        }

        if (preview && !hasPixmap()) {
            setTextInteractionFlags(
                    textInteractionFlags()
                    | Qt::TextSelectableByMouse
                    | Qt::TextSelectableByKeyboard
                    | Qt::LinksAccessibleByMouse
                    | Qt::LinksAccessibleByKeyboard
                    );
            setAlignment(Qt::AlignLeft | Qt::AlignTop);
            QString label = getTextData(m_data);
            if (label.isEmpty())
                label = textLabelForData(m_data);
            setText(label);
        }

        m_data.remove(mimeItemNotes);
    }

    void updateSize(QSize, int idealWidth) override
    {
        setFixedWidth(idealWidth);

        if (!m_preview && !hasPixmap()) {
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
    bool hasPixmap() const
    {
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
        return !pixmap(Qt::ReturnByValue).isNull();
#else
        return pixmap() != nullptr;
#endif
    }

    bool m_hasText;
    QVariantMap m_data;
    QString m_imageFormat;
    bool m_preview;
};

class DummySaver final : public ItemSaverInterface
{
public:
    explicit DummySaver(int itemDataThreshold)
        : m_itemDataThreshold(itemDataThreshold)
    {}

    bool saveItems(const QString & /* tabName */, const QAbstractItemModel &model, QIODevice *file) override
    {
        return serializeData(model, file, m_itemDataThreshold);
    }

private:
    int m_itemDataThreshold = -1;
};

class DummyLoader final : public ItemLoaderInterface
{
public:
    int priority() const override { return std::numeric_limits<int>::min(); }

    QString id() const override { return {}; }
    QString name() const override { return {}; }
    QString author() const override { return {}; }
    QString description() const override { return {}; }
    QVariant icon() const override { return {}; }

    void setEnabled(bool) override {}
    void loadSettings(const QSettings &) override
    {
        m_itemDataThreshold = AppConfig().option<Config::item_data_threshold>();
    }

    ItemWidget *create(const QVariantMap &data, QWidget *parent, bool preview) const override
    {
        return new DummyItem(data, parent, preview);
    }

    bool canLoadItems(QIODevice *) const override { return true; }

    bool canSaveItems(const QString &) const override { return true; }

    ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel *model, QIODevice *file, int maxItems) override
    {
        if ( file->size() > 0 ) {
            if ( !deserializeData(model, file, maxItems) ) {
                const int itemsLoadedCount = model->rowCount();
                if ( itemsLoadedCount > 0 && askToKeepCorruptedTab(tabName) ) {
                    log(QStringLiteral("Keeping corrupted tab on user request"));
                    file->close();
                    file->open(QIODevice::WriteOnly);
                    ItemSaverPtr saver = std::make_shared<DummySaver>(m_itemDataThreshold);
                    saver->saveItems(tabName, *model, file);
                    return saver;
                }

                model->removeRows(0, itemsLoadedCount);
                return nullptr;
            }
        }

        return std::make_shared<DummySaver>(m_itemDataThreshold);
    }

    ItemSaverPtr initializeTab(const QString &, QAbstractItemModel *, int) override
    {
        return std::make_shared<DummySaver>(m_itemDataThreshold);
    }

    bool matches(const QModelIndex &index, const ItemFilter &filter) const override
    {
        const QString text = index.data(contentType::text).toString();
        return filter.matches(text) || filter.matches(accentsRemoved(text));
    }

private:
    int m_itemDataThreshold = -1;
};

ItemSaverPtr transformSaver(
        QAbstractItemModel *model,
        const ItemSaverPtr &saverToTransform, const ItemLoaderPtr &currentLoader,
        const ItemLoaderList &loaders)
{
    ItemSaverPtr newSaver = saverToTransform;

    for ( auto &loader : loaders ) {
        if (loader != currentLoader && loader->isEnabled())
            newSaver = loader->transformSaver(newSaver, model);
    }

    return newSaver;
}

std::pair<ItemSaverPtr, ItemLoaderPtr> saveWithOther(
        const QString &tabName,
        QAbstractItemModel *model,
        const ItemSaverPtr &currentSaver,
        const ItemLoaderPtr &currentLoader,
        const ItemLoaderList &loaders,
        int maxItems)
{
    ItemLoaderPtr newLoader;

    for ( auto &loader : loaders ) {
        if ( loader->isEnabled() && loader->canSaveItems(tabName) ) {
            newLoader = loader;
            break;
        }
    }

    if (!newLoader || newLoader == currentLoader)
        return {currentSaver, currentLoader};

    COPYQ_LOG( QString("Tab \"%1\": Saving items using other plugin")
               .arg(tabName) );

    auto newSaver = newLoader->initializeTab(tabName, model, maxItems);
    if ( !newSaver || !saveItems(tabName, *model, newSaver) ) {
        COPYQ_LOG( QString("Tab \"%1\": Failed to re-save items")
                   .arg(tabName) );
        return {currentSaver, currentLoader};
    }

    return {newSaver, newLoader};
}

} // namespace

ItemFactory::ItemFactory(QObject *parent)
    : QObject(parent)
    , m_loaders()
    , m_dummyLoader(std::make_shared<DummyLoader>())
    , m_loaderChildren()
{
}

ItemFactory::~ItemFactory()
{
    // Plugins are unloaded at application exit.
}

ItemWidget *ItemFactory::createItem(
        const ItemLoaderPtr &loader, const QVariantMap &data,
        QWidget *parent, bool antialiasing, bool transform, bool preview)
{
    ItemWidget *item = loader->create(data, parent, preview);

    if (item != nullptr) {
        if (transform)
            item = transformItem(item, data);
        QWidget *w = item->widget();
        const auto notes = getTextData(data, mimeItemNotes);
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
        connect(w, &QObject::destroyed, this, &ItemFactory::loaderChildDestroyed);
        return item;
    }

    return nullptr;
}

ItemWidget *ItemFactory::createItem(
        const QVariantMap &data, QWidget *parent, bool antialiasing, bool transform, bool preview)
{
    for ( auto &loader : m_loaders ) {
        if ( !loader->isEnabled() )
            continue;

        ItemWidget *item = createItem(loader, data, parent, antialiasing, transform, preview);
        if (item != nullptr)
            return item;
    }

    return createSimpleItem(data, parent, antialiasing);
}

ItemWidget *ItemFactory::createSimpleItem(
        const QVariantMap &data, QWidget *parent, bool antialiasing)
{
    return createItem(m_dummyLoader, data, parent, antialiasing);
}

QStringList ItemFactory::formatsToSave() const
{
    QStringList formats;

    for (const auto &loader : m_loaders) {
        if ( !loader->isEnabled() )
            continue;

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
    if ( !formats.contains(mimeTextUtf8) )
        formats.append(mimeTextUtf8);

    return formats;
}

void ItemFactory::setPluginPriority(const QStringList &pluginNames)
{
    m_loaders.removeOne(m_dummyLoader);
    std::sort( m_loaders.begin(), m_loaders.end(), PluginSorter(pluginNames) );
    m_loaders.append(m_dummyLoader);
}

ItemSaverPtr ItemFactory::loadItems(const QString &tabName, QAbstractItemModel *model, QIODevice *file, int maxItems)
{
    for (auto &loader : m_loaders) {
        if ( !loader->isEnabled() )
            continue;

        file->seek(0);
        if ( loader->canLoadItems(file) ) {
            file->seek(0);
            auto saver = loader->loadItems(tabName, model, file, maxItems);
            if (!saver)
                return nullptr;
            file->close();
            ItemLoaderPtr newLoader;
            std::tie(saver, newLoader) = saveWithOther(tabName, model, saver, loader, m_loaders, maxItems);
            return transformSaver(model, saver, newLoader, m_loaders);
        }
    }

    const auto errorString =
            QObject::tr("Tab %1 is corrupted or some CopyQ plugins are missing!")
            .arg( quoteString(tabName) );
    emitError(errorString);

    return nullptr;
}

ItemSaverPtr ItemFactory::initializeTab(const QString &tabName, QAbstractItemModel *model, int maxItems)
{
    for (const auto &loader : m_loaders) {
        if ( loader->isEnabled() && loader->canSaveItems(tabName) ) {
            const auto saver = loader->initializeTab(tabName, model, maxItems);
            return saver ? transformSaver(model, saver, loader, m_loaders) : nullptr;
        }
    }

    return nullptr;
}

bool ItemFactory::matches(const QModelIndex &index, const ItemFilter &filter) const
{
    if ( filter.matchesIndex(index) )
        return true;

    for (const auto &loader : m_loaders) {
        if ( loader->isEnabled() && loader->matches(index, filter) )
            return true;
    }

    return false;
}

ItemScriptable *ItemFactory::scriptableObject(const QString &name) const
{
    for (const auto &loader : m_loaders) {
        if (loader->id() == name)
            return loader->isEnabled() ? loader->scriptableObject() : nullptr;
    }
    return nullptr;
}

QVector<Command> ItemFactory::commands(bool enabled) const
{
    QVector<Command> commands;

    for (const auto &loader : m_loaders) {
        if (loader->isEnabled() == enabled)
            commands << loader->commands();
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

bool ItemFactory::loadPlugins()
{
    QDir pluginsDir;
    if ( !findPluginDir(&pluginsDir) )
        return false;

    // Plugins can be loaded only once.
    static bool pluginsLoaded = false;
    Q_ASSERT(!pluginsLoaded);
    if (pluginsLoaded)
        return true;
    pluginsLoaded = true;

    for (const auto &fileName : pluginsDir.entryList(QDir::Files)) {
        const QString path = pluginsDir.absoluteFilePath(fileName);
        auto loader = loadPlugin(path, QString());
        if (loader)
            addLoader(loader);
    }

    std::sort(m_loaders.begin(), m_loaders.end(), priorityLessThan);
    addLoader(m_dummyLoader);

    return true;
}

void ItemFactory::loadItemFactorySettings(QSettings *settings)
{
    // load settings for each plugin
    settings->beginGroup("Plugins");
    for (const auto &loader : m_loaders) {
        const bool enabled = loadItemFactorySettings(loader, settings);
        loader->setEnabled(enabled);
    }
    settings->endGroup();

    // load plugin priority
    const QStringList pluginPriority =
            settings->value("plugin_priority", QStringList()).toStringList();
    setPluginPriority(pluginPriority);
}

QObject *ItemFactory::createExternalEditor(const QModelIndex &index, const QVariantMap &data, QWidget *parent) const
{
    for (const auto &loader : m_loaders) {
        if ( !loader->isEnabled() )
            continue;

        QObject *editor = loader->createExternalEditor(index, data, parent);
        if (editor != nullptr)
            return editor;
    }

    return nullptr;
}

QVariantMap ItemFactory::data(const QModelIndex &index) const
{
    QVariantMap data = index.data(contentType::data).toMap();
    for (const auto &loader : m_loaders) {
        if ( loader->isEnabled() && !loader->data(&data, index) )
            return QVariantMap();
    }
    return data;
}

bool ItemFactory::setData(const QVariantMap &data, const QModelIndex &index, QAbstractItemModel *model) const
{
    for (const auto &loader : m_loaders) {
        if ( loader->isEnabled() && loader->setData(data, index, model) )
            return true;
    }

    return model->setData(index, data, contentType::updateData);
}

ItemWidget *ItemFactory::transformItem(ItemWidget *item, const QVariantMap &data)
{
    for (const auto &loader : m_loaders) {
        if ( !loader->isEnabled() )
            continue;

        ItemWidget *newItem = loader->transform(item, data);
        if (newItem != nullptr)
            item = newItem;
    }

    return item;
}

void ItemFactory::addLoader(const ItemLoaderPtr &loader)
{
    m_loaders.append(loader);
    const QObject *signaler = loader->signaler();
    if (signaler) {
        const auto loaderMetaObject = signaler->metaObject();

        if ( loaderMetaObject->indexOfSignal("error(QString)") != -1 )
            connect( signaler, SIGNAL(error(QString)), this, SIGNAL(error(QString)) );

        if ( loaderMetaObject->indexOfSignal("addCommands(QVector<Command>)") != -1 )
            connect( signaler, SIGNAL(addCommands(QVector<Command>)), this, SIGNAL(addCommands(QVector<Command>)) );
    }
}

ItemLoaderPtr ItemFactory::loadPlugin(const QString &path, const QString &id) const
{
    if ( !QLibrary::isLibrary(path) )
        return ItemLoaderPtr();

    COPYQ_LOG_VERBOSE( QString("Loading plugin: %1").arg(path) );
    QPluginLoader pluginLoader(path);
    QObject *plugin = pluginLoader.instance();
    if (plugin == nullptr) {
        log( pluginLoader.errorString(), LogError );
        return ItemLoaderPtr();
    }

    ItemLoaderPtr loader( qobject_cast<ItemLoaderInterface *>(plugin) );
    if ( loader == nullptr || (!id.isEmpty() && id != loader->id()) ) {
        COPYQ_LOG_VERBOSE( QString("Unloading plugin: %1").arg(path) );
        loader = nullptr;
        pluginLoader.unload();
        return ItemLoaderPtr();
    }

    return loader;
}

bool ItemFactory::loadItemFactorySettings(const ItemLoaderPtr &loader, QSettings *settings) const
{
    if (loader == m_dummyLoader) {
        loader->loadSettings(*settings);
        return true;
    }

    settings->beginGroup(loader->id());

    const auto enabled = settings->value("enabled", true).toBool();
    loader->loadSettings(*settings);

    settings->endGroup();

#ifdef HAS_TESTS
    // For tests, enable only the tested plugin.
    const QString testId = qApp->property("CopyQ_test_id").toString();
    if ( !testId.isEmpty() )
        return loader->id() == testId;
#endif
    return enabled;
}
