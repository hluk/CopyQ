/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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
#include "common/config.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/option.h"
#include "common/settings.h"
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
#include <QMimeData>
#include <QSettings>
#include <QTemporaryFile>
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

void saveValue(const char *key, const QRegExp &re, QSettings *settings)
{
    settings->setValue(key, re.pattern());
}

void saveValue(const char *key, const QVariant &value, QSettings *settings)
{
    settings->setValue(key, value);
}

/// Save only modified command properties.
template <typename Member>
void saveNewValue(const char *key, const Command &command, const Member &member, QSettings *settings)
{
    if (command.*member != Command().*member)
        saveValue(key, command.*member, settings);
}

void saveCommand(const Command &c, QSettings *settings)
{
    saveNewValue("Name", c, &Command::name, settings);
    saveNewValue("Match", c, &Command::re, settings);
    saveNewValue("Window", c, &Command::wndre, settings);
    saveNewValue("MatchCommand", c, &Command::matchCmd, settings);
    saveNewValue("Command", c, &Command::cmd, settings);
    saveNewValue("Input", c, &Command::input, settings);
    saveNewValue("Output", c, &Command::output, settings);

    // Separator for new command is set to '\n' for convenience.
    // But this value shouldn't be saved if output format is not set.
    if ( c.sep != "\\n" || !c.output.isEmpty() )
        saveNewValue("Separator", c, &Command::sep, settings);

    saveNewValue("Wait", c, &Command::wait, settings);
    saveNewValue("Automatic", c, &Command::automatic, settings);
    saveNewValue("InMenu", c, &Command::inMenu, settings);
    saveNewValue("Transform", c, &Command::transform, settings);
    saveNewValue("Remove", c, &Command::remove, settings);
    saveNewValue("HideWindow", c, &Command::hideWindow, settings);
    saveNewValue("Enable", c, &Command::enable, settings);
    saveNewValue("Icon", c, &Command::icon, settings);
    saveNewValue("Shortcut", c, &Command::shortcuts, settings);
    saveNewValue("GlobalShortcut", c, &Command::globalShortcuts, settings);
    saveNewValue("Tab", c, &Command::tab, settings);
    saveNewValue("OutputTab", c, &Command::outputTab, settings);
}

void saveCommands(const ConfigurationManager::Commands &commands, QSettings *settings)
{
    settings->remove("Commands");
    settings->remove("Command");

    if (commands.size() == 1) {
        settings->beginGroup("Command");
        saveCommand(commands[0], settings);
        settings->endGroup();
    } else {
        settings->beginWriteArray("Commands");
        int i = 0;
        foreach (const Command &c, commands) {
            settings->setArrayIndex(i++);
            saveCommand(c, settings);
        }
        settings->endArray();
    }
}

void loadCommand(QSettings *settings, bool onlyEnabled, ConfigurationManager::Commands *commands)
{
    Command c;
    c.enable = settings->value("Enable", true).toBool();

    if (onlyEnabled && !c.enable)
        return;

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
    c.shortcuts = settings->value("Shortcut").toStringList();
    c.globalShortcuts = settings->value("GlobalShortcut").toStringList();
    c.tab = settings->value("Tab").toString();
    c.outputTab = settings->value("OutputTab").toString();
    c.inMenu = settings->value("InMenu").toBool();

    if (settings->value("Ignore").toBool()) {
        c.remove = c.automatic = true;
        settings->remove("Ignore");
        settings->setValue("Remove", c.remove);
        settings->setValue("Automatic", c.automatic);
    } else {
        c.remove = settings->value("Remove").toBool();
    }

    commands->append(c);
}

ConfigurationManager::Commands loadCommands(QSettings *settings, bool onlyEnabled = false)
{
    ConfigurationManager::Commands commands;

    const QStringList groups = settings->childGroups();

    if ( groups.contains("Command") ) {
        settings->beginGroup("Command");
        loadCommand(settings, onlyEnabled, &commands);
        settings->endGroup();
    }

    int size = settings->beginReadArray("Commands");

    for(int i=0; i<size; ++i) {
        settings->setArrayIndex(i);
        loadCommand(settings, onlyEnabled, &commands);
    }

    settings->endArray();

    return commands;
}

#ifndef NO_GLOBAL_SHORTCUTS
enum GlobalAction {
    GlobalActionToggleMainWindow,
    GlobalActionShowTray,
    GlobalActionEditClipboard,
    GlobalActionEditFirstItem,
    GlobalActionCopySecondItem,
    GlobalActionShowActionDialog,
    GlobalActionCreateItem,
    GlobalActionCopyNextItem,
    GlobalActionCopyPreviousItem,
    GlobalActionPasteAsPlainText,
    GlobalActionDisableClipboardStoring,
    GlobalActionEnableClipboardStoring,
    GlobalActionPasteAndCopyNext,
    GlobalActionPasteAndCopyPrevious
};

void createGlobalShortcut(const QString &name, const QString &script, IconId icon,
                          const QStringList &s, Command *c)
{
    c->name = name;
    c->cmd = "copyq: " + script;
    c->icon = QString(QChar(icon));
    c->globalShortcuts = s.isEmpty()
            ? QStringList(ConfigurationManager::tr("Ctrl+Shift+1", "Global shortcut for some predefined commands"))
            : s;
}

void createGlobalShortcut(GlobalAction id, Command *c, const QStringList &s = QStringList())
{
    if (id == GlobalActionToggleMainWindow)
        createGlobalShortcut( ConfigurationManager::tr("Show/hide main window"), "toggle()", IconListAlt, s, c );
    else if (id == GlobalActionShowTray)
        createGlobalShortcut( ConfigurationManager::tr("Show the tray menu"), "menu()", IconInbox, s, c );
    else if (id == GlobalActionEditClipboard)
        createGlobalShortcut( ConfigurationManager::tr("Edit clipboard"), "edit(-1)", IconEdit, s, c );
    else if (id == GlobalActionEditFirstItem)
        createGlobalShortcut( ConfigurationManager::tr("Edit first item"), "edit(0)", IconEdit, s, c );
    else if (id == GlobalActionCopySecondItem)
        createGlobalShortcut( ConfigurationManager::tr("Copy second item"), "select(1)", IconCopy, s, c );
    else if (id == GlobalActionShowActionDialog)
        createGlobalShortcut( ConfigurationManager::tr("Show action dialog"), "action()", IconCog, s, c );
    else if (id == GlobalActionCreateItem)
        createGlobalShortcut( ConfigurationManager::tr("Create new item"), "edit()", IconAsterisk, s, c );
    else if (id == GlobalActionCopyNextItem)
        createGlobalShortcut( ConfigurationManager::tr("Copy next item"), "next()", IconArrowDown, s, c );
    else if (id == GlobalActionCopyPreviousItem)
        createGlobalShortcut( ConfigurationManager::tr("Copy previous item"), "previous()", IconArrowUp, s, c );
    else if (id == GlobalActionPasteAsPlainText)
        createGlobalShortcut( ConfigurationManager::tr("Paste clipboard as plain text"), "copy(clipboard()); paste()", IconPaste, s, c );
    else if (id == GlobalActionDisableClipboardStoring)
        createGlobalShortcut( ConfigurationManager::tr("Disable clipboard storing"), "disable()", IconEyeSlash, s, c );
    else if (id == GlobalActionEnableClipboardStoring)
        createGlobalShortcut( ConfigurationManager::tr("Enable clipboard storing"), "enable()", IconEyeOpen, s, c );
    else if (id == GlobalActionPasteAndCopyNext)
        createGlobalShortcut( ConfigurationManager::tr("Paste and copy next"), "paste(); next()", IconArrowCircleODown, s, c );
    else if (id == GlobalActionPasteAndCopyPrevious)
        createGlobalShortcut( ConfigurationManager::tr("Paste and copy previous"), "paste(); previous()", IconArrowCircleOUp, s, c );
    else
        Q_ASSERT(false);
}

void restoreGlobalAction(GlobalAction id, const QString &key, QSettings *settings, ConfigurationManager::Commands *commands)
{
    const QStringList shortcuts = settings->value(key).toStringList();
    if ( !shortcuts.isEmpty() ) {
        Command c;
        createGlobalShortcut(id, &c, shortcuts);
        commands->append(c);
        settings->remove(key);
    }
}

/**
 * Load deprecated global shortcut settings.
 * Removes the settings and stores as commands.
 */
void restoreGlobalActions()
{
    ConfigurationManager::Commands commands;
    QSettings settings;

    settings.beginGroup("Options");
    restoreGlobalAction( GlobalActionToggleMainWindow, "toggle_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionShowTray, "menu_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionEditClipboard, "edit_clipboard_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionEditFirstItem, "edit_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionCopySecondItem, "second_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionShowActionDialog, "show_action_dialog", &settings, &commands );
    restoreGlobalAction( GlobalActionCreateItem, "new_item_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionCopyNextItem, "next_item_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionCopyPreviousItem, "previous_item_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionPasteAsPlainText, "paste_as_plain_text", &settings, &commands );
    restoreGlobalAction( GlobalActionDisableClipboardStoring, "disable_monitoring_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionEnableClipboardStoring, "enable_monitoring_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionPasteAndCopyNext, "paste_and_copy_next_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionPasteAndCopyPrevious, "paste_and_copy_previous_shortcut", &settings, &commands );
    settings.endGroup();

    if ( !commands.isEmpty() ) {
        commands.append( loadCommands(&settings, false) );
        saveCommands(commands, &settings);
    }
}

#endif

} // namespace

// singleton
ConfigurationManager *ConfigurationManager::m_Instance = 0;

ConfigurationManager *ConfigurationManager::instance()
{
    Q_ASSERT(m_Instance != NULL);
    return m_Instance;
}

ConfigurationManager::ConfigurationManager()
    : QDialog()
    , ui(new Ui::ConfigurationManager)
    , m_options()
    , m_tabIcons()
    , m_itemFactory(new ItemFactory(this))
    , m_iconFactory(new IconFactory)
    , m_optionWidgetsLoaded(false)
{
    ui->setupUi(this);
    setWindowIcon(iconFactory()->appIcon());
    // Don't use darker background in QScrollArea widgets.
    setStyleSheet("QScrollArea { background: transparent; border: none; }"
                  "QScrollArea > QWidget > QWidget { background: transparent; }");

    if ( !itemFactory()->hasLoaders() )
        ui->tabItems->deleteLater();

    initOptions();

    connect(this, SIGNAL(finished(int)), SLOT(onFinished(int)));

    QAction *act = new QAction(ui->itemOrderListCommands);
    ui->itemOrderListCommands->addAction(act);
    act->setShortcut(QKeySequence::Paste);
    connect(act, SIGNAL(triggered()), SLOT(tryPasteCommandFromClipboard()));

    act = new QAction(ui->itemOrderListCommands);
    ui->itemOrderListCommands->addAction(act);
    act->setShortcut(QKeySequence::Copy);
    connect(act, SIGNAL(triggered()), SLOT(copySelectedCommandsToClipboard()));
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
        c->cmd  = "copyq:\ncopy(input())\npaste()";
        c->hideWindow = true;
        c->inMenu = true;
        c->shortcuts.append( tr("Shift+Return") );
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
        c->automatic = true;
    } else if (index == ++i) {
        c->name = tr("Run shell script");
        c->re   = QRegExp("^#!/bin/bash");
        c->icon = QString(QChar(IconTerminal));
        c->cmd  = "/bin/bash";
        c->input = mimeText;
        c->output = mimeText;
        c->outputTab = "&BASH";
        c->sep = "\\n";
        c->shortcuts.append( tr("Ctrl+R") );
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
        c->cmd  = "base64 | perl -p -i -e:\n"
                  "BEGIN {\n"
                  "    print '<img src=\"data:image/bmp;base64,'\n"
                  "}\n"
                  "END {\n"
                  "    print '\" /><p>LABEL'\n"
                  "}";
        c->input = "image/bmp";
        c->output = "text/html";
        c->wait = true;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Open URL");
        c->re   = reURL;
        c->icon = QString(QChar(IconLink));
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
        c->cmd = "copyq ignore";
#endif
    } else if (index == ++i) {
        c->name = tr("Encrypt (needs GnuPG)");
        c->icon = QString(QChar(IconLock));
        c->input = mimeItems;
        c->output = mimeEncryptedData;
        c->inMenu = true;
        c->transform = true;
        c->cmd = getEncryptCommand() + " --encrypt";
        c->shortcuts.append( tr("Ctrl+L") );
    } else if (index == ++i) {
        c->name = tr("Decrypt");
        c->icon = QString(QChar(IconUnlock));
        c->input = mimeEncryptedData;
        c->output = mimeItems;
        c->inMenu = true;
        c->transform = true;
        c->cmd = getEncryptCommand() + " --decrypt";
        c->shortcuts.append( tr("Ctrl+L") );
    } else if (index == ++i) {
        c->name = tr("Decrypt and Copy");
        c->icon = QString(QChar(IconUnlockAlt));
        c->input = mimeEncryptedData;
        c->inMenu = true;
        c->cmd = getEncryptCommand() + " --decrypt | copyq copy " + mimeItems + " -";
        c->shortcuts.append( tr("Ctrl+Shift+L") );
    } else if (index == ++i) {
        c->name = tr("Move to Trash");
        c->icon = QString(QChar(IconTrash));
        c->inMenu = true;
        c->tab  = tr("(trash)");
        c->remove = true;
        c->shortcuts.append( shortcutToRemove() );
#ifndef NO_GLOBAL_SHORTCUTS
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionToggleMainWindow, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionShowTray, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionEditClipboard, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionEditFirstItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionCopySecondItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionShowActionDialog, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionCreateItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionCopyNextItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionCopyPreviousItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionPasteAsPlainText, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionDisableClipboardStoring, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionEnableClipboardStoring, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionPasteAndCopyNext, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionPasteAndCopyPrevious, c);
#endif
    } else {
        return false;
    }

    return true;
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

    for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i) {
        QWidget *w = ui->itemOrderListCommands->itemWidget(i);
        CommandWidget *cmdWidget = qobject_cast<CommandWidget *>(w);
        cmdWidget->updateIcons();
    }
}

void ConfigurationManager::registerWindowGeometry(QWidget *window)
{
    window->installEventFilter(this);
    bool openOnCurrentScreen = value("open_windows_on_current_screen").toBool();
    restoreWindowGeometry(window, openOnCurrentScreen);
}

bool ConfigurationManager::eventFilter(QObject *object, QEvent *event)
{
    // Restore and save geometry of widgets passed to registerWindowGeometry().
    if ( event->type() == QEvent::WindowDeactivate
         || event->type() == QEvent::WindowActivate
         || (event->type() == QEvent::Paint && object->property("CopyQ_restore_geometry").toBool()) )
    {
        // Restore window geometry later because some window managers move new windows a bit after
        // adding window decorations.
        object->setProperty("CopyQ_restore_geometry", event->type() == QEvent::WindowActivate);

        bool save = event->type() == QEvent::WindowDeactivate;
        QWidget *w = qobject_cast<QWidget*>(object);
        bool openOnCurrentScreen = value("open_windows_on_current_screen").toBool();
        if (save)
            saveWindowGeometry(w, openOnCurrentScreen);
        else
            restoreWindowGeometry(w, openOnCurrentScreen);
    }

    return false;
}

QByteArray ConfigurationManager::mainWindowState(const QString &mainWindowObjectName)
{
    const QString optionName = "Options/" + mainWindowObjectName + "_state";
    return geometryOptionValue(optionName);
}

void ConfigurationManager::saveMainWindowState(const QString &mainWindowObjectName, const QByteArray &state)
{
    const QString optionName = "Options/" + mainWindowObjectName + "_state";
    QSettings geometrySettings( getConfigurationFilePath("_geometry.ini"), QSettings::IniFormat );
    geometrySettings.setValue(optionName, state);
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
    tw->setTabIcon( tw->indexOf(ui->tabNotifications), f->createPixmap(IconInfoSign, color) );
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
        addCommandWithoutSave(command);

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

    const QString currentLocale = QLocale().name();
    bool currentLocaleFound = false; // otherwise not found or partial match ("uk" partially matches locale "uk_UA")
    QSet<QString> languages;

    foreach ( const QString &path, qApp->property("CopyQ_translation_directories").toStringList() ) {
        foreach ( const QString &item, QDir(path).entryList(QStringList("copyq_*.qm")) ) {
            const int i = item.indexOf('_');
            const QString locale = item.mid(i + 1, item.lastIndexOf('.') - i - 1);
            const QString language = QLocale(locale).nativeLanguageName();

            if ( !languages.contains(language) ) {
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
    platform->setAutostartEnabled( value("autostart").toBool() );
}

void ConfigurationManager::initOptions()
{
    /* general options */
    bind("autostart", ui->checkBoxAutostart, false);
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
    bind("always_on_top", ui->checkBoxAlwaysOnTop, false);
    bind("open_windows_on_current_screen", ui->checkBoxOpenWindowsOnCurrentScreen, true);
    bind("transparency_focused", ui->spinBoxTransparencyFocused, 0);
    bind("transparency", ui->spinBoxTransparencyUnfocused, 0);
    bind("hide_tabs", ui->checkBoxHideTabs, false);
    bind("hide_toolbar", ui->checkBoxHideToolbar, false);
    bind("hide_toolbar_labels", ui->checkBoxHideToolbarLabels, true);
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

void ConfigurationManager::addCommandWithoutSave(const Command &command)
{
    CommandWidget *cmdWidget = new CommandWidget(this);

    cmdWidget->setCommand(command);

    connect( cmdWidget, SIGNAL(iconChanged(QString)),
             this, SLOT(onCurrentCommandWidgetIconChanged(QString)) );
    connect( cmdWidget, SIGNAL(nameChanged(QString)),
             this, SLOT(onCurrentCommandWidgetNameChanged(QString)) );

    ui->itemOrderListCommands->appendItem(command.name, command.enable,
                                          getCommandIcon(cmdWidget->currentIcon()), cmdWidget);
}

void ConfigurationManager::loadCommandsFromFile(const QString &fileName)
{
    QSettings commandsSettings(fileName, QSettings::IniFormat);
    QList<int> rowsToSelect;

    foreach ( const Command &command, loadCommands(&commandsSettings) ) {
        rowsToSelect.append(ui->itemOrderListCommands->rowCount());
        addCommandWithoutSave(command);
    }

    ui->itemOrderListCommands->setSelectedRows(rowsToSelect);
}

ConfigurationManager::Commands ConfigurationManager::selectedCommands()
{
    QList<int> rows = ui->itemOrderListCommands->selectedRows();
    const Commands allCommands = commands(false, false);
    Commands commandsToSave;
    foreach (int row, rows) {
        Q_ASSERT(row < allCommands.size());
        commandsToSave.append(allCommands.value(row));
    }

    return commandsToSave;
}

QVariant ConfigurationManager::value(const QString &name) const
{
    if ( m_options.contains(name) )
        return m_options[name].value();
    return QSettings().value("Options/" + name);
}

void ConfigurationManager::setValue(const QString &name, const QVariant &value)
{
    const QString key = "Options/" + name;

    if ( m_options.contains(name) ) {
        if ( m_options[name].value() == value )
            return;

        m_options[name].setValue(value);

        // Save the retrieved option value since option widget can modify it (e.g. int in range).
        Settings().setValue( key, m_options[name].value() );

        emit configurationChanged();
    } else if ( QSettings().value(key) != value ) {
        Settings().setValue(key, value);
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
            if ( !value.isValid() || !m_options[key].setValue(value) )
                log( tr("Invalid value for option \"%1\"").arg(key), LogWarning );
        }
    }
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

    if (m_tabIcons.isEmpty()) {
        const int size = settings.beginReadArray("Tabs");
        for(int i = 0; i < size; ++i) {
            settings.setArrayIndex(i);
            m_tabIcons.insert(settings.value("name").toString(),
                              settings.value("icon").toString());
        }
        settings.endArray();
    }

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

void ConfigurationManager::addCommand(const Command &command)
{
    addCommandWithoutSave(command);
    Commands cmds = commands(false, true);
    cmds.append(command);
    Settings settings;
    saveCommands(cmds, &settings);
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

    foreach ( const QString &tabName, m_tabIcons.keys() ) {
        const QRegExp re(QRegExp::escape(tabName) + "(?:|/.*)$");
        if ( tabs.indexOf(re) == -1 )
            m_tabIcons.remove(tabName);
    }
}

QStringList ConfigurationManager::savedTabs() const
{
    QStringList tabs = value("tabs").toStringList();
    const QString configPath = settingsDirectoryPath();

    QStringList files = QDir(configPath).entryList(QStringList("*_tab_*.dat"));
    files.append( QDir(configPath).entryList(QStringList("*_tab_*.dat.tmp")) );

    QRegExp re("_tab_([^.]*)");

    foreach (const QString fileName, files) {
        if ( fileName.contains(re) ) {
            const QString tabName =
                    QString::fromUtf8(QByteArray::fromBase64(re.cap(1).toUtf8()));
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

QString ConfigurationManager::getIconForTabName(const QString &tabName) const
{
    return m_tabIcons.value(tabName);
}

QIcon ConfigurationManager::getIconForTabName(
        const QString &tabName, const QColor &color, const QColor &activeColor) const
{
    const QString fileName = getIconForTabName(tabName);
    return fileName.isEmpty() ? QIcon() : iconFactory()->iconFromFile(fileName, color, activeColor);
}

void ConfigurationManager::setIconForTabName(const QString &name, const QString &icon)
{
    m_tabIcons[name] = icon;

    QSettings settings;
    settings.beginWriteArray("Tabs");
    int i = 0;

    foreach ( const QString &tabName, m_tabIcons.keys() ) {
        settings.setArrayIndex(i++);
        settings.setValue("name", tabName);
        settings.setValue("icon", m_tabIcons[tabName]);
    }

    settings.endArray();
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
        emit started();
    }
}

void ConfigurationManager::createInstance()
{
    Q_ASSERT(m_Instance == NULL);
#ifndef NO_GLOBAL_SHORTCUTS
    restoreGlobalActions();
#endif
    m_Instance = new ConfigurationManager();
    m_Instance->loadSettings();
    m_Instance->registerWindowGeometry(m_Instance);
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
        saveCommands(commands(false, false), &settings);

        settings.beginGroup("Shortcuts");
        tabShortcuts()->saveShortcuts(settings);
        settings.endGroup();

        settings.beginGroup("Theme");
        tabAppearance()->saveTheme(settings);
        settings.endGroup();

        // save settings for each plugin
        if ( itemFactory()->hasLoaders() ) {
            settings.beginGroup("Plugins");
            for (int i = 0; i < ui->itemOrderListPlugins->itemCount(); ++i) {
                bool isPluginEnabled = ui->itemOrderListPlugins->isItemChecked(i);
                QWidget *w = ui->itemOrderListPlugins->itemWidget(i);
                PluginWidget *pluginWidget = qobject_cast<PluginWidget *>(w);
                pluginWidget->applySettings(&settings, isPluginEnabled);
                itemFactory()->setLoaderEnabled(pluginWidget->loader(), isPluginEnabled);
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
    addCommandWithoutSave(cmd);
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

    foreach (const QString &fileName, fileNames)
        loadCommandsFromFile(fileName);
}

void ConfigurationManager::on_pushButtonSaveCommands_clicked()
{
    QString fileName =
            QFileDialog::getSaveFileName(this, tr("Save Selected Commands"),
                                          QString(), tr("Commands (*.ini)"));
    if ( !fileName.isEmpty() ) {
        if ( !fileName.endsWith(".ini") )
            fileName.append(".ini");

        QSettings settings(fileName, QSettings::IniFormat);
        saveCommands(selectedCommands(), &settings);
        settings.sync();
    }
}

void ConfigurationManager::onFinished(int result)
{
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

    if (!isVisible())
        emit stopped();
}

void ConfigurationManager::on_checkBoxMenuTabIsCurrent_stateChanged(int state)
{
    ui->comboBoxMenuTab->setEnabled(state == Qt::Unchecked);
}

void ConfigurationManager::on_lineEditFilterCommands_textChanged(const QString &text)
{
    for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i) {
        QWidget *w = ui->itemOrderListCommands->itemWidget(i);
        CommandWidget *cmdWidget = qobject_cast<CommandWidget *>(w);
        const Command c = cmdWidget->command();
        bool show = text.isEmpty() || QString(c.name).remove('&').contains(text, Qt::CaseInsensitive)
                || c.cmd.contains(text, Qt::CaseInsensitive);
        ui->itemOrderListCommands->setItemWidgetVisible(i, show);
    }
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

void ConfigurationManager::copySelectedCommandsToClipboard()
{
    const Commands commands = selectedCommands();
    if ( commands.isEmpty() )
        return;

    QTemporaryFile tmpfile;
    if ( !openTemporaryFile(&tmpfile) )
        return;

    if ( !tmpfile.open() )
        return;

    QSettings commandsSettings(tmpfile.fileName(), QSettings::IniFormat);
    saveCommands(commands, &commandsSettings);
    commandsSettings.sync();
    QApplication::clipboard()->setText( QString::fromUtf8(tmpfile.readAll()) );
}

void ConfigurationManager::tryPasteCommandFromClipboard()
{
    const QMimeData *data = clipboardData(QClipboard::Clipboard);
    if (data && data->hasText()) {
        const QString text = data->text().trimmed();
        if ( text.startsWith("[Command]") || text.startsWith("[Commands]") ) {
            QTemporaryFile tmpfile;
            if ( !openTemporaryFile(&tmpfile) )
                return;

            if ( !tmpfile.open() )
                return;

            tmpfile.write(text.toUtf8());
            tmpfile.flush();

            loadCommandsFromFile( tmpfile.fileName() );
        }
    }
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
