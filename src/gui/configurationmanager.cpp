/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "configurationmanager.h"
#include "ui_configurationmanager.h"

#include "common/appconfig.h"
#include "common/command.h"
#include "common/common.h"
#include "common/config.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/option.h"
#include "common/settings.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/pluginwidget.h"
#include "gui/windowgeometryguard.h"
#include "item/clipboardmodel.h"
#include "item/itemdelegate.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "platform/platformnativeinterface.h"

#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QTranslator>

#ifdef Q_OS_WIN
#   define DEFAULT_EDITOR "notepad %1"
#elif defined(Q_OS_MAC)
#   define DEFAULT_EDITOR "open -t %1"
#else
#   define DEFAULT_EDITOR "gedit %1"
#endif

namespace {

class PluginItem : public ItemOrderList::Item {
public:
    explicit PluginItem(const ItemLoaderInterfacePtr &loader)
        : m_loader(loader)
    {
    }

private:
    QWidget *createWidget(QWidget *parent) const
    {
        return new PluginWidget(m_loader, parent);
    }

    ItemLoaderInterfacePtr m_loader;
};

QString defaultClipboardTabName()
{
    return ConfigurationManager::tr(
                "&clipboard", "Default name of the tab that automatically stores new clipboard content");
}

void printItemFileError(const QString &id, const QString &fileName, const QFile &file)
{
    log( ConfigurationManager::tr("Cannot save tab %1 to %2 (%3)!")
         .arg( quoteString(id) )
         .arg( quoteString(fileName) )
         .arg( file.errorString() )
         , LogError );
}

bool needToSaveItemsAgain(const QAbstractItemModel &model, const ItemFactory &itemFactory,
                          const ItemLoaderInterfacePtr &currentLoader)
{
    if (!currentLoader)
        return false;

    bool saveWithCurrent = true;
    foreach ( const ItemLoaderInterfacePtr &loader, itemFactory.loaders() ) {
        if ( itemFactory.isLoaderEnabled(loader) && loader->canSaveItems(model) )
            return loader != currentLoader;
        else if (loader == currentLoader)
            saveWithCurrent = false;
    }

    return !saveWithCurrent;
}

QString nativeLanguageName(const QString &localeName)
{
    // Traditional Chinese
    if (localeName == "zh_TW")
        return QString::fromUtf8("\xe6\xad\xa3\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87");

    // Simplified Chinese
    if (localeName == "zh_CN")
        return QString::fromUtf8("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87");

    return QLocale(localeName).nativeLanguageName();
}

} // namespace

// singleton
ConfigurationManager *ConfigurationManager::m_Instance = 0;

ConfigurationManager *ConfigurationManager::instance()
{
    Q_ASSERT(m_Instance != NULL);
    return m_Instance;
}

ConfigurationManager::ConfigurationManager(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConfigurationManager)
    , m_options()
    , m_tabIcons()
    , m_itemFactory(new ItemFactory(this))
    , m_optionWidgetsLoaded(false)
{
    ui->setupUi(this);
    setWindowIcon(appIcon());

    if ( !itemFactory()->hasLoaders() )
        ui->tabItems->deleteLater();

    initOptions();

    connect(m_itemFactory, SIGNAL(error(QString)), SIGNAL(error(QString)));
}

ConfigurationManager::~ConfigurationManager()
{
    m_Instance = NULL;
    delete ui;
}

ItemLoaderInterfacePtr ConfigurationManager::loadItems(ClipboardModel &model)
{
    if ( !createItemDirectory() )
        return ItemLoaderInterfacePtr();

    const QString tabName = model.property("tabName").toString();
    const QString fileName = itemFileName(tabName);

    // Load file with items.
    QFile file(fileName);
    if ( !file.exists() ) {
        // Try to open temporary file if regular file doesn't exist.
        QFile tmpFile(fileName + ".tmp");
        if ( tmpFile.exists() )
            tmpFile.rename(fileName);
    }

    ItemLoaderInterfacePtr loader;

    model.setDisabled(true);

    if ( file.exists() ) {
        COPYQ_LOG( QString("Tab \"%1\": Loading items").arg(tabName) );
        if ( file.open(QIODevice::ReadOnly) )
            loader = itemFactory()->loadItems(&model, &file);
        saveItemsWithOther(model, &loader);
    } else {
        COPYQ_LOG( QString("Tab \"%1\": Creating new tab").arg(tabName) );
        if ( file.open(QIODevice::WriteOnly) ) {
            file.close();
            loader = itemFactory()->initializeTab(&model);
            saveItems(model, loader);
        }
    }

    file.close();

    if (loader) {
        COPYQ_LOG( QString("Tab \"%1\": %2 items loaded").arg(tabName).arg(model.rowCount()) );
    } else {
        model.removeRows(0, model.rowCount());
        COPYQ_LOG( QString("Tab \"%1\": Disabled").arg(tabName) );
    }

    model.setDisabled(!loader);

    return loader;
}

bool ConfigurationManager::saveItems(const ClipboardModel &model,
                                     const ItemLoaderInterfacePtr &loader)
{
    const QString tabName = model.property("tabName").toString();
    const QString fileName = itemFileName(tabName);

    if ( !createItemDirectory() )
        return false;

    // Save to temp file.
    QFile file( fileName + ".tmp" );
    if ( !file.open(QIODevice::WriteOnly) ) {
        printItemFileError(tabName, fileName, file);
        return false;
    }

    COPYQ_LOG( QString("Tab \"%1\": Saving %2 items").arg(tabName).arg(model.rowCount()) );

    if ( loader->saveItems(model, &file) ) {
        // Overwrite previous file.
        QFile oldTabFile(fileName);
        if (oldTabFile.exists() && !oldTabFile.remove())
            printItemFileError(tabName, fileName, oldTabFile);
        else if ( file.rename(fileName) )
            COPYQ_LOG( QString("Tab \"%1\": Items saved").arg(tabName) );
        else
            printItemFileError(tabName, fileName, file);
    } else {
        COPYQ_LOG( QString("Tab \"%1\": Failed to save items!").arg(tabName) );
    }

    return true;
}

bool ConfigurationManager::saveItemsWithOther(ClipboardModel &model,
                                              ItemLoaderInterfacePtr *loader)
{
    if ( !needToSaveItemsAgain(model, *itemFactory(), *loader) )
        return false;

    model.setDisabled(true);

    COPYQ_LOG( QString("Tab \"%1\": Saving items using other plugin")
               .arg(model.property("tabName").toString()) );

    (*loader)->uninitializeTab(&model);
    *loader = itemFactory()->initializeTab(&model);
    if ( *loader && saveItems(model, *loader) ) {
        model.setDisabled(false);
        return true;
    } else {
        COPYQ_LOG( QString("Tab \"%1\": Failed to re-save items")
               .arg(model.property("tabName").toString()) );
    }

    return false;
}

void ConfigurationManager::removeItems(const QString &tabName)
{
    const QString tabFileName = itemFileName(tabName);
    QFile::remove(tabFileName);
    QFile::remove(tabFileName + ".tmp");
}

void ConfigurationManager::moveItems(const QString &oldId, const QString &newId)
{
    const QString oldFileName = itemFileName(oldId);
    const QString newFileName = itemFileName(newId);

    if ( oldFileName != newFileName && QFile::copy(oldFileName, newFileName) ) {
        QFile::remove(oldFileName);
    } else {
        COPYQ_LOG( QString("Failed to move items from \"%1\" (tab \"%2\") to \"%3\" (tab \"%4\")")
                   .arg(oldFileName).arg(oldId)
                   .arg(newFileName).arg(newId) );
    }
}

QString ConfigurationManager::itemFileName(const QString &id) const
{
    QString part( id.toUtf8().toBase64() );
    part.replace( QChar('/'), QString('-') );
    return getConfigurationFilePath("_tab_") + part + QString(".dat");
}

bool ConfigurationManager::createItemDirectory()
{
    QDir settingsDir( settingsDirectoryPath() );
    if ( !settingsDir.mkpath(".") ) {
        log( tr("Cannot create directory for settings %1!")
             .arg(quoteString(settingsDir.path()) ),
             LogError );

        return false;
    }

    return true;
}

QString ConfigurationManager::defaultTabName() const
{
    const QString tab = AppConfig().option("clipboard_tab").toString();
    return tab.isEmpty() ? defaultClipboardTabName() : tab;
}

void ConfigurationManager::initTabComboBox(QComboBox *comboBox) const
{
    initTabComboBox(comboBox, tabs());
}

void ConfigurationManager::initTabIcons()
{
    QTabWidget *tw = ui->tabWidget;
    if ( !tw->tabIcon(0).isNull() )
        return;

    tw->setTabIcon( tw->indexOf(ui->tabGeneral), getIcon("", IconWrench) );
    tw->setTabIcon( tw->indexOf(ui->tabLayout), getIcon("", IconColumns) );
    tw->setTabIcon( tw->indexOf(ui->tabHistory), getIcon("", IconListAlt) );
    tw->setTabIcon( tw->indexOf(ui->tabItems), getIcon("", IconThList) );
    tw->setTabIcon( tw->indexOf(ui->tabTray), getIcon("", IconInbox) );
    tw->setTabIcon( tw->indexOf(ui->tabNotifications), getIcon("", IconInfoSign) );
    tw->setTabIcon( tw->indexOf(ui->tabShortcuts), getIcon("", IconKeyboard) );
    tw->setTabIcon( tw->indexOf(ui->tabAppearance), getIcon("", IconPicture) );
}

void ConfigurationManager::initPluginWidgets()
{
    if (!itemFactory()->hasLoaders())
        return;

    ui->itemOrderListPlugins->clearItems();

    foreach ( const ItemLoaderInterfacePtr &loader, itemFactory()->loaders() ) {
        ItemOrderList::ItemPtr pluginItem(new PluginItem(loader));
        const QIcon icon = getIcon(loader->icon());
        ui->itemOrderListPlugins->appendItem(
                    loader->name(), itemFactory()->isLoaderEnabled(loader), false, icon, pluginItem );
    }
}

void ConfigurationManager::initLanguages()
{
    ui->comboBoxLanguage->addItem("English");
    ui->comboBoxLanguage->setItemData(0, "en");

    const QString currentLocale = QLocale().name();
    bool currentLocaleFound = false; // otherwise not found or partial match ("uk" partially matches locale "uk_UA")
    QSet<QString> languages;

    foreach ( const QString &path, qApp->property("CopyQ_translation_directories").toStringList() ) {
        foreach ( const QString &item, QDir(path).entryList(QStringList("copyq_*.qm")) ) {
            const int i = item.indexOf('_');
            const QString locale = item.mid(i + 1, item.lastIndexOf('.') - i - 1);
            const QString language = nativeLanguageName(locale);

            if (!language.isEmpty()) {
                languages.insert(language);
                const int index = ui->comboBoxLanguage->count();
                ui->comboBoxLanguage->addItem(language);
                ui->comboBoxLanguage->setItemData(index, locale);

                if (!currentLocaleFound) {
                    currentLocaleFound = (locale == currentLocale);
                    if (currentLocaleFound || currentLocale.startsWith(locale + "_"))
                        ui->comboBoxLanguage->setCurrentIndex(index);
                }
            }
        }
    }

    ui->comboBoxLanguage->setSizeAdjustPolicy(QComboBox::AdjustToContents);
}

void ConfigurationManager::updateAutostart()
{
    PlatformPtr platform = createPlatformNativeInterface();

    if ( platform->canAutostart() ) {
        bind("autostart", ui->checkBoxAutostart, platform->isAutostartEnabled());
    } else {
        ui->checkBoxAutostart->hide();
    }
}

void ConfigurationManager::setAutostartEnable()
{
    PlatformPtr platform = createPlatformNativeInterface();
    platform->setAutostartEnabled( AppConfig().option("autostart").toBool() );
}

void ConfigurationManager::initOptions()
{
    /* general options */
    bind("autostart", ui->checkBoxAutostart, false);
    bind("clipboard_tab", ui->comboBoxClipboardTab->lineEdit(), defaultClipboardTabName());
    bind("maxitems", ui->spinBoxItems, 200);
    bind("expire_tab", ui->spinBoxExpireTab, 0);
    bind("editor", ui->lineEditEditor, DEFAULT_EDITOR);
    bind("item_popup_interval", ui->spinBoxNotificationPopupInterval, 0);
    bind("notification_position", ui->comboBoxNotificationPosition, 3);
    bind("clipboard_notification_lines", ui->spinBoxClipboardNotificationLines, 0);
    bind("notification_horizontal_offset", ui->spinBoxNotificationHorizontalOffset, 10);
    bind("notification_vertical_offset", ui->spinBoxNotificationVerticalOffset, 10);
    bind("notification_maximum_width", ui->spinBoxNotificationMaximumWidth, 300);
    bind("notification_maximum_height", ui->spinBoxNotificationMaximumHeight, 100);
    bind("edit_ctrl_return", ui->checkBoxEditCtrlReturn, true);
    bind("move", ui->checkBoxMove, true);
    bind("check_clipboard", ui->checkBoxClip, true);
    bind("confirm_exit", ui->checkBoxConfirmExit, true);
    bind("vi", ui->checkBoxViMode, false);
    bind("save_filter_history", ui->checkBoxSaveFilterHistory, false);
    bind("always_on_top", ui->checkBoxAlwaysOnTop, false);
    bind("open_windows_on_current_screen", ui->checkBoxOpenWindowsOnCurrentScreen, true);
    bind("transparency_focused", ui->spinBoxTransparencyFocused, 0);
    bind("transparency", ui->spinBoxTransparencyUnfocused, 0);
    bind("hide_tabs", ui->checkBoxHideTabs, false);
    bind("hide_toolbar", ui->checkBoxHideToolbar, false);
    bind("hide_toolbar_labels", ui->checkBoxHideToolbarLabels, true);
    bind("disable_tray", ui->checkBoxDisableTray, false);
    bind("hide_main_window", ui->checkBoxHideWindow, false);
    bind("tab_tree", ui->checkBoxTabTree, false);
    bind("show_tab_item_count", ui->checkBoxShowTabItemCount, false);
    bind("text_wrap", ui->checkBoxTextWrap, true);

    bind("activate_closes", ui->checkBoxActivateCloses, true);
    bind("activate_focuses", ui->checkBoxActivateFocuses, false);
    bind("activate_pastes", ui->checkBoxActivatePastes, false);

    bind("tray_items", ui->spinBoxTrayItems, 5);
    bind("tray_item_paste", ui->checkBoxPasteMenuItem, true);
    bind("tray_commands", ui->checkBoxTrayShowCommands, true);
    bind("tray_tab_is_current", ui->checkBoxMenuTabIsCurrent, true);
    bind("tray_images", ui->checkBoxTrayImages, true);
    bind("tray_tab", ui->comboBoxMenuTab->lineEdit(), "");

    /* other options */
    bind("command_history_size", 100);
#ifdef COPYQ_WS_X11
    /* X11 clipboard selection monitoring and synchronization */
    bind("check_selection", ui->checkBoxSel, false);
    bind("copy_clipboard", ui->checkBoxCopyClip, false);
    bind("copy_selection", ui->checkBoxCopySel, false);
#else
    ui->checkBoxCopySel->hide();
    ui->checkBoxSel->hide();
    ui->checkBoxCopyClip->hide();
#endif

    // values of last submitted action
    bind("action_has_input", false);
    bind("action_has_output", false);
    bind("action_separator", "\\n");
    bind("action_output_tab", "");
}

void ConfigurationManager::bind(const char *optionKey, QCheckBox *obj, bool defaultValue)
{
    m_options[optionKey] = Option(defaultValue, "checked", obj);
}

void ConfigurationManager::bind(const char *optionKey, QSpinBox *obj, int defaultValue)
{
    m_options[optionKey] = Option(defaultValue, "value", obj);
}

void ConfigurationManager::bind(const char *optionKey, QLineEdit *obj, const QString &defaultValue)
{
    m_options[optionKey] = Option(defaultValue, "text", obj);
}

void ConfigurationManager::bind(const char *optionKey, QComboBox *obj, int defaultValue)
{
    m_options[optionKey] = Option(defaultValue, "currentIndex", obj);
}

void ConfigurationManager::bind(const char *optionKey, const QVariant &defaultValue)
{
    m_options[optionKey] = Option(defaultValue);
}

void ConfigurationManager::initTabComboBox(QComboBox *comboBox, const QStringList &tabs) const
{
    setComboBoxItems(comboBox, tabs);

    for (int i = 1; i < comboBox->count(); ++i) {
        const QString tabName = comboBox->itemText(i);
        const QIcon icon = getIconForTabName(tabName);
        comboBox->setItemIcon(i, icon);
    }
}

void ConfigurationManager::updateTabComboBoxes(const QStringList &tabs)
{
    initTabComboBox(ui->comboBoxClipboardTab, tabs);
    initTabComboBox(ui->comboBoxMenuTab, tabs);
}

void ConfigurationManager::updateTabComboBoxes()
{
    updateTabComboBoxes( tabs() );
}

QStringList ConfigurationManager::options() const
{
    QStringList options;
    foreach ( const QString &option, m_options.keys() ) {
        if ( m_options[option].value().canConvert(QVariant::String)
             && !optionToolTip(option).isEmpty() )
        {
            options.append(option);
        }
    }

    return options;
}

QString ConfigurationManager::optionToolTip(const QString &name) const
{
    return m_options[name].tooltip();
}

void ConfigurationManager::loadSettings()
{
    QSettings settings;

    settings.beginGroup("Options");
    foreach ( const QString &key, m_options.keys() ) {
        if ( settings.contains(key) ) {
            QVariant value = settings.value(key);
            if ( !value.isValid() || !m_options[key].setValue(value) )
                log( tr("Invalid value for option \"%1\"").arg(key), LogWarning );
        } else {
            m_options[key].reset();
        }
    }
    settings.endGroup();

    settings.beginGroup("Shortcuts");
    tabShortcuts()->loadShortcuts(settings);
    settings.endGroup();

    settings.beginGroup("Theme");
    tabAppearance()->loadTheme(settings);
    settings.endGroup();

    tabAppearance()->setEditor( AppConfig().option("editor").toString() );

    // load settings for each plugin
    settings.beginGroup("Plugins");
    foreach ( const ItemLoaderInterfacePtr &loader, itemFactory()->loaders() ) {
        settings.beginGroup(loader->id());

        QVariantMap s;
        foreach (const QString &name, settings.allKeys()) {
            s[name] = settings.value(name);
        }
        loader->loadSettings(s);
        itemFactory()->setLoaderEnabled( loader, settings.value("enabled", true).toBool() );

        settings.endGroup();
    }
    settings.endGroup();

    // load plugin priority
    const QStringList pluginPriority =
            settings.value("plugin_priority", QStringList()).toStringList();
    itemFactory()->setPluginPriority(pluginPriority);

    on_checkBoxMenuTabIsCurrent_stateChanged( ui->checkBoxMenuTabIsCurrent->checkState() );

    if (m_tabIcons.isEmpty()) {
        const int size = settings.beginReadArray("Tabs");
        for(int i = 0; i < size; ++i) {
            settings.setArrayIndex(i);
            m_tabIcons.insert(settings.value("name").toString(),
                              settings.value("icon").toString());
        }
        settings.endArray();
    }

    setTabs( tabs() );

    updateAutostart();
}

void ConfigurationManager::on_buttonBox_clicked(QAbstractButton* button)
{
    int answer;

    switch( ui->buttonBox->buttonRole(button) ) {
    case QDialogButtonBox::ApplyRole:
        apply();
        break;
    case QDialogButtonBox::AcceptRole:
        accept();
        break;
    case QDialogButtonBox::RejectRole:
        reject();
        break;
    case QDialogButtonBox::ResetRole:
        // ask before resetting values
        answer = QMessageBox::question(
                    this,
                    tr("Reset preferences?"),
                    tr("This action will reset all your preferences (in all tabs) to default values.<br /><br />"
                       "Do you really want to <strong>reset all preferences</strong>?"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            foreach ( const QString &key, m_options.keys() ) {
                m_options[key].reset();
            }
        }
        break;
    default:
        return;
    }
}

void ConfigurationManager::setTabs(const QStringList &tabs)
{
    Q_ASSERT( !tabs.contains(QString()) );
    Q_ASSERT( tabs.toSet().size() == tabs.size() );

    AppConfig().setOption("tabs", tabs);

    updateTabComboBoxes(tabs);

    foreach ( const QString &tabName, m_tabIcons.keys() ) {
        const QRegExp re(QRegExp::escape(tabName) + "(?:|/.*)$");
        if ( tabs.indexOf(re) == -1 )
            m_tabIcons.remove(tabName);
    }
}

QStringList ConfigurationManager::savedTabs() const
{
    QStringList tabs = this->tabs();
    tabs.removeAll(QString());

    const QString configPath = settingsDirectoryPath();

    QStringList files = QDir(configPath).entryList(QStringList("*_tab_*.dat"));
    files.append( QDir(configPath).entryList(QStringList("*_tab_*.dat.tmp")) );

    QRegExp re("_tab_([^.]*)");

    foreach (const QString fileName, files) {
        if ( fileName.contains(re) ) {
            const QString tabName =
                    getTextData(QByteArray::fromBase64(re.cap(1).toUtf8()));
            if ( !tabName.isEmpty() && !tabs.contains(tabName) )
                tabs.append(tabName);
        }
    }

    return tabs;
}

ConfigTabAppearance *ConfigurationManager::tabAppearance() const
{
    return ui->configTabAppearance;
}

ConfigTabShortcuts *ConfigurationManager::tabShortcuts() const
{
    return ui->configTabShortcuts;
}

QString ConfigurationManager::getIconNameForTabName(const QString &tabName) const
{
    return m_tabIcons.value(tabName);
}

void ConfigurationManager::setIconNameForTabName(const QString &name, const QString &icon)
{
    m_tabIcons[name] = icon;

    Settings settings;
    settings.beginWriteArray("Tabs");
    int i = 0;

    foreach ( const QString &tabName, m_tabIcons.keys() ) {
        settings.setArrayIndex(i++);
        settings.setValue("name", tabName);
        settings.setValue("icon", m_tabIcons[tabName]);
    }

    settings.endArray();

    updateTabComboBoxes();
}

QIcon ConfigurationManager::getIconForTabName(const QString &tabName) const
{
    const QString fileName = getIconNameForTabName(tabName);
    return fileName.isEmpty() ? QIcon() : iconFromFile(fileName);
}

void ConfigurationManager::setVisible(bool visible)
{
    QDialog::setVisible(visible);

    if (visible) {
        initTabIcons();
        initPluginWidgets();
        initLanguages();
        m_optionWidgetsLoaded = true;
    }
}

ConfigurationManager *ConfigurationManager::createInstance(QWidget *parent)
{
    Q_ASSERT(m_Instance == NULL);
    m_Instance = new ConfigurationManager(parent);
    m_Instance->loadSettings();
    WindowGeometryGuard::create(m_Instance);
    return m_Instance;
}

void ConfigurationManager::apply()
{
    Settings settings;

    settings.beginGroup("Options");
    foreach ( const QString &key, m_options.keys() ) {
        settings.setValue( key, m_options[key].value() );
    }
    settings.endGroup();

    // Save configuration without command line alternatives only if option widgets are initialized
    // (i.e. clicked OK or Apply in configuration dialog).
    if (m_optionWidgetsLoaded) {
        settings.beginGroup("Shortcuts");
        tabShortcuts()->saveShortcuts(*settings.settingsData());
        settings.endGroup();

        settings.beginGroup("Theme");
        tabAppearance()->saveTheme(*settings.settingsData());
        settings.endGroup();

        // save settings for each plugin
        if ( itemFactory()->hasLoaders() ) {
            settings.beginGroup("Plugins");
            for (int i = 0; i < ui->itemOrderListPlugins->itemCount(); ++i) {
                bool isPluginEnabled = ui->itemOrderListPlugins->isItemChecked(i);
                QWidget *w = ui->itemOrderListPlugins->widget(i);
                if (w) {
                    PluginWidget *pluginWidget = qobject_cast<PluginWidget *>(w);
                    pluginWidget->applySettings(settings.settingsData(), isPluginEnabled);
                    itemFactory()->setLoaderEnabled(pluginWidget->loader(), isPluginEnabled);
                }
            }
            settings.endGroup();

            // save plugin priority
            QStringList pluginPriority;
            for (int i = 0; i <  ui->itemOrderListPlugins->itemCount(); ++i)
                pluginPriority.append( ui->itemOrderListPlugins->itemLabel(i) );
            settings.setValue("plugin_priority", pluginPriority);
            itemFactory()->setPluginPriority(pluginPriority);
        }
    }

    tabAppearance()->setEditor( AppConfig().option("editor").toString() );

    setAutostartEnable();

    // Language changes after restart.
    const int newLocaleIndex = ui->comboBoxLanguage->currentIndex();
    const QString newLocaleName = ui->comboBoxLanguage->itemData(newLocaleIndex).toString();
    settings.setValue("Options/language", newLocaleName);
    const QLocale oldLocale;
    if (QLocale(newLocaleName).name() != oldLocale.name()) {
        QMessageBox::information( this, tr("Restart Required"),
                                  tr("Language will be changed after application is restarted.") );
    }

    emit configurationChanged();
}

void ConfigurationManager::done(int result)
{
    if (result == QDialog::Accepted) {
        apply();
    } else {
        loadSettings();
    }

    QDialog::done(result);

    if (!isVisible()) {
        m_optionWidgetsLoaded = false;
        if ( itemFactory()->hasLoaders() )
            ui->itemOrderListPlugins->clearItems();

        ui->comboBoxLanguage->clear();
    }
}

void ConfigurationManager::on_checkBoxMenuTabIsCurrent_stateChanged(int state)
{
    ui->comboBoxMenuTab->setEnabled(state == Qt::Unchecked);
}

void ConfigurationManager::on_spinBoxTrayItems_valueChanged(int value)
{
    ui->checkBoxPasteMenuItem->setEnabled(value > 0);
}

void setDefaultTabItemCounterStyle(QWidget *widget)
{
    QFont font = widget->font();
    const qreal pointSize = font.pointSizeF();
    if (pointSize > 0.0)
        font.setPointSizeF(pointSize * 0.7);
    else
        font.setPixelSize(font.pixelSize() * 0.7);
    widget->setFont(font);

    QPalette pal = widget->palette();
    const QPalette::ColorRole role = widget->foregroundRole();
    QColor color = pal.color(role);
    color.setAlpha( qMax(50, color.alpha() - 100) );
    color.setRed( qMin(255, color.red() + 120) );
    pal.setColor(role, color);
    widget->setPalette(pal);
}

void setComboBoxItems(QComboBox *comboBox, const QStringList &items)
{
    const QString text = comboBox->currentText();
    comboBox->clear();
    comboBox->addItem(QString());
    comboBox->addItems(items);
    comboBox->setEditText(text);

    const int currentIndex = comboBox->findText(text);
    if (currentIndex != -1)
        comboBox->setCurrentIndex(currentIndex);
}

QStringList ConfigurationManager::tabs() const
{
    return AppConfig().option("tabs").toStringList();
}
