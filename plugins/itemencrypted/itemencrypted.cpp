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

#include "itemencrypted.h"
#include "ui_itemencryptedsettings.h"

#include "common/contenttype.h"
#include "item/encrypt.h"
#include "item/serialize.h"

#include <QFile>
#include <QVBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSettings>
#include <QtPlugin>

namespace {

const QString defaultFormat = QString("application/x-copyq-encrypted");

void startGpgProcess(QProcess *p, const QStringList &args)
{
    p->start("gpg", getDefaultEncryptCommandArguments() + args);
}

QByteArray readGpgOutput(const QStringList &args, const QByteArray &input = QByteArray())
{
    QProcess p;
    startGpgProcess( &p, args );
    p.write(input);
    p.closeWriteChannel();
    p.waitForFinished();
    return p.readAllStandardOutput();
}

bool keysExist()
{
    return !readGpgOutput( QStringList("--list-keys") ).isEmpty();
}

int getEncryptedFormatIndex(const QModelIndex &index)
{
    const QStringList formats = index.data(contentType::formats).toStringList();
    return formats.indexOf(defaultFormat);
}

bool decryptMimeData(QMimeData *data, const QModelIndex &index, int formatIndex)
{
    if (formatIndex < 0)
        return false;

    const QByteArray encryptedBytes = index.data(contentType::firstFormat + formatIndex).toByteArray();
    const QByteArray bytes = readGpgOutput( QStringList() << "--decrypt", encryptedBytes );

    return deserializeData(data, bytes);
}

void encryptMimeData(const QMimeData &data, const QModelIndex &index, QAbstractItemModel *model)
{
    const QByteArray bytes = serializeData(data);
    const QByteArray encryptedBytes = readGpgOutput( QStringList("--encrypt"), bytes );
    QVariantMap dataMap;
    dataMap.insert(defaultFormat, encryptedBytes);
    model->setData(index, dataMap, contentType::data);
}

bool isEncryptedFile(QFile *file)
{
    file->seek(0);
    QDataStream stream(file);

    QString header;
    stream >> header;

    return header == QString("CopyQ_encrypted_tab");
}

} // namespace

ItemEncrypted::ItemEncrypted(QWidget *parent)
    : QWidget(parent)
    , ItemWidget(this)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(6);

    // Show small icon.
    QLabel *iconLabel = new QLabel(this);
    iconLabel->setObjectName("item_child");
    iconLabel->setTextFormat(Qt::RichText);
    iconLabel->setText("<span style=\"font-family:FontAwesome\">&#xf023;</span>");
    layout->addWidget(iconLabel);

    updateSize();
}

void ItemEncrypted::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // Decrypt before editing.
    QPlainTextEdit *textEdit = qobject_cast<QPlainTextEdit *>(editor);
    if (textEdit != NULL) {
        QMimeData data;
        if ( decryptMimeData(&data, index, getEncryptedFormatIndex(index)) ) {
            textEdit->setPlainText(data.text());
            textEdit->selectAll();
        }
    }
}

void ItemEncrypted::setModelData(QWidget *editor, QAbstractItemModel *model,
                                 const QModelIndex &index) const
{
    // Encrypt after editing.
    QPlainTextEdit *textEdit = qobject_cast<QPlainTextEdit*>(editor);
    if (textEdit != NULL) {
        QMimeData data;
        data.setText( textEdit->toPlainText() );
        encryptMimeData(data, index, model);
    }
}

void ItemEncrypted::updateSize()
{
    setMinimumWidth(maximumWidth());
    adjustSize();
}

ItemEncryptedLoader::ItemEncryptedLoader()
    : ui(NULL)
    , m_settings()
    , m_gpgProcessStatus(GpgNotRunning)
    , m_gpgProcess(NULL)
{
}

ItemEncryptedLoader::~ItemEncryptedLoader()
{
    terminateGpgProcess();
    delete ui;
}

ItemWidget *ItemEncryptedLoader::create(const QModelIndex &index, QWidget *parent) const
{
    const QStringList formats = index.data(contentType::formats).toStringList();
    return formats.contains(defaultFormat) ? new ItemEncrypted(parent) : NULL;
}

QStringList ItemEncryptedLoader::formatsToSave() const
{
    return QStringList(defaultFormat);
}

QVariantMap ItemEncryptedLoader::applySettings()
{
    Q_ASSERT(ui != NULL);
    m_settings.insert( "encrypt_tabs", ui->plainTextEditEncryptTabs->toPlainText().split('\n') );
    return m_settings;
}

QWidget *ItemEncryptedLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemEncryptedSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    ui->plainTextEditEncryptTabs->setPlainText(
                m_settings.value("encrypt_tabs").toStringList().join("\n") );

    // Check if gpg application is available.
    QProcess p;
    startGpgProcess(&p, QStringList("--version"));
    p.closeWriteChannel();
    p.waitForFinished();
    if ( p.error() != QProcess::UnknownError ) {
        m_gpgProcessStatus = GpgNotInstalled;
    } else {
        KeyPairPaths keys;
        ui->labelShareInfo->setTextFormat(Qt::RichText);
        ui->labelShareInfo->setText( tr("To share encrypted items on other computer or"
                                        " session, you'll need public and secret key files:"
                                        "<ul>"
                                        "<li>\"%1\"</li>"
                                        "<li>\"%2\"<br />(Keep this secret key on a safe place.)</li>"
                                        "</ul>"
                                        )
                                     .arg(keys.pub)
                                     .arg(keys.sec)
                                     );
    }

    updateUi();

    connect( ui->pushButtonPassword, SIGNAL(clicked()),
             this, SLOT(setPassword()) );

    return w;
}

bool ItemEncryptedLoader::loadItems(const QString &, QAbstractItemModel *model, QFile *file)
{
    bool encrypted = isEncryptedFile(file);

    // decrypt
    if (encrypted) {
        model->setProperty("disabled", true);

        if (m_gpgProcessStatus == GpgNotInstalled)
            return true;

        QProcess p;
        startGpgProcess( &p, QStringList("--decrypt") );

        char encryptedBytes[4096];

        QDataStream stream(file);
        while ( !stream.atEnd() ) {
            const int bytesRead = stream.readRawData(encryptedBytes, 4096);
            p.write(encryptedBytes, bytesRead);
        }

        p.closeWriteChannel();
        p.waitForFinished();

        const QByteArray bytes = p.readAllStandardOutput();
        if ( bytes.isEmpty() )
            return true;

        QDataStream stream2(bytes);

        quint64 maxItems = model->property("maxItems").toInt();
        quint64 length;
        stream2 >> length;
        if ( length <= 0 || stream2.status() != QDataStream::Ok )
            return true;
        length = qMin(length, maxItems) - model->rowCount();

        for ( quint64 i = 0; i < length && stream2.status() == QDataStream::Ok; ++i ) {
            if ( !model->insertRow(i) )
                return true;
            QVariantMap dataMap;
            stream2 >> dataMap;
            model->setData( model->index(i, 0), dataMap, contentType::data );
        }

        if (stream2.status() != QDataStream::Ok)
            return true;

        model->setProperty("disabled", false);
    }

    return encrypted;
}

bool ItemEncryptedLoader::saveItems(const QString &tabName, const QAbstractItemModel &model, QFile *file)
{
    if (m_gpgProcessStatus == GpgNotInstalled)
        return false;

    if ( !shouldEncryptTab(tabName) )
        return false;

    quint64 length = model.rowCount();
    if (length == 0)
        return false; // No need to encode empty tab.

    QByteArray bytes;

    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << length;

        for (quint64 i = 0; i < length && stream.status() == QDataStream::Ok; ++i) {
            QVariantMap dataMap;
            QModelIndex index = model.index(i, 0);
            const QStringList formats = index.data(contentType::formats).toStringList();
            for ( int j = 0; j < formats.size(); ++j )
                dataMap.insert(formats[j], index.data(contentType::firstFormat + j).toByteArray() );
            stream << dataMap;
        }
    }

    bytes = readGpgOutput(QStringList("--encrypt"), bytes);
    if ( bytes.isEmpty() )
        return false;

    QDataStream stream(file);
    stream << QString("CopyQ_encrypted_tab");
    stream.writeRawData( bytes.data(), bytes.size() );

    return true;
}

void ItemEncryptedLoader::itemsLoaded(const QString &tabName, QAbstractItemModel *model, QFile *file)
{
    if (m_gpgProcessStatus == GpgNotInstalled)
        return;

    // Check if items need to be save again.
    bool encrypt = shouldEncryptTab(tabName);
    if ( encrypt != isEncryptedFile(file) ) {
        if (encrypt)
            saveItems(tabName, *model, file);
        else
            model->setProperty("dirty", true);
    }
}

void ItemEncryptedLoader::setPassword()
{
    if (m_gpgProcessStatus == GpgGeneratingKeys)
        return;

    if (m_gpgProcess != NULL) {
        terminateGpgProcess();
        return;
    }

    if ( !keysExist() ) {
        // Generate keys if they don't exist.
        const KeyPairPaths keys;
        m_gpgProcessStatus = GpgGeneratingKeys;
        m_gpgProcess = new QProcess(this);
        startGpgProcess( m_gpgProcess, QStringList() << "--batch" << "--gen-key" );
        m_gpgProcess->write( "\nKey-Type: RSA"
                 "\nKey-Usage: encrypt"
                 "\nKey-Length: 2048"
                 "\nName-Real: copyq"
                 "\n%secring " + keys.sec.toUtf8() +
                 "\n%pubring " + keys.pub.toUtf8() +
                 "\n%commit"
                 "\n" );
        m_gpgProcess->closeWriteChannel();
    } else {
        // Change password.
        m_gpgProcessStatus = GpgChangingPassword;
        m_gpgProcess = new QProcess(this);
        startGpgProcess( m_gpgProcess, QStringList() << "--edit-key" << "copyq" << "passwd" << "save");
    }

    m_gpgProcess->waitForStarted();
    if ( m_gpgProcess->state() == QProcess::NotRunning ) {
        onGpgProcessFinished( m_gpgProcess->exitCode(), m_gpgProcess->exitStatus() );
    } else {
        connect( m_gpgProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
                 this, SLOT(onGpgProcessFinished(int,QProcess::ExitStatus)) );
        updateUi();
    }
}

void ItemEncryptedLoader::terminateGpgProcess()
{
    if (m_gpgProcess == NULL)
        return;
    QProcess *p = m_gpgProcess;
    m_gpgProcess = NULL;
    p->terminate();
    p->waitForFinished();
    p->deleteLater();
    m_gpgProcessStatus = GpgNotRunning;
    updateUi();
}

void ItemEncryptedLoader::onGpgProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString error;

    if (m_gpgProcess != NULL) {
        if (ui != NULL) {
            error = tr("Error: %1");
            if (exitStatus != QProcess::NormalExit)
                error = error.arg(m_gpgProcess->errorString());
            else if (exitCode != 0)
                error = error.arg(QString::fromUtf8(m_gpgProcess->readAllStandardError()));
            else if ( m_gpgProcess->error() != QProcess::UnknownError )
                error = error.arg(m_gpgProcess->errorString());
            else if ( !keysExist() )
                error = error.arg( tr("Failed to generate keys.") );
            else
                error.clear();
        }

        m_gpgProcess->deleteLater();
        m_gpgProcess = NULL;
    }

    GpgProcessStatus oldStatus = m_gpgProcessStatus;
    m_gpgProcessStatus = GpgNotRunning;

    if ( oldStatus == GpgGeneratingKeys && error.isEmpty() ) {
        setPassword();
    } else {
        updateUi();
        ui->labelInfo->setText( error.isEmpty() ? tr("Done") : error );
    }
}

bool ItemEncryptedLoader::shouldEncryptTab(const QString &tabName) const
{
    foreach ( const QString &encryptTabName, m_settings.value("encrypt_tabs").toStringList() ) {
        QString tabName1 = tabName;

        // Ignore ampersands (usually just for underlining mnemonics) if none is specified.
        if ( !encryptTabName.contains('&') )
            tabName1.remove('&');

        // Ignore path in tab tree if none path separator is specified.
        if ( !encryptTabName.contains('/') ) {
            const int i = tabName1.lastIndexOf('/');
            tabName1.remove(0, i + 1);
        }

        if ( tabName1 == encryptTabName )
            return true;
    }

    return false;
}

void ItemEncryptedLoader::updateUi()
{
    if (ui == NULL)
        return;

    if (m_gpgProcessStatus == GpgNotInstalled) {
        ui->labelInfo->setText("To use item encryption, install"
                               " <a href=\"http://www.gnupg.org/\">GnuPG</a>"
                               " application and restart CopyQ.");
        ui->pushButtonPassword->hide();
        ui->groupBoxEncryptTabs->hide();
    } else if (m_gpgProcessStatus == GpgGeneratingKeys) {
        ui->labelInfo->setText( tr("Creating new keys (this may take a few minutes)...") );
        ui->pushButtonPassword->setText( tr("Cancel") );
    } else if (m_gpgProcessStatus == GpgChangingPassword) {
        ui->labelInfo->setText( tr("Setting new password...") );
        ui->pushButtonPassword->setText( tr("Cancel") );
    } else if ( !keysExist() ) {
        ui->labelInfo->setText( tr("Encryption keys <strong>must be generated</strong>"
                                   " before item encryption can be used.") );
        ui->pushButtonPassword->setText( tr("Generate New Keys...") );
    } else {
        ui->pushButtonPassword->setText( tr("Change Password...") );
    }
}

Q_EXPORT_PLUGIN2(itemdata, ItemEncryptedLoader)
