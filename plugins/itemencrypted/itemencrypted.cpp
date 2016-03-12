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

#include "itemencrypted.h"
#include "ui_itemencryptedsettings.h"

#include "common/command.h"
#include "common/common.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "gui/icons.h"
#include "gui/iconwidget.h"
#include "item/serialize.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QTextEdit>
#include <QtPlugin>
#include <QVBoxLayout>

namespace {

const char mimeEncryptedData[] = "application/x-copyq-encrypted";

const char dataFileHeader[] = "CopyQ_encrypted_tab";
const char dataFileHeaderV2[] = "CopyQ_encrypted_tab v2";

struct KeyPairPaths {
    KeyPairPaths()
    {
        const QString path = getConfigurationFilePath(QString());
        sec = QDir::toNativeSeparators(path + ".sec");
        pub = QDir::toNativeSeparators(path + ".pub");
    }

    QString sec;
    QString pub;
};

QStringList getDefaultEncryptCommandArguments(const QString &publicKeyPath)
{
    return QStringList() << "--trust-model" << "always" << "--recipient" << "copyq"
                         << "--charset" << "utf-8" << "--display-charset" << "utf-8" << "--no-tty"
                         << "--no-default-keyring" << "--keyring" << publicKeyPath;
}

QStringList getDefaultEncryptCommandArgumentsEscaped()
{
    KeyPairPaths keys;
    QStringList args = getDefaultEncryptCommandArguments(keys.pub);

    // Escape characters
    for ( int i = 0; i < args.size(); ++i ) {
        args[i].replace("\\", "\\\\")
               .replace(" ", "\\ ")
               .replace("\"", "\\\"");
    }

    return args;
}

void startGpgProcess(QProcess *p, const QStringList &args, bool importPrivateKey = false)
{
    KeyPairPaths keys;

    if (importPrivateKey) {
        p->start("gpg", getDefaultEncryptCommandArguments(keys.pub) << "--import" << keys.sec);
        if (!p->waitForFinished(5000)) {
            p->terminate();
            if ( !p->waitForFinished(5000) )
                p->kill();
            return;
        }
    }

    p->start("gpg", getDefaultEncryptCommandArguments(keys.pub) + args);
}

bool verifyProcess(QProcess *p)
{
    if ( p->exitStatus() != QProcess::NormalExit ) {
        COPYQ_LOG( "ItemEncrypt ERROR: Failed to run GnuPG: " + p->errorString() );
        return false;
    } else if ( p->exitCode() != 0 ) {
        const QString errors = p->readAllStandardError();
        if ( !errors.isEmpty() )
            COPYQ_LOG( "ItemEncrypt ERROR: GnuPG stderr:\n" + errors );
        return false;
    }

    return true;
}

QByteArray readGpgOutput(const QStringList &args, const QByteArray &input = QByteArray())
{
    QProcess p;
    startGpgProcess( &p, args );
    p.write(input);
    p.closeWriteChannel();
    p.waitForFinished();
    verifyProcess(&p);
    return p.readAllStandardOutput();
}

bool keysExist()
{
    return !readGpgOutput( QStringList("--list-keys") ).isEmpty();
}

bool decryptMimeData(QVariantMap *detinationData, const QModelIndex &index)
{
    const QVariantMap data = index.data(contentType::data).toMap();
    if ( !data.contains(mimeEncryptedData) )
        return false;

    const QByteArray encryptedBytes = data.value(mimeEncryptedData).toByteArray();
    const QByteArray bytes = readGpgOutput( QStringList() << "--decrypt", encryptedBytes );

    return deserializeData(detinationData, bytes);
}

void encryptMimeData(const QVariantMap &data, const QModelIndex &index, QAbstractItemModel *model)
{
    const QByteArray bytes = serializeData(data);
    const QByteArray encryptedBytes = readGpgOutput( QStringList("--encrypt"), bytes );
    QVariantMap dataMap;
    dataMap.insert(mimeEncryptedData, encryptedBytes);
    model->setData(index, dataMap, contentType::data);
}

} // namespace

ItemEncrypted::ItemEncrypted(QWidget *parent)
    : QWidget(parent)
    , ItemWidget(this)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(6);

    // Show small icon.
    QWidget *iconWidget = new IconWidget(IconLock, this);
    layout->addWidget(iconWidget);
}

void ItemEncrypted::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // Decrypt before editing.
    QTextEdit *textEdit = qobject_cast<QTextEdit *>(editor);
    if (textEdit != NULL) {
        QVariantMap data;
        if ( decryptMimeData(&data, index) ) {
            textEdit->setPlainText( getTextData(data, mimeText) );
            textEdit->selectAll();
        }
    }
}

void ItemEncrypted::setModelData(QWidget *editor, QAbstractItemModel *model,
                                 const QModelIndex &index) const
{
    // Encrypt after editing.
    QTextEdit *textEdit = qobject_cast<QTextEdit*>(editor);
    if (textEdit != NULL)
        encryptMimeData( createDataMap(mimeText, textEdit->toPlainText()), index, model );
}

ItemEncryptedLoader::ItemEncryptedLoader()
    : ui()
    , m_settings()
    , m_gpgProcessStatus(GpgNotRunning)
    , m_gpgProcess(NULL)
{
}

ItemEncryptedLoader::~ItemEncryptedLoader()
{
    terminateGpgProcess();
}

ItemWidget *ItemEncryptedLoader::create(const QModelIndex &index, QWidget *parent) const
{
    const QVariantMap dataMap = index.data(contentType::data).toMap();
    return dataMap.contains(mimeEncryptedData) ? new ItemEncrypted(parent) : NULL;
}

QStringList ItemEncryptedLoader::formatsToSave() const
{
    return QStringList(mimeEncryptedData);
}

QVariantMap ItemEncryptedLoader::applySettings()
{
    Q_ASSERT(ui != NULL);
    m_settings.insert( "encrypt_tabs", ui->plainTextEditEncryptTabs->toPlainText().split('\n') );
    return m_settings;
}

QWidget *ItemEncryptedLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemEncryptedSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    ui->plainTextEditEncryptTabs->setPlainText(
                m_settings.value("encrypt_tabs").toStringList().join("\n") );

    // Check if gpg application is available.
    QProcess p;
    startGpgProcess(&p, QStringList("--version"));
    p.closeWriteChannel();
    p.waitForFinished();
    if ( !verifyProcess(&p) ) {
        m_gpgProcessStatus = GpgNotInstalled;
    } else {
        KeyPairPaths keys;
        ui->labelShareInfo->setTextFormat(Qt::RichText);
        ui->labelShareInfo->setText( tr("To share encrypted items on other computer or"
                                        " session, you'll need public and secret key files:"
                                        "<ul>"
                                        "<li>%1</li>"
                                        "<li>%2<br />(Keep this secret key in a safe place.)</li>"
                                        "</ul>"
                                        )
                                     .arg( quoteString(keys.pub) )
                                     .arg( quoteString(keys.sec) )
                                     );
    }

    updateUi();

    connect( ui->pushButtonPassword, SIGNAL(clicked()),
             this, SLOT(setPassword()) );

    return w;
}

bool ItemEncryptedLoader::canLoadItems(QFile *file) const
{
    QDataStream stream(file);

    QString header;
    stream >> header;

    return stream.status() == QDataStream::Ok
            && (header == dataFileHeader || header == dataFileHeaderV2);
}

bool ItemEncryptedLoader::canSaveItems(const QAbstractItemModel &model) const
{
    const QString tabName = model.property("tabName").toString();

    foreach ( const QString &encryptTabName, m_settings.value("encrypt_tabs").toStringList() ) {
        if ( encryptTabName.isEmpty() )
            continue;

        QString tabName1 = tabName;

        // Ignore ampersands (usually just for underlining mnemonics) if none is specified.
        if ( !hasKeyHint(encryptTabName) )
            removeKeyHint(tabName1);

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

bool ItemEncryptedLoader::loadItems(QAbstractItemModel *model, QFile *file)
{
    // This is needed to skip header.
    if ( !canLoadItems(file) )
        return false;

    if (m_gpgProcessStatus == GpgNotInstalled) {
        emit error( tr("GnuPG must be installed to view encrypted tabs.") );
        return false;
    }

    QProcess p;
    startGpgProcess( &p, QStringList("--decrypt"), true );

    char encryptedBytes[4096];

    QDataStream stream(file);
    while ( !stream.atEnd() ) {
        const int bytesRead = stream.readRawData(encryptedBytes, 4096);
        if (bytesRead == -1) {
            emitEncryptFailed();
            COPYQ_LOG("ItemEncrypted ERROR: Failed to read encrypted data");
            return false;
        }
        p.write(encryptedBytes, bytesRead);
    }

    p.closeWriteChannel();
    p.waitForFinished();

    const QByteArray bytes = p.readAllStandardOutput();
    if ( bytes.isEmpty() ) {
        emitEncryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to read decrypted data.");
        verifyProcess(&p);
        return false;
    }

    QDataStream stream2(bytes);

    quint64 maxItems = model->property("maxItems").toInt();
    quint64 length;
    stream2 >> length;
    if ( length <= 0 || stream2.status() != QDataStream::Ok ) {
        emitEncryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to parse item count!");
        return false;
    }
    length = qMin(length, maxItems) - model->rowCount();

    for ( quint64 i = 0; i < length && stream2.status() == QDataStream::Ok; ++i ) {
        if ( !model->insertRow(i) ) {
            emitEncryptFailed();
            COPYQ_LOG("ItemEncrypt ERROR: Failed to insert item!");
            return false;
        }
        QVariantMap dataMap;
        stream2 >> dataMap;
        model->setData( model->index(i, 0), dataMap, contentType::data );
    }

    if ( stream2.status() != QDataStream::Ok ) {
        emitEncryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to decrypt item!");
        return false;
    }

    return true;
}

bool ItemEncryptedLoader::saveItems(const QAbstractItemModel &model, QFile *file)
{
    if (m_gpgProcessStatus == GpgNotInstalled)
        return false;

    quint64 length = model.rowCount();
    if (length == 0)
        return false; // No need to encode empty tab.

    QByteArray bytes;

    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_4_7);

        stream << length;

        for (quint64 i = 0; i < length && stream.status() == QDataStream::Ok; ++i) {
            QModelIndex index = model.index(i, 0);
            const QVariantMap dataMap = index.data(contentType::data).toMap();
            stream << dataMap;
        }
    }

    bytes = readGpgOutput(QStringList("--encrypt"), bytes);
    if ( bytes.isEmpty() ) {
        emitDecryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to read encrypted data");
        return false;
    }

    QDataStream stream(file);
    stream << QString(dataFileHeaderV2);
    stream.writeRawData( bytes.data(), bytes.size() );

    if ( stream.status() != QDataStream::Ok ) {
        emitDecryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to write encrypted data");
        return false;
    }

    return true;
}

bool ItemEncryptedLoader::initializeTab(QAbstractItemModel *)
{
    return true;
}

QString ItemEncryptedLoader::script() const
{
    const QString args = '"' + getDefaultEncryptCommandArgumentsEscaped().join("\",\"") + '"';

    return
        "plugins." + id() + " = {"

        "\n" "  mime: '" + QString(mimeEncryptedData) + "',"

        "\n" "  gpgRun: function(gpgArg, bytes) {"
        "\n" "    var cmd = execute('gpg', " + args + ", gpgArg, null, bytes)"
        "\n" "    if (!cmd)"
        "\n" "      throw 'Failed to execute GPG!'"
        "\n" "    if (cmd.exit_code != 0)"
        "\n" "      throw 'GPG (exit code ' + cmd.exit_code + '): ' + cmd.stderr"
        "\n" "    return cmd.stdout"
        "\n" "  },"

        "\n" "  encrypt: function(bytes) {"
        "\n" "    return this.gpgRun('--encrypt', bytes)"
        "\n" "  },"

        "\n" "  decrypt: function(bytes) {"
        "\n" "    return this.gpgRun('--decrypt', bytes)"
        "\n" "  },"

        "\n" "}";
}

QList<Command> ItemEncryptedLoader::commands() const
{
    QList<Command> commands;

    Command c;
    c.name = tr("Encrypt (needs GnuPG)");
    c.icon = QString(QChar(IconLock));
    c.input = mimeItems;
    c.output = mimeEncryptedData;
    c.inMenu = true;
    c.transform = true;
    c.cmd = "copyq: plugins.itemencrypted.encrypt(input())";
    c.shortcuts.append( toPortableShortcutText(tr("Ctrl+L")) );
    commands.append(c);

    c = Command();
    c.name = tr("Decrypt");
    c.icon = QString(QChar(IconUnlock));
    c.input = mimeEncryptedData;
    c.output = mimeItems;
    c.inMenu = true;
    c.transform = true;
    c.cmd = "copyq: plugins.itemencrypted.decrypt(input())";
    c.shortcuts.append( toPortableShortcutText(tr("Ctrl+L")) );
    commands.append(c);

    c = Command();
    c.name = tr("Decrypt and Copy");
    c.icon = QString(QChar(IconUnlockAlt));
    c.input = mimeEncryptedData;
    c.inMenu = true;
    c.cmd = "copyq:\n"
             "  var data = plugins.itemencrypted.decrypt(input());\n"
             "  if (data)\n"
             "    copy(\"" + QString(mimeItems) + "\", data, \"" + QString(mimeHidden) + "\", 1)";
    c.shortcuts.append( toPortableShortcutText(tr("Ctrl+Shift+L")) );
    commands.append(c);

    return commands;
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
                error = error.arg(getTextData(m_gpgProcess->readAllStandardError()));
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
        ui->groupBoxShareInfo->hide();
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

void ItemEncryptedLoader::emitEncryptFailed()
{
    emit error( tr("Encryption failed!") );
}

void ItemEncryptedLoader::emitDecryptFailed()
{
    emit error( tr("Decryption failed!") );
}

Q_EXPORT_PLUGIN2(itemencrypted, ItemEncryptedLoader)
