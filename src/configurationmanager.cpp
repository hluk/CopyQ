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

#include "client_server.h"
#include "clipboardmodel.h"
#include "iconfactory.h"
#include "itemdelegate.h"
#include "itemfactory.h"
#include "itemwidget.h"
#include "pluginwidget.h"
#include "shortcutdialog.h"

#include <QColorDialog>
#include <QFile>
#include <QFileDialog>
#include <QFontDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMutex>
#include <QSettings>
#include <QTreeWidgetItem>

#ifdef Q_OS_WIN
#define DEFAULT_EDITOR "notepad %1"
#else
#define DEFAULT_EDITOR "gedit %1"
#endif

const QRegExp reURL("^(https?|ftps?|file)://");

// singleton
ConfigurationManager* ConfigurationManager::m_Instance = 0;

ConfigurationManager *ConfigurationManager::instance()
{
    static QMutex mutex;

    if (!m_Instance) {
        QMutexLocker lock(&mutex);
        if (!m_Instance)
            m_Instance = new ConfigurationManager();
    }

    return m_Instance;
}

void ConfigurationManager::drop()
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    delete m_Instance;
    m_Instance = NULL;
}

ConfigurationManager::ConfigurationManager()
    : QDialog()
    , ui(new Ui::ConfigurationManager)
    , m_datfilename()
    , m_options()
    , m_theme()
    , m_commands()
{
    ui->setupUi(this);

    ui->scrollAreaCommand->hide();

    ClipboardBrowser *c = ui->clipboardBrowserPreview;
    c->addItems( QStringList()
                 << tr("Search string is \"item\".")
                 << tr("Select an item and\n"
                       "press F2 to edit.")
                 << tr("Select items and move them with\n"
                       "CTRL and up or down key.")
                 << tr("Remove item with Delete key.")
                 << tr("Example item %1").arg(1)
                 << tr("Example item %1").arg(2)
                 << tr("Example item %1").arg(3) );
    c->filterItems( tr("item") );

#ifdef NO_GLOBAL_SHORTCUTS
    ui->tabShortcuts->deleteLater();
#endif

    Command cmd;
    int i = 0;
    QMenu *menu = new QMenu(ui->toolButtonAddCommand);
    ui->toolButtonAddCommand->setMenu(menu);
    while ( defaultCommand(++i, &cmd) ) {
        menu->addAction( IconFactory::iconFromFile(cmd.icon), cmd.name.remove('&') )
                ->setProperty("COMMAND", i);
    }

    /* general options */
    m_options["maxitems"] = Option(200, "value", ui->spinBoxItems);
    m_options["editor"] = Option(DEFAULT_EDITOR, "text", ui->lineEditEditor);
    m_options["edit_ctrl_return"] = Option(true, "checked", ui->checkBoxEditCtrlReturn);
    m_options["check_clipboard"] = Option(true, "checked", ui->checkBoxClip);
    m_options["confirm_exit"] = Option(true, "checked", ui->checkBoxConfirmExit);
    m_options["vi"] = Option(false, "checked", ui->checkBoxViMode);
    m_options["always_on_top"] = Option(false, "checked", ui->checkBoxAlwaysOnTop);
    m_options["tab_position"] = Option(0, "currentIndex", ui->comboBoxTabPosition);
    m_options["text_wrap"] = Option(true, "checked", ui->checkBoxTextWrap);

    m_options["tray_items"] = Option(5, "value", ui->spinBoxTrayItems);
    m_options["tray_commands"] = Option(true, "checked", ui->checkBoxTrayShowCommands);
    m_options["tray_tab_is_current"] = Option(true, "checked", ui->checkBoxMenuTabIsCurrent);
    m_options["tray_images"] = Option(true, "checked", ui->checkBoxTrayImages);
    m_options["tray_tab"] = Option("", "text", ui->comboBoxMenuTab->lineEdit());

    // Tooltip to show on command line.
    ui->comboBoxMenuTab->lineEdit()->setToolTip( ui->comboBoxMenuTab->toolTip() );

    /* other options */
    m_options["tabs"] = Option(QStringList());
    m_options["command_history_size"] = Option(100);
    m_options["_last_hash"] = Option(0);
#ifndef NO_GLOBAL_SHORTCUTS
    /* shortcuts */
    m_options["toggle_shortcut"] = Option("", "text", ui->pushButton);
    m_options["menu_shortcut"] = Option("", "text", ui->pushButton_2);
    m_options["edit_clipboard_shortcut"] = Option("", "text", ui->pushButton_3);
    m_options["edit_shortcut"] = Option("", "text", ui->pushButton_4);
    m_options["second_shortcut"] = Option("", "text", ui->pushButton_5);
    m_options["show_action_dialog"] = Option("", "text", ui->pushButton_6);
    m_options["new_item_shortcut"] = Option("", "text", ui->pushButton_7);
#endif
#ifdef Q_WS_X11
    /* X11 clipboard selection monitoring and synchronization */
    m_options["check_selection"] = Option(true, "checked", ui->checkBoxSel);
    m_options["copy_clipboard"] = Option(true, "checked", ui->checkBoxCopyClip);
    m_options["copy_selection"] = Option(true, "checked", ui->checkBoxCopySel);
#else
    ui->checkBoxCopySel->hide();
    ui->checkBoxSel->hide();
    ui->checkBoxCopyClip->hide();
#endif

    // values of last submitted action
    m_options["action_has_input"]  = Option(false);
    m_options["action_has_output"] = Option(false);
    m_options["action_separator"]  = Option("\\n");
    m_options["action_output_tab"] = Option("");

    /* appearance options */
    QString name;
    QPalette p;
    name = p.color(QPalette::Base).name();
    m_theme["bg"]          = Option(name, "VALUE", ui->pushButtonColorBg);
    m_theme["edit_bg"]     = Option(name, "VALUE", ui->pushButtonColorEditorBg);
    name = p.color(QPalette::Text).name();
    m_theme["fg"]          = Option(name, "VALUE", ui->pushButtonColorFg);
    m_theme["edit_fg"]     = Option(name, "VALUE", ui->pushButtonColorEditorFg);
    name = p.color(QPalette::Text).lighter(400).name();
    m_theme["num_fg"]      = Option(name, "VALUE", ui->pushButtonColorNumberFg);
    name = p.color(QPalette::AlternateBase).name();
    m_theme["alt_bg"]      = Option(name, "VALUE", ui->pushButtonColorAltBg);
    name = p.color(QPalette::Highlight).name();
    m_theme["sel_bg"]      = Option(name, "VALUE", ui->pushButtonColorSelBg);
    name = p.color(QPalette::HighlightedText).name();
    m_theme["sel_fg"]      = Option(name, "VALUE", ui->pushButtonColorSelFg);
    m_theme["find_bg"]     = Option("#ff0", "VALUE", ui->pushButtonColorFindBg);
    m_theme["find_fg"]     = Option("#000", "VALUE", ui->pushButtonColorFindFg);
    m_theme["font"]        = Option("", "VALUE", ui->pushButtonFont);
    m_theme["edit_font"]   = Option("", "VALUE", ui->pushButtonEditorFont);
    m_theme["find_font"]   = Option("", "VALUE", ui->pushButtonFoundFont);
    m_theme["num_font"]    = Option("", "VALUE", ui->pushButtonNumberFont);
    m_theme["show_number"] = Option(true, "checked", ui->checkBoxShowNumber);
    m_theme["show_scrollbars"] = Option(true, "checked", ui->checkBoxScrollbars);

    m_options["use_system_icons"] = Option(false, "checked", ui->checkBoxSystemIcons);

    /* datafile for items */
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    m_datfilename = settings.fileName();
    m_datfilename.replace( QRegExp(".ini$"), QString("_tab_") );

    connect(this, SIGNAL(finished(int)), SLOT(onFinished(int)));

    // Tab icons
    QTabWidget *tw = ui->tabWidget;
    tw->setTabIcon( tw->indexOf(ui->tabClipboard), getIcon("", IconPaste) );
    tw->setTabIcon( tw->indexOf(ui->tabGeneral), getIcon("", IconListOl) );
    tw->setTabIcon( tw->indexOf(ui->tabTray), getIcon("", IconInbox) );
    tw->setTabIcon( tw->indexOf(ui->tabCommands), getIcon("", IconCogs) );
    tw->setTabIcon( tw->indexOf(ui->tabShortcuts), getIcon("", IconHandUp) );
    tw->setTabIcon( tw->indexOf(ui->tabAppearance), getIcon("", IconPicture) );

    loadSettings();
}

ConfigurationManager::~ConfigurationManager()
{
    delete ui;
}

void ConfigurationManager::loadItems(ClipboardModel &model, const QString &id)
{
    const QString fileName = itemFileName(id);

    // Load file with items.
    QFile file(fileName);
    if ( !file.exists() ) {
        // Try to open temp file if regular file doesn't exist.
        file.setFileName(fileName + ".tmp");
        if ( !file.exists() )
            return;
        file.rename(fileName);
    }
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);
    in >> model;
}

void ConfigurationManager::saveItems(const ClipboardModel &model, const QString &id)
{
    const QString fileName = itemFileName(id);

    // Save to temp file.
    QFile file( fileName + ".tmp" );
    file.open(QIODevice::WriteOnly);
    QDataStream out(&file);
    out << model;

    // Overwrite previous file.
    QFile::remove(fileName);
    file.rename(fileName);
}

void ConfigurationManager::removeItems(const QString &id)
{
    QFile::remove( itemFileName(id) );
}

bool ConfigurationManager::defaultCommand(int index, Command *c)
{
    *c = Command();
    switch(index) {
    case 1:
        c->name = tr("New command");
        c->input = c->output = "";
        c->wait = c->automatic = c->ignore = false;
        c->sep = QString("\\n");
        break;
    case 2:
        c->name = tr("Ignore items with no or single character");
        c->re   = QRegExp("^\\s*\\S?\\s*$");
        c->icon = QString(QChar(IconExclamationSign));
        c->ignore = true;
        break;
    case 3:
        c->name = tr("Open in &Browser");
        c->re   = reURL;
        c->icon = QString(QChar(IconGlobe));
        c->cmd  = "firefox %1";
        break;
    case 4:
        c->name = tr("Autoplay videos");
        c->re   = QRegExp("^http://.*\\.(mp4|avi|mkv|wmv|flv|ogv)$");
        c->icon = QString(QChar(IconPlayCircle));
        c->cmd  = "vlc %1";
        c->automatic = true;
        break;
    case 5:
        c->name = tr("Copy URL (web address) to other tab");
        c->re   = reURL;
        c->icon = QString(QChar(IconCopy));
        c->tab  = "&web";
        break;
    case 6:
        c->name = tr("Run shell script");
        c->re   = QRegExp("^#!/bin/bash");
        c->icon = QString(QChar(IconEdit));
        c->cmd  = "/bin/bash";
        c->input = "text/plain";
        c->output = "text/plain";
        c->outputTab = "&BASH";
        c->sep = "\\n";
        c->shortcut = tr("Ctrl+R");
        break;
    case 7:
        c->name = tr("Create thumbnail (needs ImageMagick)");
        c->icon = QString(QChar(IconPicture));
        c->cmd  = "convert - -resize 92x92 png:-";
        c->input = "image/png";
        c->output = "image/png";
        break;
    case 8:
        c->name = tr("Create QR Code from URL (needs qrencode)");
        c->re   = reURL;
        c->icon = QString(QChar(IconQRCode));
        c->cmd  = "qrencode -o - -t PNG -s 6";
        c->input = "text/plain";
        c->output = "image/png";
        break;
    case 9:
        c->name = tr("Label image");
        c->icon = QString(QChar(IconTag));
        c->cmd  = "base64 | perl -p -i -e 'BEGIN {print \"<img src=\\\\\"data:image/bmp;base64,\"} "
                  "END {print \"\\\\\" /><p>LABEL\"}'";
        c->input = "image/bmp";
        c->output = "text/html";
        c->wait = true;
        break;
    case 10:
        c->name = tr("Open URL");
        c->re   = reURL;
        c->icon = QString(QChar(IconEyeOpen));
        c->cmd  = "curl %1";
        c->input = "text/plain";
        c->output = "text/html";
        break;
    case 11:
        c->name = tr("Add to &TODO tab");
        c->icon = QString(QChar(IconShare));
        c->cmd  = QApplication::arguments().value(0) + " tab TODO write text/plain -";
        c->input = "text/plain";
        break;
#if defined(Q_WS_X11) || defined(Q_OS_WIN)
    case 12:
        c->name  = tr("Ignore *\"Password\"* window");
        c->wndre = QRegExp(tr("Password"));
        c->icon = QString(QChar(IconAsterisk));
        c->ignore = true;
        break;
#endif
    default:
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

void ConfigurationManager::updateIcons()
{
    IconFactory *factory = IconFactory::instance();
    factory->invalidateCache();
    factory->setUseSystemIcons(value("use_system_icons").toBool());

    // Command button icons.
    ui->toolButtonAddCommand->setIcon( getIcon("list-add", IconPlus) );
    ui->pushButtonRemove->setIcon( getIcon("list-remove", IconMinus) );
    ui->pushButtonDown->setIcon( getIcon("go-down", IconArrowDown) );
    ui->pushButtonUp->setIcon( getIcon("go-up", IconArrowUp) );
}

void ConfigurationManager::decorateBrowser(ClipboardBrowser *c) const
{
    QFont font;
    QPalette p;
    QColor color;

    /* fonts */
    font.fromString( ui->pushButtonFont->property("VALUE").toString() );
    c->setFont(font);

    /* scrollbars */
    Qt::ScrollBarPolicy scrollbarPolicy = m_theme["show_scrollbars"].value().toBool()
            ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    c->setVerticalScrollBarPolicy(scrollbarPolicy);
    c->setHorizontalScrollBarPolicy(scrollbarPolicy);

    /* colors */
    p = c->palette();
    color.setNamedColor( m_theme["bg"].value().toString() );
    p.setBrush(QPalette::Base, color);
    color.setNamedColor( m_theme["fg"].value().toString() );
    p.setBrush(QPalette::Text, color);
    color.setNamedColor( m_theme["alt_bg"].value().toString() );
    p.setBrush(QPalette::AlternateBase, color);
    color.setNamedColor( m_theme["sel_bg"].value().toString() );
    p.setBrush(QPalette::Highlight, color);
    color.setNamedColor( m_theme["sel_fg"].value().toString() );
    p.setBrush(QPalette::HighlightedText, color);
    c->setPalette(p);

    /* search style */
    ItemDelegate *d = static_cast<ItemDelegate *>( c->itemDelegate() );
    font.fromString( m_theme["find_font"].value().toString() );
    color.setNamedColor( m_theme["find_bg"].value().toString() );
    p.setColor(QPalette::Base, color);
    color.setNamedColor( m_theme["find_fg"].value().toString() );
    p.setColor(QPalette::Text, color);
    d->setSearchStyle(font, p);

    /* editor style */
    d->setSearchStyle(font, p);
    font.fromString( m_theme["edit_font"].value().toString() );
    color.setNamedColor( m_theme["edit_bg"].value().toString() );
    p.setColor(QPalette::Base, color);
    color.setNamedColor( m_theme["edit_fg"].value().toString() );
    p.setColor(QPalette::Text, color);
    d->setEditorStyle(font, p);

    /* number style */
    d->setShowNumber(m_theme["show_number"].value().toBool());
    font.fromString( m_theme["num_font"].value().toString() );
    color.setNamedColor( m_theme["num_fg"].value().toString() );
    p.setColor(QPalette::Text, color);
    d->setNumberStyle(font, p);

    c->redraw();
}

bool ConfigurationManager::loadGeometry(QWidget *widget) const
{
    QSettings settings;
    QString widgetName = widget->objectName();
    QVariant option = settings.value("Options/" + widgetName + "_geometry");
    return widget->restoreGeometry(option.toByteArray());
}

void ConfigurationManager::saveGeometry(const QWidget *widget)
{
    QSettings settings;
    QString widgetName = widget->objectName();
    settings.setValue( "Options/" + widgetName + "_geometry",
                       widget->saveGeometry() );
}

QVariant ConfigurationManager::value(const QString &name) const
{
    return m_options[name].value();
}

void ConfigurationManager::setValue(const QString &name, const QVariant &value)
{
    m_options[name].setValue(value);
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
    settings.endGroup();

    ui->listWidgetCommands->clear();
    m_commands.clear();

    int size = settings.beginReadArray("Commands");
    m_commands.reserve(size);
    for(int i=0; i<size; ++i)
    {
        settings.setArrayIndex(i);

        Command c;
        c.name = settings.value("Name").toString();
        c.re   = QRegExp( settings.value("Match").toString() );
        c.wndre = QRegExp( settings.value("Window").toString() );
        c.cmd = settings.value("Command").toString();
        c.sep = settings.value("Separator").toString();

        c.input = settings.value("Input").toString();
        if ( c.input == "false" || c.input == "true" )
            c.input = c.input == "true" ? QString("text/plain") : QString();

        c.output = settings.value("Output").toString();
        if ( c.output == "false" || c.output == "true" )
            c.output = c.output == "true" ? QString("text/plain") : QString();

        c.wait = settings.value("Wait").toBool();
        c.automatic = settings.value("Automatic").toBool();
        c.ignore = settings.value("Ignore").toBool();
        c.enable = settings.value("Enable").toBool();
        c.icon = settings.value("Icon").toString();
        c.shortcut = settings.value("Shortcut").toString();
        c.tab = settings.value("Tab").toString();
        c.outputTab = settings.value("OutputTab").toString();

        addCommand(c);
    }
    settings.endArray();

    settings.beginGroup("Theme");
    loadTheme(settings);
    settings.endGroup();

    updateIcons();

    // load settings for each plugin
    settings.beginGroup("Plugins");
    foreach (ItemLoaderInterface *loader, ItemFactory::instance()->loaders()) {
        settings.beginGroup(loader->name());

        QVariantMap s;
        foreach (const QString &name, settings.allKeys()) {
            s[name] = settings.value(name);
        }
        loader->loadSettings(s);
        loader->setEnabled( settings.value("enabled", true).toBool() );

        settings.endGroup();
    }
    settings.endGroup();
    emit loadPluginConfiguration();

    // load plugin priority
    const QStringList pluginPriority =
            settings.value("plugin_priority", QStringList()).toStringList();
    ItemFactory::instance()->setPluginPriority(pluginPriority);

    // reload plugin widgets
    while ( ui->tabWidgetPlugins->count() > 0 )
        delete ui->tabWidgetPlugins->widget(0);

    foreach (ItemLoaderInterface *loader, ItemFactory::instance()->loaders()) {
        PluginWidget *pluginWidget = new PluginWidget(loader, ui->tabWidgetPlugins);
        ui->tabWidgetPlugins->addTab(pluginWidget, loader->name());

        // fix tab order
        QWidget *lastWidget = ui->tabWidgetPlugins;
        foreach (QWidget *w, pluginWidget->findChildren<QWidget*>()) {
            if (w->focusPolicy() != Qt::NoFocus) {
                setTabOrder(lastWidget, w);
                lastWidget = w;
            }
        }

        connect( this, SIGNAL(applyPluginConfiguration()),
                 pluginWidget, SLOT(applySettings()) );
    }

    on_checkBoxMenuTabIsCurrent_stateChanged( ui->checkBoxMenuTabIsCurrent->checkState() );
}

void ConfigurationManager::saveSettings()
{
    QSettings settings;

    settings.beginGroup("Options");
    foreach ( const QString &key, m_options.keys() ) {
        settings.setValue( key, m_options[key].value() );
    }
    settings.endGroup();

    // save commands
    settings.beginWriteArray("Commands");
    int i=0;
    foreach (const Command &c, m_commands) {
        settings.setArrayIndex(i++);
        settings.setValue("Name", c.name);
        settings.setValue("Match", c.re.pattern());
        settings.setValue("Window", c.wndre.pattern());
        settings.setValue("Command", c.cmd);
        settings.setValue("Separator", c.sep);
        settings.setValue("Input", c.input);
        settings.setValue("Output", c.output);
        settings.setValue("Wait", c.wait);
        settings.setValue("Automatic", c.automatic);
        settings.setValue("Ignore", c.ignore);
        settings.setValue("Enable", c.enable);
        settings.setValue("Icon", c.icon);
        settings.setValue("Shortcut", c.shortcut);
        settings.setValue("Tab", c.tab);
        settings.setValue("OutputTab", c.outputTab);
    }
    settings.endArray();

    settings.beginGroup("Theme");
    saveTheme(settings);
    settings.endGroup();

    updateIcons();

    // save settings for each plugin
    emit applyPluginConfiguration();
    settings.beginGroup("Plugins");
    foreach (ItemLoaderInterface *loader, ItemFactory::instance()->loaders()) {
        settings.beginGroup(loader->name());

        QVariantMap s = loader->applySettings();
        foreach (const QString &name, s.keys()) {
            settings.setValue(name, s[name]);
        }

        settings.setValue("enabled", loader->isEnabled());

        settings.endGroup();
    }
    settings.endGroup();

    // save plugin priority
    QStringList pluginPriority;
    for (int i = 0; i <  ui->tabWidgetPlugins->count(); ++i)
        pluginPriority.append( ui->tabWidgetPlugins->tabText(i) );
    settings.setValue("plugin_priority", pluginPriority);
    ItemFactory::instance()->setPluginPriority(pluginPriority);

    QStringList formats = ItemFactory::instance()->formatsToSave();
    formats.prepend(QString("text/plain"));
    formats.prepend(QString());
    formats.removeDuplicates();
    ui->widgetCommand->setFormats(formats);

    emit configurationChanged();
}

void ConfigurationManager::loadTheme(QSettings &settings)
{
    foreach ( const QString &key, m_theme.keys() ) {
        if ( settings.contains(key) ) {
            QVariant value = settings.value(key);
            if ( value.isValid() )
                m_theme[key].setValue(value);
        }
    }

    /* color indicating icons for color buttons */
    QSize iconSize(16, 16);
    QPixmap pix(iconSize);
    QList<QPushButton *> buttons;
    buttons << ui->pushButtonColorBg << ui->pushButtonColorFg
            << ui->pushButtonColorAltBg
            << ui->pushButtonColorSelBg << ui->pushButtonColorSelFg
            << ui->pushButtonColorFindBg << ui->pushButtonColorFindFg
            << ui->pushButtonColorEditorBg << ui->pushButtonColorEditorFg
            << ui->pushButtonColorNumberFg;
    foreach (QPushButton *button, buttons) {
        QColor color = button->property("VALUE").toString();
        pix.fill(color);
        button->setIcon(pix);
        button->setIconSize(iconSize);
    }

    decorateBrowser(ui->clipboardBrowserPreview);
}

void ConfigurationManager::saveTheme(QSettings &settings) const
{
    foreach ( const QString &key, m_theme.keys() )
        settings.setValue( key, m_theme[key].value() );
}

ConfigurationManager::Commands ConfigurationManager::commands() const
{
    Commands commands;
    foreach (const Command &c, m_commands) {
        if (c.enable)
            commands.append(c);
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

void ConfigurationManager::addCommand(const Command &c)
{
    m_commands.append(c);

    QListWidget *list = ui->listWidgetCommands;
    QListWidgetItem *item = new QListWidgetItem(list);
    item->setCheckState(c.enable ? Qt::Checked : Qt::Unchecked);
    updateCommandItem(item);
}

void ConfigurationManager::setTabs(const QStringList &tabs)
{
    setValue("tabs", tabs);
    ui->widgetCommand->setTabs(tabs);

    QString text = ui->comboBoxMenuTab->currentText();
    ui->comboBoxMenuTab->clear();
    ui->comboBoxMenuTab->addItem(QString());
    ui->comboBoxMenuTab->addItems(tabs);
    ui->comboBoxMenuTab->setEditText(text);
}

void ConfigurationManager::apply()
{
    // update current command
    int row = ui->listWidgetCommands->currentRow();
    if (row >= 0) {
        m_commands[row] = ui->widgetCommand->command();
        updateCommandItem( ui->listWidgetCommands->item(row) );
    }

    // commit changes on command
    QListWidget *list = ui->listWidgetCommands;
    QListWidgetItem *item = list->currentItem();
    if (item != NULL) {
        int row = ui->listWidgetCommands->row(item);
        m_commands[row] = ui->widgetCommand->command();
        updateCommandItem(item);
    }

    saveSettings();
}

void ConfigurationManager::on_toolButtonAddCommand_triggered(QAction *action)
{
    Command cmd;
    if ( !defaultCommand(action->property("COMMAND").toInt(), &cmd) )
        return;
    addCommand(cmd);

    QListWidget *list = ui->listWidgetCommands;
    QListWidgetItem *item = list->item(list->count()-1);
    list->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
    ui->widgetCommand->setFocus();
}


void ConfigurationManager::on_pushButtonRemove_clicked()
{
    QListWidget *list = ui->listWidgetCommands;
    const QItemSelectionModel *sel = list->selectionModel();
    QListWidgetItem *current = list->currentItem();
    bool deleteCurrent = current != NULL && current->isSelected();

    list->blockSignals(true);

    // remove selected rows
    QModelIndexList indexes = sel->selectedRows();
    while ( !indexes.isEmpty() ) {
        int row = indexes.first().row();
        m_commands.removeAt(row);
        delete ui->listWidgetCommands->takeItem(row);
        indexes = sel->selectedRows();
    }

    if (deleteCurrent) {
        int row = list->currentRow();
        ui->widgetCommand->setCommand(row >= 0 ? m_commands[row] : Command());
        ui->widgetCommand->setEnabled(row >= 0);
    }

    list->blockSignals(false);
}

void ConfigurationManager::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    loadGeometry(this);
}

void ConfigurationManager::onFinished(int result)
{
    saveGeometry(this);
    if (result == QDialog::Accepted) {
        apply();
    } else {
        loadSettings();
    }
}

void ConfigurationManager::on_pushButtonUp_clicked()
{
    QListWidget *list = ui->listWidgetCommands;
    int row = list->currentRow();
    if (row < 1)
        return;

    list->blockSignals(true);
    m_commands.swap(row, row-1);
    list->setCurrentRow(row-1);
    list->blockSignals(false);

    updateCommandItem( list->item(row) );
    updateCommandItem( list->item(row-1) );
}

void ConfigurationManager::on_pushButtonDown_clicked()
{
    QListWidget *list = ui->listWidgetCommands;
    int row = list->currentRow();
    if (row < 0 || row == list->count()-1)
        return;

    list->blockSignals(true);
    m_commands.swap(row, row+1);
    list->setCurrentRow(row+1);
    list->blockSignals(false);

    updateCommandItem( list->item(row) );
    updateCommandItem( list->item(row+1) );
}

void ConfigurationManager::updateCommandItem(QListWidgetItem *item)
{
    QListWidget *list = ui->listWidgetCommands;

    list->blockSignals(true);

    int row = list->row(item);
    const Command &c = m_commands[row];

    // text
    QString text;
    if ( !c.name.isEmpty() ) {
        text = c.name;
        text.remove('&');
    } else if ( !c.cmd.isEmpty() ) {
        text = c.cmd;
    } else {
        text = tr("<untitled command>");
    }
    item->setText(text);

    // icon
    item->setIcon( IconFactory::iconFromFile(c.icon) );

    // check state
    item->setCheckState(c.enable ? Qt::Checked : Qt::Unchecked);

    list->blockSignals(false);
}

void ConfigurationManager::shortcutButtonClicked(QPushButton *button)
{
    ShortcutDialog *dialog = new ShortcutDialog(this);
    if (dialog->exec() == QDialog::Accepted) {
        QKeySequence shortcut = dialog->shortcut();
        QString text;
        if ( !shortcut.isEmpty() )
            text = shortcut.toString(QKeySequence::NativeText);
        button->setText(text);
    }
}

void ConfigurationManager::fontButtonClicked(QPushButton *button)
{
    QFont font;
    font.fromString( button->property("VALUE").toString() );
    QFontDialog dialog(font, this);
    if ( dialog.exec() == QDialog::Accepted ) {
        font = dialog.selectedFont();
        button->setProperty( "VALUE", font.toString() );
        decorateBrowser(ui->clipboardBrowserPreview);
    }
}

void ConfigurationManager::colorButtonClicked(QPushButton *button)
{
    QColor color = button->property("VALUE").toString();
    QColorDialog dialog(color, this);
    if ( dialog.exec() == QDialog::Accepted ) {
        color = dialog.selectedColor();
        button->setProperty( "VALUE", color.name() );
        decorateBrowser(ui->clipboardBrowserPreview);

        QPixmap pix(16, 16);
        pix.fill(color);
        button->setIcon(pix);
    }
}

void ConfigurationManager::on_pushButton_clicked()
{
    shortcutButtonClicked(ui->pushButton);
}

void ConfigurationManager::on_pushButton_2_clicked()
{
    shortcutButtonClicked(ui->pushButton_2);
}

void ConfigurationManager::on_pushButton_3_clicked()
{
    shortcutButtonClicked(ui->pushButton_3);
}

void ConfigurationManager::on_pushButton_4_clicked()
{
    shortcutButtonClicked(ui->pushButton_4);
}

void ConfigurationManager::on_pushButton_5_clicked()
{
    shortcutButtonClicked(ui->pushButton_5);
}

void ConfigurationManager::on_pushButton_6_clicked()
{
    shortcutButtonClicked(ui->pushButton_6);
}

void ConfigurationManager::on_pushButton_7_clicked()
{
    shortcutButtonClicked(ui->pushButton_7);
}

void ConfigurationManager::on_listWidgetCommands_currentItemChanged(
        QListWidgetItem *current, QListWidgetItem *previous)
{
    int row;
    if (previous != NULL) {
        row = ui->listWidgetCommands->row(previous);
        if ( row < m_commands.size() ) {
            m_commands[row] = ui->widgetCommand->command();
            updateCommandItem(previous);
        }
    }

    bool ok = current != NULL;
    if (ok)
        row = ui->listWidgetCommands->row(current);
    ui->scrollAreaCommand->setVisible(ok);
    ui->widgetCommand->setCommand(ok ? m_commands[row] : Command());
    ui->widgetCommand->setEnabled(ok);
}

void ConfigurationManager::on_listWidgetCommands_itemChanged(
        QListWidgetItem *item)
{
    if ( ui->listWidgetCommands->currentItem() != item )
        return;

    int row = ui->listWidgetCommands->row(item);
    Command c = ui->widgetCommand->command();
    c.enable = m_commands[row].enable = (item->checkState() == Qt::Checked);
    ui->widgetCommand->setCommand(c);
}

void ConfigurationManager::on_listWidgetCommands_itemSelectionChanged()
{
    const QItemSelectionModel *sel = ui->listWidgetCommands->selectionModel();
    ui->pushButtonRemove->setEnabled( sel->hasSelection() );
}

void ConfigurationManager::on_pushButtonFont_clicked()
{
    fontButtonClicked(ui->pushButtonFont);
}

void ConfigurationManager::on_pushButtonEditorFont_clicked()
{
    fontButtonClicked(ui->pushButtonEditorFont);
}

void ConfigurationManager::on_pushButtonFoundFont_clicked()
{
    fontButtonClicked(ui->pushButtonFoundFont);
}

void ConfigurationManager::on_pushButtonNumberFont_clicked()
{
    fontButtonClicked(ui->pushButtonNumberFont);
}

void ConfigurationManager::on_pushButtonColorBg_clicked()
{
    colorButtonClicked(ui->pushButtonColorBg);
}

void ConfigurationManager::on_pushButtonColorFg_clicked()
{
    colorButtonClicked(ui->pushButtonColorFg);
}

void ConfigurationManager::on_pushButtonColorAltBg_clicked()
{
    colorButtonClicked(ui->pushButtonColorAltBg);
}

void ConfigurationManager::on_pushButtonColorSelBg_clicked()
{
    colorButtonClicked(ui->pushButtonColorSelBg);
}

void ConfigurationManager::on_pushButtonColorSelFg_clicked()
{
    colorButtonClicked(ui->pushButtonColorSelFg);
}

void ConfigurationManager::on_pushButtonColorFindBg_clicked()
{
    colorButtonClicked(ui->pushButtonColorFindBg);
}

void ConfigurationManager::on_pushButtonColorFindFg_clicked()
{
    colorButtonClicked(ui->pushButtonColorFindFg);
}

void ConfigurationManager::on_pushButtonColorEditorBg_clicked()
{
    colorButtonClicked(ui->pushButtonColorEditorBg);
}

void ConfigurationManager::on_pushButtonColorEditorFg_clicked()
{
    colorButtonClicked(ui->pushButtonColorEditorFg);
}

void ConfigurationManager::on_pushButtonColorNumberFg_clicked()
{
    colorButtonClicked(ui->pushButtonColorNumberFg);
}

void ConfigurationManager::on_pushButtonLoadTheme_clicked()
{
    const QString filename =
        QFileDialog::getOpenFileName(this, tr("Open Theme File"), QString(), QString("*.ini"));
    if ( !filename.isNull() ) {
        QSettings settings(filename, QSettings::IniFormat);
        loadTheme(settings);
    }
}

void ConfigurationManager::on_pushButtonSaveTheme_clicked()
{
    QString filename =
        QFileDialog::getSaveFileName(this, tr("Save Theme File As"), QString(), QString("*.ini"));
    if ( !filename.isNull() ) {
        if ( !filename.endsWith(".ini") )
            filename.append(".ini");
        QSettings settings(filename, QSettings::IniFormat);
        saveTheme(settings);
    }
}

void ConfigurationManager::on_checkBoxShowNumber_stateChanged(int)
{
    decorateBrowser(ui->clipboardBrowserPreview);
}

void ConfigurationManager::on_checkBoxScrollbars_stateChanged(int)
{
    decorateBrowser(ui->clipboardBrowserPreview);
}

void ConfigurationManager::on_checkBoxMenuTabIsCurrent_stateChanged(int state)
{
    ui->comboBoxMenuTab->setEnabled(state == Qt::Unchecked);
}

void ConfigurationManager::on_pushButtonPluginPriorityUp_clicked()
{
    QTabWidget *tabs = ui->tabWidgetPlugins;
    const int i = tabs->currentIndex();
    if (i > 0) {
        tabs->insertTab(i - 1, tabs->widget(i), tabs->tabText(i));
        tabs->setCurrentIndex(i - 1);
    }
}

void ConfigurationManager::on_pushButtonPluginPriorityDown_clicked()
{
    QTabWidget *tabs = ui->tabWidgetPlugins;
    const int i = tabs->currentIndex();
    if (i >= 0 && i + 1 < tabs->count()) {
        tabs->insertTab(i + 1, tabs->widget(i), tabs->tabText(i));
        tabs->setCurrentIndex(i + 1);
    }
}
