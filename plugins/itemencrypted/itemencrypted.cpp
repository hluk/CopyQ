/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include "common/config.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/processsignals.h"
#include "common/textdata.h"
#include "gui/icons.h"
#include "gui/iconwidget.h"
#include "item/serialize.h"

#ifdef HAS_TESTS
#   include "tests/itemencryptedtests.h"
#endif

#include <QAbstractItemModel>
#include <QDir>
#include <QIODevice>
#include <QLabel>
#include <QModelIndex>
#include <QSettings>
#include <QTextEdit>
#include <QtPlugin>
#include <QVBoxLayout>

namespace {

const QLatin1String mimeEncryptedData("application/x-copyq-encrypted");

const QLatin1String dataFileHeader("CopyQ_encrypted_tab");
const QLatin1String dataFileHeaderV2("CopyQ_encrypted_tab v2");

const QLatin1String configEncryptTabs("encrypt_tabs");

const int maxItemCount = 10000;

bool waitOrTerminate(QProcess *p, int timeoutMs)
{
    p->waitForStarted();

    if ( p->state() != QProcess::NotRunning && !p->waitForFinished(timeoutMs) ) {
        p->terminate();
        if ( !p->waitForFinished(5000) )
            p->kill();
        return false;
    }

    return true;
}

bool verifyProcess(QProcess *p, int timeoutMs = 30000)
{
    if ( !waitOrTerminate(p, timeoutMs) ) {
        log( "ItemEncrypt ERROR: Process timed out; stderr: " + p->readAllStandardError(), LogError );
        return false;
    }

    const int exitCode = p->exitCode();
    if ( p->exitStatus() != QProcess::NormalExit ) {
        log( "ItemEncrypt ERROR: Failed to run GnuPG: " + p->errorString(), LogError );
        return false;
    }

    if (exitCode != 0) {
        const QString errors = p->readAllStandardError();
        if ( !errors.isEmpty() )
            log( "ItemEncrypt ERROR: GnuPG stderr:\n" + errors, LogError );
        return false;
    }

    return true;
}

QString getGpgVersionOutput(const QString &executable) {
    QProcess p;
    p.start(executable, QStringList("--version"), QIODevice::ReadWrite);
    p.closeReadChannel(QProcess::StandardError);

    if ( !verifyProcess(&p, 5000) )
        return QString();

    return p.readAllStandardOutput();
}

bool checkGpgExecutable(const QString &executable)
{
    const auto versionOutput = getGpgVersionOutput(executable);
    return versionOutput.contains(" 2.");
}

#ifdef Q_OS_WIN
bool checkUnixGpg(const QString &executable)
{
    static const auto unixGpg = getGpgVersionOutput(executable).contains("Home: /c/");
    return unixGpg;
}
#endif

QString findGpgExecutable()
{
    for (const auto &executable : {"gpg2", "gpg"}) {
        if ( checkGpgExecutable(executable) )
            return executable;
    }

    return QString();
}

const QString &gpgExecutable()
{
    static const auto gpg = findGpgExecutable();
    return gpg;
}

struct KeyPairPaths {
    KeyPairPaths()
    {
        const QString path = getConfigurationFilePath("");
        sec = QDir::toNativeSeparators(path + ".sec");
        pub = QDir::toNativeSeparators(path + ".pub");

#ifdef Q_OS_WIN
        if (checkUnixGpg(gpgExecutable())) {
            pub = QDir::fromNativeSeparators(pub).replace(":", "").insert(0, '/');
            sec = QDir::fromNativeSeparators(sec).replace(":", "").insert(0, '/');
        }
#endif
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

void startGpgProcess(QProcess *p, const QStringList &args, QIODevice::OpenModeFlag mode)
{
    KeyPairPaths keys;
    p->start(gpgExecutable(), getDefaultEncryptCommandArguments(keys.pub) + args, mode);
}

QString importGpgKey()
{
    KeyPairPaths keys;

    QProcess p;
    p.start(gpgExecutable(), getDefaultEncryptCommandArguments(keys.pub) << "--import" << keys.sec);
    if ( !verifyProcess(&p) )
        return "Failed to import private key (see log).";

    return QString();
}

QString exportGpgKey()
{
    KeyPairPaths keys;

    // Private key already created or exported.
    if ( QFile::exists(keys.sec) )
        return QString();

    QProcess p;
    p.start(gpgExecutable(), getDefaultEncryptCommandArguments(keys.pub) << "--export-secret-key" << "copyq");
    if ( !verifyProcess(&p) )
        return "Failed to export private key (see log).";

    QFile secKey(keys.sec);
    if ( !secKey.open(QIODevice::WriteOnly) )
        return "Failed to create private key.";

    if ( !secKey.setPermissions(QFile::ReadOwner | QFile::WriteOwner) )
        return "Failed to set permissions for private key.";

    const QByteArray secKeyData = p.readAllStandardOutput();
    secKey.write(secKeyData);
    secKey.close();

    return QString();
}

QByteArray readGpgOutput(const QStringList &args, const QByteArray &input = QByteArray())
{
    QProcess p;
    startGpgProcess(&p, args, QIODevice::ReadWrite);
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

bool decryptMimeData(QVariantMap *data)
{
    const QByteArray encryptedBytes = data->take(mimeEncryptedData).toByteArray();
    const QByteArray bytes = readGpgOutput( QStringList() << "--decrypt", encryptedBytes );
    if ( bytes.isEmpty() )
        return false;

    return deserializeData(data, bytes);
}

bool encryptMimeData(const QVariantMap &data, const QModelIndex &index, QAbstractItemModel *model)
{
    QVariantMap dataToEncrypt;
    QVariantMap dataMap;
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        if ( it.key().startsWith(COPYQ_MIME_PREFIX) )
            dataMap.insert(it.key(), it.value());
        else
            dataToEncrypt.insert(it.key(), it.value());
    }

    if ( dataToEncrypt.isEmpty() )
        return false;

    const QByteArray bytes = serializeData(dataToEncrypt);
    const QByteArray encryptedBytes = readGpgOutput( QStringList("--encrypt"), bytes );
    if ( encryptedBytes.isEmpty() )
        return false;

    dataMap.insert(mimeEncryptedData, encryptedBytes);
    return model->setData(index, dataMap, contentType::updateData);
}

void startGenerateKeysProcess(QProcess *process, bool useTransientPasswordlessKey = false)
{
    const KeyPairPaths keys;

    auto args = QStringList() << "--batch" << "--gen-key";

    QByteArray transientOptions;
    if (useTransientPasswordlessKey) {
        args << "--debug-quick-random";
        transientOptions =
                "\n%no-protection"
                "\n%transient-key";
    }

    startGpgProcess(process, args, QIODevice::ReadWrite);
    process->write( "\nKey-Type: RSA"
             "\nKey-Usage: encrypt"
             "\nKey-Length: 2048"
             "\nName-Real: copyq"
             + transientOptions +
             "\n%secring " + keys.sec.toUtf8() +
             "\n%pubring " + keys.pub.toUtf8() +
             "\n%commit"
             "\n" );
    process->closeWriteChannel();
}

QString exportImportGpgKeys()
{
    const auto error = exportGpgKey();
    if ( !error.isEmpty() )
        return error;

    return importGpgKey();
}

bool isGpgInstalled()
{
    return !gpgExecutable().isEmpty();
}

} // namespace

ItemEncrypted::ItemEncrypted(QWidget *parent)
    : QWidget(parent)
    , ItemWidget(this)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Show small icon.
    QWidget *iconWidget = new IconWidget(IconLock, this);
    layout->addWidget(iconWidget);
}

bool ItemEncryptedSaver::saveItems(const QString &, const QAbstractItemModel &model, QIODevice *file)
{
    const auto length = model.rowCount();
    if (length == 0)
        return false; // No need to encode empty tab.

    QByteArray bytes;

    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_4_7);

        stream << static_cast<quint64>(length);

        for (int i = 0; i < length && stream.status() == QDataStream::Ok; ++i) {
            QModelIndex index = model.index(i, 0);
            const QVariantMap dataMap = index.data(contentType::data).toMap();
            stream << dataMap;
        }
    }

    bytes = readGpgOutput(QStringList("--encrypt"), bytes);
    if ( bytes.isEmpty() ) {
        emitEncryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to read encrypted data");
        return false;
    }

    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);
    stream << QString(dataFileHeaderV2);
    stream.writeRawData( bytes.data(), bytes.size() );

    if ( stream.status() != QDataStream::Ok ) {
        emitEncryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to write encrypted data");
        return false;
    }

    return true;
}

void ItemEncryptedSaver::emitEncryptFailed()
{
    emit error( ItemEncryptedLoader::tr("Encryption failed!") );
}

bool ItemEncryptedScriptable::isEncrypted()
{
    const auto args = currentArguments();
    for (const auto &arg : args) {
        bool ok;
        const int row = arg.toInt(&ok);
        if (ok) {
            const auto result = call("read", QVariantList() << "?" << row);
            if ( result.toByteArray().contains(mimeEncryptedData.data()) )
                return true;
        }
    }

    return false;
}

QByteArray ItemEncryptedScriptable::encrypt()
{
    const auto args = currentArguments();
    const auto bytes = args.value(0).toByteArray();
    return encrypt(bytes);
}

QByteArray ItemEncryptedScriptable::decrypt()
{
    const auto args = currentArguments();
    const auto bytes = args.value(0).toByteArray();
    return decrypt(bytes);
}

void ItemEncryptedScriptable::encryptItem()
{
    QVariantMap dataMap;
    const auto formats = call("dataFormats").toList();
    for (const auto &formatValue : formats) {
        const auto format = formatValue.toString();
        if ( !format.startsWith(COPYQ_MIME_PREFIX) ) {
            const auto data = call("data", QVariantList() << format).toByteArray();
            dataMap.insert(format, data);
        }
    }

    const auto bytes = call("pack", QVariantList() << dataMap).toByteArray();
    const auto encryptedBytes = encrypt(bytes);
    if (encryptedBytes.isEmpty())
        return;

    call("setData", QVariantList() << mimeEncryptedData << encryptedBytes);

    for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it)
        call("removeData", QVariantList() << it.key());
}

void ItemEncryptedScriptable::decryptItem()
{
    const auto encryptedBytes = call("data", QVariantList() << mimeEncryptedData).toByteArray();
    const auto itemData = decrypt(encryptedBytes);
    if (itemData.isEmpty())
        return;

    const auto dataMap = call("unpack", QVariantList() << itemData).toMap();
    for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it) {
        const auto &format = it.key();
        call("setData", QVariantList() << format << dataMap[format]);
    }
}

void ItemEncryptedScriptable::encryptItems()
{
    const auto dataValueList = call("selectedItemsData").toList();

    QVariantList dataList;
    for (const auto &itemDataValue : dataValueList) {
        auto itemData = itemDataValue.toMap();

        QVariantMap itemDataToEncrypt;
        const auto formats = itemData.keys();
        for (const auto &format : formats) {
            if ( !format.startsWith(COPYQ_MIME_PREFIX) ) {
                itemDataToEncrypt.insert(format, itemData[format]);
                itemData.remove(format);
            }
        }

        const auto bytes = call("pack", QVariantList() << itemDataToEncrypt).toByteArray();
        const auto encryptedBytes = encrypt(bytes);
        if (encryptedBytes.isEmpty())
            return;
        itemData.insert(mimeEncryptedData, encryptedBytes);

        dataList.append(itemData);
    }

    call( "setSelectedItemsData", QVariantList() << QVariant(dataList) );
}

void ItemEncryptedScriptable::decryptItems()
{
    const auto dataValueList = call("selectedItemsData").toList();

    QVariantList dataList;
    for (const auto &itemDataValue : dataValueList) {
        auto itemData = itemDataValue.toMap();

        const auto encryptedBytes = itemData.value(mimeEncryptedData).toByteArray();
        if ( !encryptedBytes.isEmpty() ) {
            itemData.remove(mimeEncryptedData);

            const auto decryptedBytes = decrypt(encryptedBytes);
            if (decryptedBytes.isEmpty())
                return;

            const auto decryptedItemData = call("unpack", QVariantList() << decryptedBytes).toMap();
            for (auto it = decryptedItemData.constBegin(); it != decryptedItemData.constEnd(); ++it)
                itemData.insert(it.key(), it.value());
        }

        dataList.append(itemData);
    }

    call( "setSelectedItemsData", QVariantList() << QVariant(dataList) );
}

void ItemEncryptedScriptable::copyEncryptedItems()
{
    const auto dataValueList = call("selectedItemsData").toList();
    QString text;
    for (const auto &dataValue : dataValueList) {
        if ( !text.isEmpty() )
            text.append('\n');

        const auto data = dataValue.toMap();
        const auto itemTextValue = data.value(mimeText);
        if ( itemTextValue.isValid() ) {
            text.append( getTextData(itemTextValue.toByteArray()) );
        } else {
            const auto encryptedBytes = data.value(mimeEncryptedData).toByteArray();
            if ( !encryptedBytes.isEmpty() ) {
                const auto itemData = decrypt(encryptedBytes);
                if (itemData.isEmpty())
                    return;
                const auto dataMap = call("unpack", QVariantList() << itemData).toMap();
                text.append( getTextData(dataMap) );
            }
        }
    }

    const auto args = QVariantList()
            << mimeText << text
            << mimeHidden << "1";
    call("copy", args);
    call("copySelection", args);
}

void ItemEncryptedScriptable::pasteEncryptedItems()
{
    copyEncryptedItems();
    const auto script =
        R"(
        if (focused()) {
            hide();
            sleep(100);
        }
        paste();
        sleep(2000);
        copy('');
        copySelection('');
        )";
    call("eval", QVariantList() << script);
}

QString ItemEncryptedScriptable::generateTestKeys()
{
    const KeyPairPaths keys;
    for ( const auto &keyFileName : {keys.sec, keys.pub} ) {
        if ( QFile::exists(keyFileName) && !QFile::remove(keyFileName) )
            return QString("Failed to remove \"%1\"").arg(keys.sec);
    }

    QProcess process;
    startGenerateKeysProcess(&process, true);

    if ( !verifyProcess(&process) ) {
        return QString("ItemEncrypt ERROR: %1; stderr: %2")
                .arg( process.errorString(),
                      QString::fromUtf8(process.readAllStandardError()) );
    }

    const auto error = exportImportGpgKeys();
    if ( !error.isEmpty() )
        return error;

    for ( const auto &keyFileName : {keys.sec, keys.pub} ) {
        if ( !QFile::exists(keyFileName) )
            return QString("Failed to create \"%1\"").arg(keys.sec);
    }

    return QString();
}

bool ItemEncryptedScriptable::isGpgInstalled()
{
    return ::isGpgInstalled();
}

QByteArray ItemEncryptedScriptable::encrypt(const QByteArray &bytes)
{
    const auto encryptedBytes = readGpgOutput(QStringList("--encrypt"), bytes);
    if ( encryptedBytes.isEmpty() )
        throwError("Failed to execute GPG!");
    return encryptedBytes;
}

QByteArray ItemEncryptedScriptable::decrypt(const QByteArray &bytes)
{
    importGpgKey();

    const auto decryptedBytes = readGpgOutput(QStringList("--decrypt"), bytes);
    if ( decryptedBytes.isEmpty() )
        throwError("Failed to execute GPG!");
    return decryptedBytes;
}

ItemEncryptedLoader::ItemEncryptedLoader()
    : ui()
    , m_gpgProcessStatus(GpgCheckIfInstalled)
    , m_gpgProcess(nullptr)
{
}

ItemEncryptedLoader::~ItemEncryptedLoader()
{
    terminateGpgProcess();
}

ItemWidget *ItemEncryptedLoader::create(const QVariantMap &data, QWidget *parent, bool) const
{
    if ( data.value(mimeHidden).toBool() )
        return nullptr;

    return data.contains(mimeEncryptedData) ? new ItemEncrypted(parent) : nullptr;
}

QStringList ItemEncryptedLoader::formatsToSave() const
{
    return QStringList(mimeEncryptedData);
}

void ItemEncryptedLoader::applySettings(QSettings &settings)
{
    Q_ASSERT(ui != nullptr);
    settings.setValue( configEncryptTabs, ui->plainTextEditEncryptTabs->toPlainText().split('\n') );
}

void ItemEncryptedLoader::loadSettings(const QSettings &settings)
{
    m_encryptTabs = settings.value(configEncryptTabs).toStringList();
}

QWidget *ItemEncryptedLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemEncryptedSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    ui->plainTextEditEncryptTabs->setPlainText(
        m_encryptTabs.join('\n') );

    if (status() != GpgNotInstalled) {
        KeyPairPaths keys;
        ui->labelShareInfo->setTextFormat(Qt::RichText);
        ui->labelShareInfo->setText( ItemEncryptedLoader::tr(
                    "To share encrypted items on other computer or"
                    " session, you'll need public and secret key files:"
                    "<ul>"
                    "<li>%1</li>"
                    "<li>%2<br />(Keep this secret key in a safe place.)</li>"
                    "</ul>"
                    )
                .arg( quoteString(keys.pub),
                    quoteString(keys.sec) )
                );
    }

    updateUi();

    connect( ui->pushButtonPassword, &QAbstractButton::clicked,
             this, &ItemEncryptedLoader::setPassword );

    return w;
}

bool ItemEncryptedLoader::canLoadItems(QIODevice *file) const
{
    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);

    QString header;
    stream >> header;

    return stream.status() == QDataStream::Ok
            && (header == dataFileHeader || header == dataFileHeaderV2);
}

bool ItemEncryptedLoader::canSaveItems(const QString &tabName) const
{
    for (const auto &encryptTabName : m_encryptTabs) {
        if ( encryptTabName.isEmpty() )
            continue;

        QString tabName1 = tabName;

        // Ignore ampersands (usually just for underlining mnemonics) if none is specified.
        if ( !hasKeyHint(encryptTabName) )
            removeKeyHint(&tabName1);

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

ItemSaverPtr ItemEncryptedLoader::loadItems(const QString &, QAbstractItemModel *model, QIODevice *file, int maxItems)
{
    // This is needed to skip header.
    if ( !canLoadItems(file) )
        return nullptr;

    if (status() == GpgNotInstalled) {
        emit error( ItemEncryptedLoader::tr("GnuPG must be installed to view encrypted tabs.") );
        return nullptr;
    }

    importGpgKey();

    QProcess p;
    startGpgProcess( &p, QStringList("--decrypt"), QIODevice::ReadWrite );

    char encryptedBytes[4096];

    QDataStream stream(file);
    while ( !stream.atEnd() ) {
        const int bytesRead = stream.readRawData(encryptedBytes, 4096);
        if (bytesRead == -1) {
            emitDecryptFailed();
            COPYQ_LOG("ItemEncrypted ERROR: Failed to read encrypted data");
            return nullptr;
        }
        p.write(encryptedBytes, bytesRead);
    }

    p.closeWriteChannel();

    // Wait for password entry dialog.
    p.waitForFinished(-1);

    if ( !verifyProcess(&p) ) {
        emitDecryptFailed();
        return nullptr;
    }

    const QByteArray bytes = p.readAllStandardOutput();
    if ( bytes.isEmpty() ) {
        emitDecryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to read encrypted data.");
        verifyProcess(&p);
        return nullptr;
    }

    QDataStream stream2(bytes);

    quint64 length;
    stream2 >> length;
    if ( length <= 0 || stream2.status() != QDataStream::Ok ) {
        emitDecryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to parse item count!");
        return nullptr;
    }
    length = qMin(length, static_cast<quint64>(maxItems)) - static_cast<quint64>(model->rowCount());

    const auto count = length < maxItemCount ? static_cast<int>(length) : maxItemCount;
    for ( int i = 0; i < count && stream2.status() == QDataStream::Ok; ++i ) {
        if ( !model->insertRow(i) ) {
            emitDecryptFailed();
            COPYQ_LOG("ItemEncrypt ERROR: Failed to insert item!");
            return nullptr;
        }
        QVariantMap dataMap;
        stream2 >> dataMap;
        model->setData( model->index(i, 0), dataMap, contentType::data );
    }

    if ( stream2.status() != QDataStream::Ok ) {
        emitDecryptFailed();
        COPYQ_LOG("ItemEncrypt ERROR: Failed to decrypt item!");
        return nullptr;
    }

    return createSaver();
}

ItemSaverPtr ItemEncryptedLoader::initializeTab(const QString &, QAbstractItemModel *, int)
{
    if (status() == GpgNotInstalled)
        return nullptr;

    return createSaver();
}

QObject *ItemEncryptedLoader::tests(const TestInterfacePtr &test) const
{
#ifdef HAS_TESTS
    QObject *tests = new ItemEncryptedTests(test);
    return tests;
#else
    Q_UNUSED(test)
    return nullptr;
#endif
}

ItemScriptable *ItemEncryptedLoader::scriptableObject()
{
    return new ItemEncryptedScriptable();
}

QVector<Command> ItemEncryptedLoader::commands() const
{
    if ( status() == GpgNotInstalled || !keysExist() )
        return QVector<Command>();

    QVector<Command> commands;

    Command c;
    c.internalId = QStringLiteral("copyq_encrypted_encrypt");
    c.name = ItemEncryptedLoader::tr("Encrypt (needs GnuPG)");
    c.icon = QString(QChar(IconLock));
    c.input = "!OUTPUT";
    c.output = mimeEncryptedData;
    c.inMenu = true;
    c.cmd = "copyq: plugins.itemencrypted.encryptItems()";
    c.shortcuts.append( toPortableShortcutText(ItemEncryptedLoader::tr("Ctrl+L")) );
    commands.append(c);

    c = Command();
    c.internalId = QStringLiteral("copyq_encrypted_decrypt");
    c.name = ItemEncryptedLoader::tr("Decrypt");
    c.icon = QString(QChar(IconUnlock));
    c.input = mimeEncryptedData;
    c.output = mimeItems;
    c.inMenu = true;
    c.cmd = "copyq: plugins.itemencrypted.decryptItems()";
    c.shortcuts.append( toPortableShortcutText(ItemEncryptedLoader::tr("Ctrl+L")) );
    commands.append(c);

    c = Command();
    c.internalId = QStringLiteral("copyq_encrypted_decrypt_and_copy");
    c.name = ItemEncryptedLoader::tr("Decrypt and Copy");
    c.icon = QString(QChar(IconUnlockAlt));
    c.input = mimeEncryptedData;
    c.inMenu = true;
    c.cmd = "copyq: plugins.itemencrypted.copyEncryptedItems()";
    c.shortcuts.append( toPortableShortcutText(ItemEncryptedLoader::tr("Ctrl+Shift+L")) );
    commands.append(c);

    c = Command();
    c.internalId = QStringLiteral("copyq_encrypted_decrypt_and_paste");
    c.name = ItemEncryptedLoader::tr("Decrypt and Paste");
    c.icon = QString(QChar(IconUnlockAlt));
    c.input = mimeEncryptedData;
    c.inMenu = true;
    c.cmd = "copyq: plugins.itemencrypted.pasteEncryptedItems()";
    c.shortcuts.append( toPortableShortcutText(ItemEncryptedLoader::tr("Enter")) );
    commands.append(c);

    return commands;
}

void ItemEncryptedLoader::setPassword()
{
    if (status() != GpgNotRunning)
        return;

    if ( !keysExist() ) {
        m_gpgProcessStatus = GpgGeneratingKeys;
        m_gpgProcess = new QProcess(this);
        startGenerateKeysProcess(m_gpgProcess);
    } else {
        // Change password.
        m_gpgProcessStatus = GpgChangingPassword;
        m_gpgProcess = new QProcess(this);
        startGpgProcess( m_gpgProcess, QStringList() << "--edit-key" << "copyq" << "passwd" << "save", QIODevice::ReadOnly );
    }

    m_gpgProcess->waitForStarted();
    if ( m_gpgProcess->state() == QProcess::NotRunning ) {
        onGpgProcessFinished( m_gpgProcess->exitCode(), m_gpgProcess->exitStatus() );
    } else {
        connectProcessFinished(m_gpgProcess, this, &ItemEncryptedLoader::onGpgProcessFinished);
        updateUi();
    }
}

void ItemEncryptedLoader::terminateGpgProcess()
{
    if (m_gpgProcess == nullptr)
        return;
    QProcess *p = m_gpgProcess;
    m_gpgProcess = nullptr;
    p->terminate();
    p->waitForFinished();
    p->deleteLater();
    m_gpgProcessStatus = GpgNotRunning;
    updateUi();
}

void ItemEncryptedLoader::onGpgProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString error;

    if (m_gpgProcess != nullptr) {
        if (ui != nullptr) {
            if (exitStatus != QProcess::NormalExit)
                error = m_gpgProcess->errorString();
            else if (exitCode != 0)
                error = getTextData(m_gpgProcess->readAllStandardError());
            else if ( m_gpgProcess->error() != QProcess::UnknownError )
                error = m_gpgProcess->errorString();
            else if ( !keysExist() )
                error = ItemEncryptedLoader::tr("Failed to generate keys.");
        }

        m_gpgProcess->deleteLater();
        m_gpgProcess = nullptr;
    }

    // Export and import private key to a file in configuration.
    if ( status() == GpgGeneratingKeys && error.isEmpty() )
        error = exportImportGpgKeys();

    if (!error.isEmpty())
        error = ItemEncryptedLoader::tr("Error: %1").arg(error);

    m_gpgProcessStatus = GpgNotRunning;

    updateUi();
    ui->labelInfo->setText( error.isEmpty() ? ItemEncryptedLoader::tr("Done") : error );
}

void ItemEncryptedLoader::updateUi()
{
    if (ui == nullptr)
        return;

    if (status() == GpgNotInstalled) {
        ui->labelInfo->setText("To use item encryption, install"
                               " <a href=\"http://www.gnupg.org/\">GnuPG</a>"
                               " application and restart CopyQ.");
        ui->pushButtonPassword->hide();
        ui->groupBoxEncryptTabs->hide();
        ui->groupBoxShareInfo->hide();
    } else if (status() == GpgGeneratingKeys) {
        ui->labelInfo->setText( ItemEncryptedLoader::tr("Creating new keys (this may take a few minutes)...") );
        ui->pushButtonPassword->setText( ItemEncryptedLoader::tr("Cancel") );
    } else if (status() == GpgChangingPassword) {
        ui->labelInfo->setText( ItemEncryptedLoader::tr("Setting new password...") );
        ui->pushButtonPassword->setText( ItemEncryptedLoader::tr("Cancel") );
    } else if ( !keysExist() ) {
        ui->labelInfo->setText( ItemEncryptedLoader::tr(
                    "Encryption keys <strong>must be generated</strong>"
                    " before item encryption can be used.") );
        ui->pushButtonPassword->setText( ItemEncryptedLoader::tr("Generate New Keys...") );
    } else {
        ui->pushButtonPassword->setText( ItemEncryptedLoader::tr("Change Password...") );
    }
}

void ItemEncryptedLoader::emitDecryptFailed()
{
    emit error( ItemEncryptedLoader::tr("Decryption failed!") );
}

ItemSaverPtr ItemEncryptedLoader::createSaver()
{
    auto saver = std::make_shared<ItemEncryptedSaver>();
    connect( saver.get(), &ItemEncryptedSaver::error,
             this, &ItemEncryptedLoader::error );
    return saver;
}

ItemEncryptedLoader::GpgProcessStatus ItemEncryptedLoader::status() const
{
    if (m_gpgProcessStatus == GpgCheckIfInstalled) {
        if (isGpgInstalled())
            m_gpgProcessStatus = GpgNotRunning;
        else
            m_gpgProcessStatus = GpgNotInstalled;
    }

    return m_gpgProcessStatus;
}

bool ItemEncryptedLoader::data(QVariantMap *data, const QModelIndex &) const
{
    return !data->contains(mimeEncryptedData) || decryptMimeData(data);
}

bool ItemEncryptedLoader::setData(const QVariantMap &data, const QModelIndex &index, QAbstractItemModel *model) const
{
    if ( !index.data(contentType::data).toMap().contains(mimeEncryptedData) )
        return false;

    return encryptMimeData(data, index, model);
}
