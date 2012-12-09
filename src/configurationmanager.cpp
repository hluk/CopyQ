/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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
#include "clipboardmodel.h"
#include "client_server.h"
#include "shortcutdialog.h"
#include "itemdelegate.h"

#include <QSettings>
#include <QMutex>
#include <QFile>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QFontDialog>
#include <QColorDialog>
#include <QFileDialog>

#ifdef Q_OS_WIN
#define DEFAULT_EDITOR "notepad %1"
#else
#define DEFAULT_EDITOR "gedit %1"
#endif

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
                 << tr("Example item 1")
                 << tr("Example item 2")
                 << tr("Example item 3") );
    c->filterItems("item");

#ifdef NO_GLOBAL_SHORTCUTS
    ui->tab_shortcuts->deleteLater();
#endif

    Command cmd;
    int i = 0;
    while ( defaultCommand(++i, &cmd) ) {
        ui->comboBoxCommands->addItem( QIcon(cmd.icon), cmd.name.remove('&') );
    }

    /* general options */
    m_options["maxitems"] = Option(200, "value", ui->spinBoxItems);
    m_options["tray_items"] = Option(5, "value", ui->spinBoxTrayItems);
    m_options["max_image_width"] = Option(320, "value", ui->spinBoxImageWidth);
    m_options["max_image_height"] = Option(240, "value", ui->spinBoxImageHeight);
    m_options["formats"] = Option("image/svg+xml\n"
                                  "image/x-inkscape-svg-compressed\n"
                                  "image/bmp\n"
                                  "text/html\n"
                                  "text/plain",
                                  "plainText", ui->plainTextEditFormats);
    m_options["editor"] = Option(DEFAULT_EDITOR, "text", ui->lineEditEditor);
    m_options["edit_ctrl_return"] = Option(true, "checked", ui->checkBoxEditCtrlReturn);
    m_options["check_clipboard"] = Option(true, "checked", ui->checkBoxClip);
    m_options["confirm_exit"] = Option(true, "checked", ui->checkBoxConfirmExit);
    m_options["vi"] = Option(false, "checked", ui->checkBoxViMode);
    m_options["always_on_top"] = Option(false, "checked", ui->checkBoxAlwaysOnTop);
    m_options["tab_position"] = Option(0, "currentIndex", ui->comboBoxTabPosition);

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

    /* datafile for items */
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    m_datfilename = settings.fileName();
    m_datfilename.replace( QRegExp(".ini$"), QString("_tab_") );

    connect(this, SIGNAL(finished(int)), SLOT(onFinished(int)));

    loadSettings();
}

ConfigurationManager::~ConfigurationManager()
{
    delete ui;
}

void ConfigurationManager::loadItems(ClipboardModel &model, const QString &id)
{
    QString part( id.toLocal8Bit().toBase64() );
    part.replace( QChar('/'), QString('-') );
    QFile file( m_datfilename + part + QString(".dat") );
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);
    in >> model;
}

void ConfigurationManager::saveItems(const ClipboardModel &model, const QString &id)
{
    QString part( id.toLocal8Bit().toBase64() );
    part.replace( QChar('/'), QString('-') );
    QFile file( m_datfilename + part + QString(".dat") );
    file.open(QIODevice::WriteOnly);
    QDataStream out(&file);
    out << model;
}

void ConfigurationManager::removeItems(const QString &id)
{
    QString part( id.toLocal8Bit().toBase64() );
    part.replace( QChar('/'), QString('-') );
    QFile::remove( m_datfilename + part + QString(".dat") );
}

bool ConfigurationManager::defaultCommand(int index, Command *c)
{
    *c = Command();
    switch(index) {
    case 1:
        c->name = tr("Ignore items with no text and single character items");
        c->re   = QRegExp("^\\s+$|^\\s*\\S\\s*$");
        c->icon = ":/images/command_ignore.svg";
        c->ignore = true;
        break;
    case 2:
        c->name = tr("Open in &Browser");
        c->re   = QRegExp("^(https?|ftps?|file|ftp)://");
        c->icon = ":/images/command_web.svg";
        c->cmd  = "firefox %1";
        break;
    case 3:
        c->name = tr("Autoplay videos");
        c->re   = QRegExp("^http://.*\\.(mp4|avi|mkv|wmv|flv|ogv)$");
        c->icon = ":/images/command_autoplay.svg";
        c->cmd  = "vlc %1";
        c->automatic = true;
        break;
    case 4:
        c->name = tr("Move non-textual items to other tab");
        c->re   = QRegExp("^$");
        c->icon = ":/images/command_move.svg";
        c->tab  = "i&mages";
        c->ignore = true;
        break;
    case 5:
        c->name = tr("Copy URL (web address) to other tab");
        c->re   = QRegExp("^(https?|ftps?|file|ftp)://");
        c->icon = ":/images/command_tab.svg";
        c->tab  = "&web";
        break;
    case 6:
        c->name = tr("Run shell script");
        c->re   = QRegExp("^#!/bin/bash");
        c->cmd  = "/bin/bash";
        c->input = true;
        c->output = true;
        c->outputTab = "&BASH";
        c->sep = "\\n";
        c->shortcut = tr("Ctrl+R");
        break;
    default:
        return false;
    }

    return true;
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
        c.name = settings.value( tr("Name") ).toString();
        c.re   = QRegExp( settings.value( tr("Match") ).toString() );
        c.cmd = settings.value( tr("Command") ).toString();
        c.sep = settings.value( tr("Separator") ).toString();
        c.input = settings.value( tr("Input") ).toBool();
        c.output = settings.value( tr("Output") ).toBool();
        c.wait = settings.value( tr("Wait") ).toBool();
        c.automatic = settings.value( tr("Automatic") ).toBool();
        c.ignore = settings.value( tr("Ignore") ).toBool();
        c.enable = settings.value( tr("Enable") ).toBool();
        c.icon = settings.value( tr("Icon") ).toString();
        c.shortcut = settings.value( tr("Shortcut") ).toString();
        c.tab = settings.value( tr("Tab") ).toString();
        c.outputTab = settings.value( tr("OutputTab") ).toString();

        addCommand(c);
    }
    settings.endArray();

    settings.beginGroup("Theme");
    loadTheme(settings);
    settings.endGroup();
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
    QPixmap pix(16, 16);
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

void ConfigurationManager::on_pushButtoAdd_clicked()
{
    Command cmd;
    cmd.input = cmd.output = cmd.wait = cmd.automatic = cmd.ignore = false;
    cmd.sep = QString("\\n");
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
        text = "(command)";
    }
    item->setText(text);

    // icon
    QPixmap pix(c.icon);
    QIcon icon;
    if (!pix.isNull())
        icon = pix.scaledToHeight(16);
    item->setIcon(icon);

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

void ConfigurationManager::on_comboBoxCommands_currentIndexChanged(int index)
{
    Command c;
    if ( defaultCommand(index, &c) ) {
        addCommand(c);
        QListWidget *list = ui->listWidgetCommands;
        list->clearSelection();
        list->setCurrentRow( list->count()-1 );
        ui->comboBoxCommands->setCurrentIndex(0);
    }
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
