#include "configurationmanager.h"
#include "ui_configurationmanager.h"
#include "clipboardmodel.h"
#include "client_server.h"
#include "shortcutdialog.h"
#include <QFile>
#include <QtGui/QDesktopWidget>
#include <QMessageBox>

#ifdef Q_WS_WIN
#define DEFAULT_EDITOR "notepad %1"
#else
#define DEFAULT_EDITOR "gedit %1"
#endif

struct _Option {
    _Option() : m_obj(NULL) {}
    _Option(const QVariant &default_value, const char *property_name = NULL, QObject *obj = NULL) :
        m_default_value(default_value), m_property_name(property_name), m_obj(obj)
    {
        reset();
    }

    QVariant value() const
    {
        return m_obj ? m_obj->property(m_property_name) : m_value;
    }

    void setValue(const QVariant &value)
    {
        if (m_obj)
            m_obj->setProperty(m_property_name, value);
        else
            m_value = value;
    }

    void reset()
    {
        setValue(m_default_value);
    }

    QString tooltip() const
    {
        return m_obj ? m_obj->property("toolTip").toString() : QString();
    }

    /* default value and also type (int, float, boolean, QString) */
    QVariant m_default_value, m_value;
    const char *m_property_name;
    QObject *m_obj;
};

inline bool readCssFile(QIODevice &device, QSettings::SettingsMap &map)
{
    map.insert( "css", device.readAll() );
    return true;
}

inline bool writeCssFile(QIODevice &device, const QSettings::SettingsMap &map)
{
    device.write( map["css"].toString().toLocal8Bit() );
    return true;
}

// singleton
ConfigurationManager* ConfigurationManager::m_Instance = 0;

ConfigurationManager::ConfigurationManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigurationManager)
{
    ui->setupUi(this);
#ifdef NO_GLOBAL_SHORTCUTS
    ui->tab_shortcuts->deleteLater();
#endif

    /* datafile for items */
    // do not use registry in windows
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());

    /* options */
    m_options.insert( "maxitems",
                      Option(200, "value", ui->spinBoxItems) );
    m_options.insert( "tray_items",
                      Option(5, "value", ui->spinBoxTrayItems) );
    m_options.insert( "max_image_width",
                      Option(320, "value", ui->spinBoxImageWidth) );
    m_options.insert( "max_image_height",
                      Option(240, "value", ui->spinBoxImageHeight) );
    m_options.insert( "formats",
                      Option("image/svg+xml\nimage/x-inkscape-svg-compressed\nimage/bmp\ntext/html\ntext/plain",
                             "plainText", ui->plainTextEditFormats) );
    // TODO: get default editor from environment variable EDITOR
    m_options.insert( "editor",
                      Option(DEFAULT_EDITOR, "text", ui->lineEditEditor) );
    m_options.insert( "check_clipboard",
                      Option(true, "checked", ui->checkBoxSel) );
    m_options.insert( "confirm_exit",
                      Option(true, "checked", ui->checkBoxConfirmExit) );
    m_options.insert( "tabs",
                      Option(QStringList()) );
#ifndef NO_GLOBAL_SHORTCUTS
    m_options.insert( "toggle_shortcut",
                      Option("(No Shortcut)", "text", ui->pushButton));
    m_options.insert( "menu_shortcut",
                      Option("(No Shortcut)", "text", ui->pushButton_2));
#endif
#ifdef Q_WS_X11
    m_options.insert( "check_selection",
                      Option(true, "checked", ui->checkBoxClip) );
    m_options.insert( "copy_clipboard",
                      Option(true, "checked", ui->checkBoxCopyClip) );
    m_options.insert( "copy_selection",
                      Option(true, "checked", ui->checkBoxCopySel) );
#else
    ui->checkBoxCopySel->hide();
    ui->checkBoxSel->hide();
    ui->checkBoxCopyClip->hide();
#endif

    m_datfilename = settings.fileName();
    m_datfilename.replace( QRegExp(".ini$"), QString("_tab_") );

    // read style sheet from configuration
    cssFormat = QSettings::registerFormat("css", readCssFile, writeCssFile);
    readStyleSheet();

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

void ConfigurationManager::readStyleSheet()
{
    QSettings cssSettings( cssFormat, QSettings::UserScope,
                           QCoreApplication::organizationName(),
                           QCoreApplication::applicationName() );

    QString css = cssSettings.value("css").toString();

    if ( css.isEmpty() ) {
        resetStyleSheet();
    } else {
        setStyleSheet(css);
        ui->plainTextEdit_css->setPlainText(css);
    }
}

void ConfigurationManager::writeStyleSheet()
{
    QSettings cssSettings( cssFormat, QSettings::UserScope,
                           QCoreApplication::organizationName(),
                           QCoreApplication::applicationName() );

    cssSettings.setValue( "css", styleSheet() );
}

void ConfigurationManager::resetStyleSheet()
{
    QFile default_css(":/styles/default.css");
    if ( default_css.open(QIODevice::ReadOnly) ) {
        QString css = default_css.readAll();
        setStyleSheet(css);
        ui->plainTextEdit_css->setPlainText(css);
    }
}

QByteArray ConfigurationManager::windowGeometry(const QString &widget_name, const QByteArray &geometry)
{
    QSettings settings;

    if ( geometry.isEmpty() ) {
        return settings.value("Options/"+widget_name+"_geometry").toByteArray();
    } else {
        settings.setValue("Options/"+widget_name+"_geometry", geometry);
        return geometry;
    }
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
    return m_options.keys();
}

QString ConfigurationManager::optionToolTip(const QString &name) const
{
    return m_options[name].tooltip();
}

void ConfigurationManager::loadSettings()
{
    QSettings settings;

    QVariant value;

    settings.beginGroup("Options");
    foreach( const QString &key, m_options.keys() ) {
        m_options[key].setValue( settings.value(key) );
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

        addCommand(c);
    }
    settings.endArray();
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

void ConfigurationManager::saveSettings()
{
    QSettings settings;

    settings.beginGroup("Options");
    foreach( const QString &key, m_options.keys() ) {
        settings.setValue( key, m_options[key].value() );
    }
    settings.endGroup();

    // commit changes on command
    QListWidget *list = ui->listWidgetCommands;
    QListWidgetItem *item = list->currentItem();
    if (item != NULL) {
        int row = ui->listWidgetCommands->row(item);
        m_commands[row] = ui->widgetCommand->command();
        updateCommandItem(item);
    }

    // save commands
    settings.beginWriteArray("Commands");
    int i=0;
    foreach (const Command &c, m_commands) {
        settings.setArrayIndex(i++);
        settings.setValue( tr("Name"), c.name );
        settings.setValue( tr("Match"), c.re.pattern() );
        settings.setValue( tr("Command"), c.cmd );
        settings.setValue( tr("Separator"), c.sep );
        settings.setValue( tr("Input"), c.input );
        settings.setValue( tr("Output"), c.output );
        settings.setValue( tr("Wait"), c.wait );
        settings.setValue( tr("Automatic"), c.automatic );
        settings.setValue( tr("Ignore"), c.ignore );
        settings.setValue( tr("Enable"), c.enable );
        settings.setValue( tr("Icon"), c.icon );
        settings.setValue( tr("Shortcut"), c.shortcut );
    }
    settings.endArray();

    writeStyleSheet();

    emit configurationChanged();
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
            foreach( const QString &key, m_options.keys() ) {
                m_options[key].reset();
            }
            resetStyleSheet();
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

void ConfigurationManager::apply()
{
    // update current command
    int row = ui->listWidgetCommands->currentRow();
    if (row >= 0) {
        m_commands[row] = ui->widgetCommand->command();
        updateCommandItem( ui->listWidgetCommands->item(row) );
    }

    // style sheet
    QString css = ui->plainTextEdit_css->toPlainText();
    if ( css.isEmpty() )
        resetStyleSheet();
    else
        setStyleSheet(css);

    saveSettings();
}

void ConfigurationManager::on_pushButtoAdd_clicked()
{
    Command cmd;
    cmd.input = cmd.output = cmd.wait = cmd.automatic = cmd.ignore = false;
    cmd.sep = QString('\n');
    addCommand(cmd);

    QListWidget *list = ui->listWidgetCommands;
    QListWidgetItem *item = list->item(list->count()-1);
    list->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
    ui->widgetCommand->setFocus();
}


void ConfigurationManager::on_pushButtonRemove_clicked()
{
    const QItemSelectionModel *sel = ui->listWidgetCommands->selectionModel();

    // remove selected rows
    QModelIndexList indexes = sel->selectedRows();
    foreach (QModelIndex index, indexes) {
        int row = index.row();
        m_commands.removeAt(row);
        delete ui->listWidgetCommands->takeItem(row);
    }
}

void ConfigurationManager::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);

    /* try to resize the dialog so that vertical scrollbar in the about
     * document is hidden
     */
    restoreGeometry( windowGeometry(objectName()) );
}

void ConfigurationManager::onFinished(int result)
{
    windowGeometry( objectName(), saveGeometry() );
    if (result == QDialog::Accepted)
        apply();
    else
        loadSettings();
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

void ConfigurationManager::getKey(QPushButton *button)
{
    ShortcutDialog *dialog = new ShortcutDialog(this);
    if (dialog->exec() == QDialog::Accepted) {
        QKeySequence shortcut = dialog->shortcut();
        QString text;
        if (shortcut.isEmpty())
            text = tr("(No Shortcut)");
        else
            text = shortcut.toString(QKeySequence::NativeText);
        button->setText(text);
    }
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

void ConfigurationManager::on_pushButton_clicked()
{
    getKey(ui->pushButton);
}

void ConfigurationManager::on_pushButton_2_clicked()
{
    getKey(ui->pushButton_2);
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
    ui->widgetCommand->setCommand(ok ? m_commands[row] : Command());
    ui->widgetCommand->setEnabled(ok);
    ui->pushButtonRemove->setEnabled(ok);
}

void ConfigurationManager::on_listWidgetCommands_itemChanged(
        QListWidgetItem *item)
{
    int row = ui->listWidgetCommands->row(item);
    Command c = ui->widgetCommand->command();
    c.enable = m_commands[row].enable = (item->checkState() == Qt::Checked);
    ui->widgetCommand->setCommand(c);
}
