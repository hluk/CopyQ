#include "configurationmanager.h"
#include "ui_configurationmanager.h"
#include "clipboardmodel.h"
#include <QFile>
#include <QSettings>
#include <QtGui/QDesktopWidget>

inline bool readCssFile(QIODevice &device, QSettings::SettingsMap &map)
{
    map.insert( "css", device.readAll() );
    return true;
}

inline bool writeCssFile(QIODevice &, const QSettings::SettingsMap &)
{
    return true;
}

// singleton
ConfigurationManager* ConfigurationManager::m_Instance = 0;

ConfigurationManager::ConfigurationManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigurationManager)
{
    ui->setupUi(this);

    /* datafile for items */
    // do not use registry in windows
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    // ini -> dat
    m_datfilename = settings.fileName();
    m_datfilename.replace( QRegExp("ini$"),QString("dat") );

    // read style sheet from configuration
    readStyleSheet();

    m_keys[Interval] = "interval";
    m_keys[Callback] = "callback";
    m_keys[Formats]  = "formats";
    m_keys[MaxItems] = "maxitems";
    m_keys[Priority] = "priority";
    m_keys[Editor]   = "editor";
    m_keys[ItemHTML] = "format";
    m_keys[CopyClipboard] = "copy_clipboard";
    m_keys[CopySelection] = "copy_selection";
    m_keys[CheckClipboard] = "check_clipboard";
    m_keys[CheckSelection] = "check_selection";

    loadSettings();
}

ConfigurationManager::~ConfigurationManager()
{
    delete ui;
}

QVariant ConfigurationManager::value(Option opt) const
{
    switch(opt) {
    case Interval: return ui->spinInterval->value();
    case Callback: return ui->lineEditScript->text();
    case Formats: return ui->lineEditFormats->text();
    case MaxItems: return ui->spinBoxItems->value();
    case Priority: return ui->lineEditPriority->text();
    case Editor: return ui->lineEditEditor->text();
    case ItemHTML: return ui->plainTextEdit_html->toPlainText();
    case CheckClipboard: return ui->checkBoxClip->isChecked();
    case CheckSelection: return ui->checkBoxSel->isChecked();
    case CopyClipboard: return ui->checkBoxCopyClip->isChecked();
    case CopySelection: return ui->checkBoxCopySel->isChecked();
    default: return QVariant();
    }
}

void ConfigurationManager::loadItems(ClipboardModel &model)
{
    QFile file(m_datfilename);
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);

    in >> model;
}

void ConfigurationManager::saveItems(const ClipboardModel &model)
{
    QFile file(m_datfilename);
    file.open(QIODevice::WriteOnly);
    QDataStream out(&file);
    out << model;
}

void ConfigurationManager::readStyleSheet()
{
    QSettings::Format cssFormat = QSettings::registerFormat(
            "css", readCssFile, writeCssFile);

    QSettings cssSettings( cssFormat, QSettings::UserScope,
                           QCoreApplication::organizationName(),
                           QCoreApplication::applicationName() );

    QString css = cssSettings.value("css", "").toString();

    setStyleSheet(css);
}

QRect ConfigurationManager::windowRect( const QString &window_name, const QRect &newrect )
{
    QSettings settings(QApplication::organizationName(),
                       QString("window"));

    if ( newrect.isValid() ) {
        settings.setValue(window_name+"_rect", newrect);
        return newrect;
    } else {
        QRect rect = settings.value(window_name+"_rect", QRect()).toRect();
        if ( !rect.isValid() ) {
            // default size (whole desktop)
            QDesktopWidget *desktop = QApplication::desktop();
            return desktop->rect();
        }
        return rect;
    }
}


void ConfigurationManager::setValue(Option opt, const QVariant &value)
{
    switch(opt) {
    case Interval:
        ui->spinInterval->setValue( value.toInt() );
        break;
    case Callback:
        ui->lineEditScript->setText( value.toString() );
        break;
    case Formats:
        ui->lineEditFormats->setText( value.toString() );
        break;
    case MaxItems:
        ui->spinBoxItems->setValue( value.toInt() );
        break;
    case Priority:
        ui->lineEditPriority->setText( value.toString() );
        break;
    case Editor:
        ui->lineEditEditor->setText( value.toString() );
        break;
    case ItemHTML:
        ui->plainTextEdit_html->setPlainText( value.toString() );
        break;
    case CheckClipboard:
        ui->checkBoxClip->setChecked( value.toBool() );
        break;
    case CheckSelection:
        ui->checkBoxSel->setChecked( value.toBool() );
        break;
    case CopyClipboard:
        ui->checkBoxCopyClip->setChecked( value.toBool() );
        break;
    case CopySelection:
        ui->checkBoxCopySel->setChecked( value.toBool() );
        break;
    default: break;
    }
}

void ConfigurationManager::loadSettings()
{
    QSettings settings;

    foreach( QString key, settings.allKeys() ) {
        QVariant value = settings.value(key);
        setValue( m_keys.key(key), value );
    }
}

void ConfigurationManager::saveSettings()
{
    QSettings settings;

    foreach( Option opt, m_keys.keys() ) {
        settings.setValue(m_keys[opt], value(opt));
    }
}

void ConfigurationManager::on_buttonBox_clicked(QAbstractButton* button)
{
    switch( ui->buttonBox->buttonRole(button) ) {
    case QDialogButtonBox::ApplyRole:
        accept();
        break;
    case QDialogButtonBox::AcceptRole:
        accept();
        close();
        break;
    default:
        return;
    }
}

void ConfigurationManager::accept()
{
    setStyleSheet( ui->plainTextEdit_css->toPlainText() );
    emit configurationChanged();
    saveSettings();
}
