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

#include "configurationmanager.h"
#include "ui_configurationmanager.h"

#include "common/command.h"
#include "common/common.h"
#include "common/option.h"
#include "gui/commandwidget.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/pluginwidget.h"
#include "item/clipboarditem.h"
#include "item/clipboardmodel.h"
#include "item/encrypt.h"
#include "item/itemdelegate.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "platform/platformnativeinterface.h"

#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
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

const QIcon iconLoadCommands(const QColor &color = QColor()) { return getIcon("document-open", IconFolderOpen, color, color); }
const QIcon iconSaveCommands(const QColor &color = QColor()) { return getIcon("document-save", IconSave, color, color); }

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

QString getCurrentLocale()
{
    QString currentLocale = QLocale().name();
    currentLocale.truncate(currentLocale.lastIndexOf('_'));
    return currentLocale;
}

void saveCommands(const ConfigurationManager::Commands &commands, QSettings *settings)
{
    settings->beginWriteArray("Commands");
    int i = 0;
    foreach (const Command &c, commands) {
        settings->setArrayIndex(i++);
        settings->setValue("Name", c.name);
        settings->setValue("Match", c.re.pattern());
        settings->setValue("Window", c.wndre.pattern());
        settings->setValue("MatchCommand", c.matchCmd);
        settings->setValue("Command", c.cmd);
        settings->setValue("Separator", c.sep);
        settings->setValue("Input", c.input);
        settings->setValue("Output", c.output);
        settings->setValue("Wait", c.wait);
        settings->setValue("Automatic", c.automatic);
        settings->setValue("InMenu", c.inMenu);
        settings->setValue("Transform", c.transform);
        settings->setValue("Remove", c.remove);
        settings->setValue("HideWindow", c.hideWindow);
        settings->setValue("Enable", c.enable);
        settings->setValue("Icon", c.icon);
        settings->setValue("Shortcut", c.shortcut);
        settings->setValue("Tab", c.tab);
        settings->setValue("OutputTab", c.outputTab);
    }
    settings->endArray();
}

ConfigurationManager::Commands loadCommands(QSettings *settings, bool onlyEnabled = false)
{
    ConfigurationManager::Commands commands;

    int size = settings->beginReadArray("Commands");

    for(int i=0; i<size; ++i) {
        settings->setArrayIndex(i);

        Command c;
        c.enable = settings->value("Enable").toBool();

        if (onlyEnabled && !c.enable)
            continue;

        c.name = settings->value("Name").toString();
        c.re   = QRegExp( settings->value("Match").toString() );
        c.wndre = QRegExp( settings->value("Window").toString() );
        c.matchCmd = settings->value("MatchCommand").toString();
        c.cmd = settings->value("Command").toString();
        c.sep = settings->value("Separator").toString();

        c.input = settings->value("Input").toString();
        if ( c.input == "false" || c.input == "true" )
            c.input = c.input == "true" ? QString(mimeText) : QString();

        c.output = settings->value("Output").toString();
        if ( c.output == "false" || c.output == "true" )
            c.output = c.output == "true" ? QString(mimeText) : QString();

        c.wait = settings->value("Wait").toBool();
        c.automatic = settings->value("Automatic").toBool();
        c.transform = settings->value("Transform").toBool();
        c.hideWindow = settings->value("HideWindow").toBool();
        c.icon = settings->value("Icon").toString();
        c.shortcut = settings->value("Shortcut").toString();
        c.tab = settings->value("Tab").toString();
        c.outputTab = settings->value("OutputTab").toString();

        // backwards compatibility with versions up to 1.8.2
        const QVariant inMenu = settings->value("InMenu");
        if ( inMenu.isValid() )
            c.inMenu = inMenu.toBool();
        else
            c.inMenu = !c.cmd.isEmpty() || !c.tab.isEmpty();

        if (settings->value("Ignore").toBool()) {
            c.remove = c.automatic = true;
            settings->remove("Ignore");
            settings->setValue("Remove", c.remove);
            settings->setValue("Automatic", c.automatic);
        } else {
            c.remove = settings->value("Remove").toBool();
        }

        commands.append(c);
    }

    settings->endArray();

    return commands;
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
    , m_datfilename()
    , m_options()
    , m_itemFactory(new ItemFactory(this))
    , m_iconFactory(new IconFactory)
    , m_optionWidgetsLoaded(false)
{
    ui->setupUi(this);
    setWindowIcon(iconFactory()->appIcon());

    if ( !itemFactory()->hasLoaders() )
        ui->tabItems->deleteLater();

    initOptions();

    /* datafile for items */
    m_datfilename = getConfigurationFilePath("_tab_");

    connect(this, SIGNAL(finished(int)), SLOT(onFinished(int)));
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
            loader = itemFactory()->initializeTab(&model);
            saveItems(model, loader);
        }
    }

    file.close();

    if (loader) {
        COPYQ_LOG( QString("Tab \"%1\": %2 items loaded").arg(tabName).arg(model.rowCount()) );
    } else {
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
        COPYQ_LOG( QString("Tab \"%1\": Items saved").arg(tabName) );

        // Overwrite previous file.
        QFile::remove(fileName);
        if ( !file.rename(fileName) )
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
    QFile::remove( itemFileName(tabName) );
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

bool ConfigurationManager::defaultCommand(int index, Command *c)
{
    static const QRegExp reURL("^(https?|ftps?|file)://");

    *c = Command();
    int i = 0;
    if (index == ++i) {
        c->name = tr("New command");
        c->input = c->output = "";
        c->wait = c->automatic = c->remove = false;
        c->sep = QString("\\n");
    } else if (index == ++i) {
        c->name = tr("Ignore items with no or single character");
        c->re   = QRegExp("^\\s*\\S?\\s*$");
        c->icon = QString(QChar(IconExclamationSign));
        c->remove = true;
        c->automatic = true;
    } else if (index == ++i) {
        c->name = tr("Open in &Browser");
        c->re   = reURL;
        c->icon = QString(QChar(IconGlobe));
        c->cmd  = "firefox %1";
        c->hideWindow = true;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Paste as Plain Text");
        c->input = mimeText;
        c->icon = QString(QChar(IconPaste));
        c->cmd  = "copyq eval \"copy(input()); paste()\"";
        c->hideWindow = true;
        c->inMenu = true;
        c->shortcut = tr("Shift+Return");
    } else if (index == ++i) {
        c->name = tr("Autoplay videos");
        c->re   = QRegExp("^http://.*\\.(mp4|avi|mkv|wmv|flv|ogv)$");
        c->icon = QString(QChar(IconPlayCircle));
        c->cmd  = "vlc %1";
        c->automatic = true;
        c->hideWindow = true;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Copy URL (web address) to other tab");
        c->re   = reURL;
        c->icon = QString(QChar(IconCopy));
        c->tab  = "&web";
    } else if (index == ++i) {
        c->name = tr("Run shell script");
        c->re   = QRegExp("^#!/bin/bash");
        c->icon = QString(QChar(IconTerminal));
        c->cmd  = "/bin/bash";
        c->input = mimeText;
        c->output = mimeText;
        c->outputTab = "&BASH";
        c->sep = "\\n";
        c->shortcut = tr("Ctrl+R");
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Create thumbnail (needs ImageMagick)");
        c->icon = QString(QChar(IconPicture));
        c->cmd  = "convert - -resize 92x92 png:-";
        c->input = "image/png";
        c->output = "image/png";
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Create QR Code from URL (needs qrencode)");
        c->re   = reURL;
        c->icon = QString(QChar(IconQRCode));
        c->cmd  = "qrencode -o - -t PNG -s 6";
        c->input = mimeText;
        c->output = "image/png";
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Label image");
        c->icon = QString(QChar(IconTag));
        c->cmd  = "base64 | perl -p -i -e 'BEGIN {print \"<img src=\\\\\"data:image/bmp;base64,\"} "
                  "END {print \"\\\\\" /><p>LABEL\"}'";
        c->input = "image/bmp";
        c->output = "text/html";
        c->wait = true;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Open URL");
        c->re   = reURL;
        c->icon = QString(QChar(IconEyeOpen));
        c->cmd  = "curl %1";
        c->input = mimeText;
        c->output = "text/html";
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Add to &TODO tab");
        c->icon = QString(QChar(IconShare));
        c->tab  = "TODO";
        c->input = mimeText;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Move to &TODO tab");
        c->icon = QString(QChar(IconShare));
        c->tab  = "TODO";
        c->input = mimeText;
        c->remove = true;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Ignore copied files");
        c->icon = QString(QChar(IconExclamationSign));
        c->input = mimeUriList;
        c->remove = true;
        c->automatic = true;
#if defined(COPYQ_WS_X11) || defined(Q_OS_WIN) || defined(Q_OS_MAC)
    } else if (index == ++i) {
        c->name = tr("Ignore *\"Password\"* window");
        c->wndre = QRegExp(tr("Password"));
        c->icon = QString(QChar(IconAsterisk));
        c->remove = true;
        c->automatic = true;
#endif
    } else if (index == ++i) {
        c->name = tr("Encrypt (needs GnuPG)");
        c->icon = QString(QChar(IconLock));
        c->input = mimeItems;
        c->output = mimeEncryptedData;
        c->inMenu = true;
        c->transform = true;
        c->cmd = getEncryptCommand() + " --encrypt";
        c->shortcut = tr("Ctrl+L");
    } else if (index == ++i) {
        c->name = tr("Decrypt");
        c->icon = QString(QChar(IconUnlock));
        c->input = mimeEncryptedData;
        c->output = mimeItems;
        c->inMenu = true;
        c->transform = true;
        c->cmd = getEncryptCommand() + " --decrypt";
        c->shortcut = tr("Ctrl+L");
    } else if (index == ++i) {
        c->name = tr("Decrypt and Copy");
        c->icon = QString(QChar(IconUnlockAlt));
        c->input = mimeEncryptedData;
        c->inMenu = true;
        c->cmd = getEncryptCommand() + " --decrypt | copyq copy " + mimeItems + " -";
        c->shortcut = tr("Ctrl+Shift+L");
    } else if (index == ++i) {
        c->name = tr("Move to Trash");
        c->icon = QString(QChar(IconTrash));
        c->inMenu = true;
        c->tab  = tr("(trash)");
        c->remove = true;
        c->shortcut = tr("Delete");
    } else {
        return false;
    }

    return true;
}

QString ConfigurationManager::itemFileName(const QString &id) const
{
    QString part( id.toLocal8Bit().toBase64() );
    part.replace( QChar('/'), QString('-') );
    return m_datfilename + part + QString(".dat");
}

bool ConfigurationManager::createItemDirectory()
{
    QDir settingsDir( QDir::cleanPath(m_datfilename + "/..") );
    if ( !settingsDir.mkpath(".") ) {
        log( tr("Cannot create directory for settings %1!")
             .arg(quoteString(settingsDir.path()) ),
             LogError );

        return false;
    }

    return true;
}

QString ConfigurationManager::getGeomentryOptionName(const QWidget *widget) const
{
    QString widgetName = widget->objectName();
    QString optionName = "Options/" + widgetName + "_geometry";

    // current screen number
    int n = widget->isVisible() && !widget->isMinimized()
            ? QApplication::desktop()->screenNumber(widget)
            : QApplication::desktop()->screenNumber(QCursor::pos());
    if (n > 0)
        optionName.append( QString("_screen_%1").arg(n) );

    return optionName;
}

void ConfigurationManager::updateIcons()
{
    iconFactory()->invalidateCache();
    iconFactory()->setUseSystemIcons(tabAppearance()->themeValue("use_system_icons").toBool());

    if ( itemFactory()->hasLoaders() )
        ui->itemOrderListPlugins->updateIcons();
    ui->itemOrderListCommands->updateIcons();

    static const QColor color = getDefaultIconColor(*ui->pushButtonLoadCommands, QPalette::Window);
    ui->pushButtonLoadCommands->setIcon(iconLoadCommands(color));
    ui->pushButtonSaveCommands->setIcon(iconSaveCommands(color));

    tabShortcuts()->updateIcons();
}

void ConfigurationManager::initTabIcons()
{
    QTabWidget *tw = ui->tabWidget;
    if ( !tw->tabIcon(0).isNull() )
        return;

    IconFactory *f = iconFactory();

    static const QColor color = getDefaultIconColor<QTabBar>(QPalette::Window);

    tw->setTabIcon( tw->indexOf(ui->tabGeneral), f->createPixmap(IconWrench, color) );
    tw->setTabIcon( tw->indexOf(ui->tabHistory), f->createPixmap(IconListAlt, color) );
    tw->setTabIcon( tw->indexOf(ui->tabItems), f->createPixmap(IconThList, color) );
    tw->setTabIcon( tw->indexOf(ui->tabTray), f->createPixmap(IconInbox, color) );
    tw->setTabIcon( tw->indexOf(ui->tabCommands), f->createPixmap(IconCogs, color) );
    tw->setTabIcon( tw->indexOf(ui->tabShortcuts), f->createPixmap(IconKeyboard, color) );
    tw->setTabIcon( tw->indexOf(ui->tabAppearance), f->createPixmap(IconPicture, color) );
}

void ConfigurationManager::initPluginWidgets()
{
    if (!itemFactory()->hasLoaders())
        return;

    ui->itemOrderListPlugins->clearItems();

    static const QColor color = getDefaultIconColor<QListWidget>(QPalette::Base);
    static const QColor activeColor = getDefaultIconColor<QListWidget>(QPalette::Highlight);

    foreach ( const ItemLoaderInterfacePtr &loader, itemFactory()->loaders() ) {
        PluginWidget *pluginWidget = new PluginWidget(loader, this);

        QIcon icon;
        const QVariant maybeIcon = loader->icon();
        if ( maybeIcon.canConvert(QVariant::UInt) )
            icon = getIcon( QString(), maybeIcon.toUInt(), color, activeColor );
        else if ( maybeIcon.canConvert(QVariant::Icon) )
            icon = maybeIcon.value<QIcon>();

        ui->itemOrderListPlugins->appendItem(
                    loader->name(), itemFactory()->isLoaderEnabled(loader), icon, pluginWidget );
    }
}

void ConfigurationManager::initCommandWidgets()
{
    ui->itemOrderListCommands->clearItems();

    foreach ( const Command &command, commands(false) )
        addCommand(command, false);

    if ( !ui->itemOrderListCommands->hasAddMenu() ) {
        Command cmd;
        int i = 0;
        QMenu *menu = new QMenu(this);
        ui->itemOrderListCommands->setAddMenu(menu);
        while ( defaultCommand(++i, &cmd) ) {
            menu->addAction( iconFactory()->iconFromFile(cmd.icon), cmd.name.remove('&') )
                    ->setProperty("COMMAND", i);
        }
    }
}

void ConfigurationManager::initLanguages()
{
    ui->comboBoxLanguage->addItem("English");
    ui->comboBoxLanguage->setItemData(0, "en");

    const QString currentLocale = getCurrentLocale();

    foreach ( const QString &item, QDir(":/translations").entryList() ) {
        const int i = item.indexOf('_');
        const QString locale = item.mid(i + 1, item.lastIndexOf('.') - i - 1);
        const QString language = QLocale(locale).nativeLanguageName();

        const int index = ui->comboBoxLanguage->count();
        ui->comboBoxLanguage->addItem(language);
        ui->comboBoxLanguage->setItemData(index, locale);

        if (locale == currentLocale)
            ui->comboBoxLanguage->setCurrentIndex(index);
    }
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
    platform->setAutostartEnabled( value("autostart").toBool() );
}

void ConfigurationManager::initOptions()
{
    /* general options */
    bind("autostart", ui->checkBoxAutostart, false);
    bind("maxitems", ui->spinBoxItems, 200);
    bind("expire_tab", ui->spinBoxExpireTab, 0);
    bind("editor", ui->lineEditEditor, DEFAULT_EDITOR);
    bind("item_popup_interval", ui->spinBoxItemPopupInterval, 0);
    bind("notification_position", ui->comboBoxNotificationPosition, 3);
    bind("clipboard_notification_lines", ui->spinBoxClipboardNotificationLines, 0);
    bind("edit_ctrl_return", ui->checkBoxEditCtrlReturn, true);
    bind("move", ui->checkBoxMove, true);
    bind("check_clipboard", ui->checkBoxClip, true);
    bind("confirm_exit", ui->checkBoxConfirmExit, true);
    bind("vi", ui->checkBoxViMode, false);
    bind("always_on_top", ui->checkBoxAlwaysOnTop, false);
    bind("transparency_focused", ui->spinBoxTransparencyFocused, 0);
    bind("transparency", ui->spinBoxTransparencyUnfocused, 0);
    bind("hide_tabs", ui->checkBoxHideTabs, false);
    bind("hide_toolbar", ui->checkBoxHideToolbar, false);
    bind("disable_tray", ui->checkBoxDisableTray, false);
    bind("tab_tree", ui->checkBoxTabTree, false);
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

    // Tooltip to show on command line.
    ui->comboBoxMenuTab->lineEdit()->setToolTip( ui->comboBoxMenuTab->toolTip() );

    /* other options */
    bind("command_history_size", 100);
    bind("_last_hash", 0);
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

void ConfigurationManager::bind(const char *optionKey, QLineEdit *obj, const char *defaultValue)
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

QIcon ConfigurationManager::getCommandIcon(const QString &iconString) const
{
    static const QColor color = getDefaultIconColor(*ui->itemOrderListCommands, QPalette::Base);
    IconFactory *iconFactory = ConfigurationManager::instance()->iconFactory();
    return iconFactory->iconFromFile(iconString, color);
}

bool ConfigurationManager::loadGeometry(QWidget *widget) const
{
    QSettings settings;
    QVariant option = settings.value( getGeomentryOptionName(widget) );
    return widget->restoreGeometry(option.toByteArray());
}

void ConfigurationManager::saveGeometry(const QWidget *widget)
{
    if (widget->isMinimized())
        return;

    QSettings settings;
    settings.setValue( getGeomentryOptionName(widget), widget->saveGeometry() );
}

QVariant ConfigurationManager::value(const QString &name) const
{
    if ( m_options.contains(name) )
        return m_options[name].value();
    return QSettings().value("Options/" + name);
}

void ConfigurationManager::setValue(const QString &name, const QVariant &value)
{
    if ( m_options.contains(name) ) {
        if ( m_options[name].value() == value )
            return;

        m_options[name].setValue(value);

        // Save the retrieved option value since option widget can modify it (e.g. int in range).
        QSettings().setValue( "Options/" + name, m_options[name].value() );

        emit configurationChanged();
    } else {
        QSettings().setValue("Options/" + name, value);
    }
}

QStringList ConfigurationManager::options() const
{
    QStringList options;
    foreach ( const QString &option, m_options.keys() ) {
        if ( value(option).canConvert(QVariant::String) &&
             !optionToolTip(option).isEmpty() )
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
            if ( value.isValid() )
                m_options[key].setValue(value);
        }
    }
    tabShortcuts()->loadGlobalShortcuts(settings);
    settings.endGroup();

    settings.beginGroup("Shortcuts");
    tabShortcuts()->loadShortcuts(settings);
    settings.endGroup();

    settings.beginGroup("Theme");
    tabAppearance()->loadTheme(settings);
    settings.endGroup();

    updateIcons();

    tabAppearance()->setEditor( value("editor").toString() );

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

    updateAutostart();
}

ConfigurationManager::Commands ConfigurationManager::commands(bool onlyEnabled, bool onlySaved) const
{
    Commands commands;

    if (!onlySaved && m_optionWidgetsLoaded) {
        for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i) {
            QWidget *w = ui->itemOrderListCommands->itemWidget(i);
            CommandWidget *cmdWidget = qobject_cast<CommandWidget *>(w);
            Command c = cmdWidget->command();
            c.enable = ui->itemOrderListCommands->isItemChecked(i);

            if (!onlyEnabled || c.enable)
                commands.append(c);
        }
    } else {
        QSettings settings;
        commands = loadCommands(&settings, onlyEnabled);
    }

    return commands;
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

void ConfigurationManager::addCommand(const Command &c, bool save)
{
    CommandWidget *cmdWidget = new CommandWidget(this);

    cmdWidget->setCommand(c);

    QStringList formats = itemFactory()->formatsToSave();
    formats.prepend(mimeText);
    formats.prepend(QString());
    formats.removeDuplicates();
    cmdWidget->setFormats(formats);

    cmdWidget->setTabs( value("tabs").toStringList() );

    connect( cmdWidget, SIGNAL(iconChanged(QString)),
             this, SLOT(onCurrentCommandWidgetIconChanged(QString)) );
    connect( cmdWidget, SIGNAL(nameChanged(QString)),
             this, SLOT(onCurrentCommandWidgetNameChanged(QString)) );

    ui->itemOrderListCommands->appendItem(c.name, c.enable,
                                          getCommandIcon(cmdWidget->currentIcon()), cmdWidget);

    if (save) {
        Commands cmds = commands(false, true);
        cmds.append(c);
        QSettings settings;
        saveCommands(cmds, &settings);
    }
}

void ConfigurationManager::setTabs(const QStringList &tabs)
{
    Q_ASSERT( !tabs.contains(QString()) );
    Q_ASSERT( tabs.toSet().size() == tabs.size() );

    setValue("tabs", tabs);

    QString text = ui->comboBoxMenuTab->currentText();
    ui->comboBoxMenuTab->clear();
    ui->comboBoxMenuTab->addItem(QString());
    ui->comboBoxMenuTab->addItems(tabs);
    ui->comboBoxMenuTab->setEditText(text);
}

QStringList ConfigurationManager::savedTabs() const
{
    QStringList tabs = value("tabs").toStringList();
    const QString configPath = QDir::cleanPath(m_datfilename + "/..");

    QStringList files = QDir(configPath).entryList(QStringList("*_tab_*.dat"));
    files.append( QDir(configPath).entryList(QStringList("*_tab_*.dat.tmp")) );

    QRegExp re("_tab_([^.]*)");

    foreach (const QString fileName, files) {
        if ( fileName.contains(re) ) {
            const QString tabName =
                    QString::fromLocal8Bit(QByteArray::fromBase64(re.cap(1).toLocal8Bit()));
            if ( !tabs.contains(tabName) )
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

void ConfigurationManager::setVisible(bool visible)
{
    QDialog::setVisible(visible);

    if (visible) {
        initTabIcons();
        initPluginWidgets();
        initCommandWidgets();
        initLanguages();
        m_optionWidgetsLoaded = true;
    }
}

void ConfigurationManager::createInstance(QWidget *parent)
{
    Q_ASSERT(m_Instance == NULL);
    m_Instance = new ConfigurationManager(parent);
    m_Instance->loadSettings();
    m_Instance->loadGeometry(m_Instance);
}

void ConfigurationManager::apply()
{
    QSettings settings;

    settings.beginGroup("Options");
    foreach ( const QString &key, m_options.keys() ) {
        settings.setValue( key, m_options[key].value() );
    }
    settings.endGroup();

    // Save configuration without command line alternatives only if option widgets are initialized
    // (i.e. clicked OK or Apply in configuration dialog).
    if (m_optionWidgetsLoaded) {
        saveCommands(commands(false, false), &settings);

        settings.beginGroup("Shortcuts");
        tabShortcuts()->saveShortcuts(settings);
        settings.endGroup();

        settings.beginGroup("Options");
        tabShortcuts()->saveGlobalShortcuts(settings);
        settings.endGroup();

        settings.beginGroup("Theme");
        tabAppearance()->saveTheme(settings);
        settings.endGroup();

        // save settings for each plugin
        if ( itemFactory()->hasLoaders() ) {
            settings.beginGroup("Plugins");
            for (int i = 0; i < ui->itemOrderListPlugins->itemCount(); ++i) {
                QWidget *w = ui->itemOrderListPlugins->itemWidget(i);
                PluginWidget *pluginWidget = qobject_cast<PluginWidget *>(w);
                ItemLoaderInterfacePtr loader = pluginWidget->loader();

                settings.beginGroup(loader->id());

                QVariantMap s = loader->applySettings();
                foreach (const QString &name, s.keys()) {
                    settings.setValue(name, s[name]);
                }

                bool enabled = ui->itemOrderListPlugins->isItemChecked(i);
                itemFactory()->setLoaderEnabled(loader, enabled);
                settings.setValue("enabled", enabled);

                settings.endGroup();
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

    tabAppearance()->setEditor( value("editor").toString() );

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

void ConfigurationManager::on_itemOrderListCommands_addButtonClicked(QAction *action)
{
    Command cmd;
    if ( !defaultCommand(action->property("COMMAND").toInt(), &cmd) )
        return;
    addCommand(cmd, false);
    ui->itemOrderListCommands->setCurrentItem( ui->itemOrderListCommands->itemCount() - 1 );
}

void ConfigurationManager::on_itemOrderListCommands_itemSelectionChanged()
{
    bool saveEnabled = !ui->itemOrderListCommands->selectedRows().isEmpty();
    ui->pushButtonSaveCommands->setEnabled(saveEnabled);
}

void ConfigurationManager::on_pushButtonLoadCommands_clicked()
{
    const QStringList fileNames =
            QFileDialog::getOpenFileNames(this, tr("Open Files with Commands"),
                                          QString(), tr("Commands (*.ini);; CopyQ Configuration (copyq.conf copyq-*.conf)"));

    foreach (const QString &fileName, fileNames) {
        QSettings settings(fileName, QSettings::IniFormat);
        QList<int> rowsToSelect;

        foreach ( const Command &command, loadCommands(&settings) ) {
            rowsToSelect.append(ui->itemOrderListCommands->rowCount());
            addCommand(command, false);
        }

        ui->itemOrderListCommands->setSelectedRows(rowsToSelect);
    }
}

void ConfigurationManager::on_pushButtonSaveCommands_clicked()
{
    QString fileName =
            QFileDialog::getSaveFileName(this, tr("Save Selected Commands"),
                                          QString(), tr("Commands (*.ini)"));
    if ( !fileName.isEmpty() ) {
        if ( !fileName.endsWith(".ini") )
            fileName.append(".ini");
        QList<int> rows = ui->itemOrderListCommands->selectedRows();
        Commands allCommands = commands(false, false);
        Commands commandsToSave;
        foreach (int row, rows) {
            Q_ASSERT(row < allCommands.size());
            commandsToSave.append(allCommands.value(row));
        }

        QSettings settings(fileName, QSettings::IniFormat);
        saveCommands(commandsToSave, &settings);
    }
}

void ConfigurationManager::onFinished(int result)
{
    saveGeometry(this);
    if (result == QDialog::Accepted) {
        apply();
    } else {
        loadSettings();
    }

    m_optionWidgetsLoaded = false;
    if ( itemFactory()->hasLoaders() )
        ui->itemOrderListPlugins->clearItems();
    ui->itemOrderListCommands->clearItems();

    ui->comboBoxLanguage->clear();
}

void ConfigurationManager::on_checkBoxMenuTabIsCurrent_stateChanged(int state)
{
    ui->comboBoxMenuTab->setEnabled(state == Qt::Unchecked);
}

void ConfigurationManager::onCurrentCommandWidgetIconChanged(const QString &iconString)
{
    ui->itemOrderListCommands->setCurrentItemIcon( getCommandIcon(iconString) );
}

void ConfigurationManager::onCurrentCommandWidgetNameChanged(const QString &name)
{
    ui->itemOrderListCommands->setCurrentItemLabel(name);
}

void ConfigurationManager::on_spinBoxTrayItems_valueChanged(int value)
{
    ui->checkBoxPasteMenuItem->setEnabled(value > 0);
}

const QIcon &getIconFromResources(const QString &iconName)
{
    return ConfigurationManager::instance()->iconFactory()->getIcon(iconName);
}

QIcon getIconFromResources(const QString &iconName, const QColor &color, const QColor &activeColor)
{
    Q_ASSERT( !iconName.isEmpty() );
    return ConfigurationManager::instance()->iconFactory()->getIcon(iconName, color, activeColor);
}

QIcon getIcon(const QString &themeName, ushort iconId)
{
    return ConfigurationManager::instance()->iconFactory()->getIcon(themeName, iconId);
}

QIcon getIcon(const QString &themeName, ushort iconId, const QColor &color, const QColor &activeColor)
{
    return ConfigurationManager::instance()->iconFactory()->getIcon(themeName, iconId, color, activeColor);
}
