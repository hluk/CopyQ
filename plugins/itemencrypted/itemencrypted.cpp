// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <QVariantMap>

namespace {

const QLatin1String mimeEncryptedData("application/x-copyq-encrypted");

const QLatin1String dataFileHeader("CopyQ_encrypted_tab");
const QLatin1String dataFileHeaderV2("CopyQ_encrypted_tab v2");

const QLatin1String configEncryptTabs("encrypt_tabs");

const int maxItemCount = 100'000;

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
        log( QStringLiteral("ItemEncrypt: Process timed out; stderr: %1")
             .arg(QString::fromUtf8(p->readAllStandardError())), LogError );
        return false;
    }

    const int exitCode = p->exitCode();
    if ( p->exitStatus() != QProcess::NormalExit ) {
        log( QStringLiteral("ItemEncrypt: Failed to run GnuPG: %1")
             .arg(p->errorString()), LogError );
        return false;
    }

    if (exitCode != 0) {
        if (const QString errors = p->readAllStandardError(); !errors.isEmpty()) {
            log( QStringLiteral("ItemEncrypt: GnuPG stderr:\n%1")
                 .arg(errors), LogError );
        }
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

struct GpgVersion {
    int major;
    int minor;
};

GpgVersion parseVersion(const QString &versionOutput)
{
    const int lineEndIndex = versionOutput.indexOf('\n');
#if QT_VERSION < QT_VERSION_CHECK(5,15,2)
    const QStringRef firstLine = versionOutput.midRef(0, lineEndIndex);
#else
    const auto firstLine = QStringView{versionOutput}.mid(0, lineEndIndex);
#endif
    const QRegularExpression versionRegex(QStringLiteral(R"( (\d+)\.(\d+))"));
    const QRegularExpressionMatch match = versionRegex.match(firstLine);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    const int major = match.hasMatch() ? match.capturedView(1).toInt() : 0;
    const int minor = match.hasMatch() ? match.capturedView(2).toInt() : 0;
#else
    const int major = match.hasMatch() ? match.capturedRef(1).toInt() : 0;
    const int minor = match.hasMatch() ? match.capturedRef(2).toInt() : 0;
#endif
    return GpgVersion{major, minor};
}

class GpgExecutable {
public:
    GpgExecutable() = default;

    explicit GpgExecutable(const QString &executable)
        : m_executable(executable)
    {
        const auto versionOutput = getGpgVersionOutput(executable);
        if ( !versionOutput.isEmpty() ) {
            COPYQ_LOG_VERBOSE(
                QStringLiteral("ItemEncrypt INFO: '%1 --version' output: %2")
                .arg(executable, versionOutput) );

            const GpgVersion version = parseVersion(versionOutput);
            m_isSupported = version.major >= 2;
            COPYQ_LOG( QStringLiteral("ItemEncrypt INFO: %1 gpg version: %2.%3")
                    .arg(m_isSupported ? "Supported" : "Unsupported")
                    .arg(version.major)
                    .arg(version.minor) );

            const bool needsSecring = version.major == 2 && version.minor == 0;

            const QString path = getConfigurationFilePath("");
            m_pubring = path + ".pub";
            m_pubringNative = QDir::toNativeSeparators(m_pubring);
            if (needsSecring) {
                m_secring = path + ".sec";
                m_secringNative = QDir::toNativeSeparators(m_secring);
            }

#ifdef Q_OS_WIN
            const bool isUnixGpg = versionOutput.contains("Home: /c/");
            if (isUnixGpg) {
                m_pubringNative = QString(m_pubring).replace(":", "").insert(0, '/');
                if (needsSecring)
                    m_secringNative = QString(m_secring).replace(":", "").insert(0, '/');
            }
#endif
        }
    }

    const QString &executable() const { return m_executable; }
    bool isSupported() const { return m_isSupported; }
    bool needsSecring() const { return !m_secring.isEmpty(); }
    const QString &pubring() const { return m_pubring; }
    const QString &secring() const { return m_secring; }
    const QString &pubringNative() const { return m_pubringNative; }
    const QString &secringNative() const { return m_secringNative; }

private:
    QString m_executable;
    QString m_pubring;
    QString m_secring;
    QString m_pubringNative;
    QString m_secringNative;
    bool m_isSupported = false;
};

GpgExecutable findGpgExecutable()
{
    for (const auto &executable : {"gpg2", "gpg"}) {
        GpgExecutable gpg(executable);
        if ( gpg.isSupported() )
            return gpg;
    }

    return GpgExecutable();
}

const GpgExecutable &gpgExecutable()
{
    static const auto gpg = findGpgExecutable();
    return gpg;
}

QStringList getDefaultEncryptCommandArguments(const QString &publicKeyPath)
{
    return QStringList() << "--trust-model" << "always" << "--recipient" << "copyq"
                         << "--charset" << "utf-8" << "--display-charset" << "utf-8" << "--no-tty"
                         << "--no-default-keyring" << "--keyring" << publicKeyPath;
}

void startGpgProcess(QProcess *p, const QStringList &args, QIODevice::OpenModeFlag mode)
{
    const auto &gpg = gpgExecutable();
    p->start(gpg.executable(), getDefaultEncryptCommandArguments(gpg.pubringNative()) + args, mode);
}

QString importGpgKey()
{
    const auto &gpg = gpgExecutable();
    if ( !gpg.needsSecring() )
        return QString();

    QProcess p;
    p.start(gpg.executable(), getDefaultEncryptCommandArguments(gpg.pubringNative()) << "--import" << gpg.secringNative());
    if ( !verifyProcess(&p) )
        return "Failed to import private key (see log).";

    return QString();
}

QString exportGpgKey()
{
    const auto &gpg = gpgExecutable();
    if ( !gpg.needsSecring() )
        return QString();

    // Private key already created or exported.
    if ( QFile::exists(gpg.secring()) )
        return QString();

    QProcess p;
    p.start(gpg.executable(), getDefaultEncryptCommandArguments(gpg.pubringNative()) << "--export-secret-key" << gpg.secringNative());
    if ( !verifyProcess(&p) )
        return "Failed to export private key (see log).";

    QFile secKey(gpg.secring());
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
    const auto &gpg = gpgExecutable();

    auto args = QStringList() << "--batch" << "--gen-key";

    QByteArray transientOptions;
    if (useTransientPasswordlessKey) {
        args << "--debug-quick-random";
        transientOptions =
                "\n%no-protection"
                "\n%transient-key";
    }

    startGpgProcess(process, args, QIODevice::ReadWrite);
    process->write(
        "\nKey-Type: RSA"
        "\nKey-Usage: encrypt"
        "\nKey-Length: 4096"
        "\nName-Real: copyq"
        + transientOptions +
        "\n%pubring " + gpg.pubringNative().toUtf8()
    );

    if ( gpg.needsSecring() )
        process->write("\n%secring " + gpg.secringNative().toUtf8());

    process->write("\n%commit\n");
    process->closeWriteChannel();
}

QString exportImportGpgKeys()
{
    if (const auto error = exportGpgKey(); !error.isEmpty())
        return error;

    return importGpgKey();
}

bool isGpgInstalled()
{
    return gpgExecutable().isSupported();
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
    QByteArray bytes;

    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_4_7);

        stream << static_cast<quint64>(length);

        for (int i = 0; i < length && stream.status() == QDataStream::Ok; ++i) {
            QModelIndex index = model.index(i, 0);
            QVariantMap dataMap = index.data(contentType::data).toMap();
            for (auto it = dataMap.begin(); it != dataMap.end(); ++it) {
                if (it.value().type() != QVariant::ByteArray)
                    it.value() = it.value().toByteArray();
            }
            stream << dataMap;
        }
    }

    bytes = readGpgOutput(QStringList("--encrypt"), bytes);
    if ( bytes.isEmpty() ) {
        emitEncryptFailed();
        log("ItemEncrypt: Failed to read encrypted data", LogError);
        return false;
    }

    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);
    stream << QString(dataFileHeaderV2);
    stream.writeRawData( bytes.data(), bytes.size() );

    if ( stream.status() != QDataStream::Ok ) {
        emitEncryptFailed();
        log("ItemEncrypt: Failed to write encrypted data", LogError);
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
    const auto &gpg = gpgExecutable();

    const QStringList keys = gpg.needsSecring()
        ? QStringList{gpg.pubring(), gpg.secring()}
        : QStringList{gpg.pubring()};

    for (const auto &keyFileName : keys) {
        if ( QFile::exists(keyFileName) && !QFile::remove(keyFileName) )
            return QString("Failed to remove \"%1\"").arg(keyFileName);
    }

    QProcess process;
    startGenerateKeysProcess(&process, true);

    if ( !verifyProcess(&process) ) {
        return QString("ItemEncrypt: %1; stderr: %2")
                .arg( process.errorString(),
                      QString::fromUtf8(process.readAllStandardError()) );
    }

    if ( const auto error = exportImportGpgKeys(); !error.isEmpty() )
        return error;

    for (const auto &keyFileName : keys) {
        if ( !QFile::exists(keyFileName) )
            return QString("Failed to create \"%1\"").arg(keyFileName);
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
        const auto &gpg = gpgExecutable();
        ui->labelShareInfo->setTextFormat(Qt::RichText);
        QString text = ItemEncryptedLoader::tr(
            "To share encrypted items on other computer or"
            " session, you'll need these secret key files (keep them in a safe place):"
        );
        if (gpg.needsSecring()) {
            text.append( QStringLiteral(
                "<ul>"
                "<li>%1</li>"
                "<li>%2</li>"
                "</ul>"
                ).arg(quoteString(gpg.pubringNative()), quoteString(gpg.secringNative()))
            );
        } else {
            text.append( QStringLiteral(
                "<ul>"
                "<li>%1</li>"
                "</ul>"
                ).arg(quoteString(gpg.pubringNative()))
            );
        }
        ui->labelShareInfo->setText(text);
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
            log("ItemEncrypted: Failed to read encrypted data", LogError);
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
        log("ItemEncrypt: Failed to read encrypted data", LogError);
        verifyProcess(&p);
        return nullptr;
    }

    QDataStream stream2(bytes);

    quint64 length;
    stream2 >> length;
    if ( stream2.status() != QDataStream::Ok ) {
        emitDecryptFailed();
        log("ItemEncrypt: Failed to parse item count", LogError);
        return nullptr;
    }
    length = qMin(length, static_cast<quint64>(maxItems)) - static_cast<quint64>(model->rowCount());

    const auto count = length < maxItemCount ? static_cast<int>(length) : maxItemCount;
    for ( int i = 0; i < count && stream2.status() == QDataStream::Ok; ++i ) {
        if ( !model->insertRow(i) ) {
            emitDecryptFailed();
            log("ItemEncrypt: Failed to insert item", LogError);
            return nullptr;
        }
        QVariantMap dataMap;
        stream2 >> dataMap;
        model->setData( model->index(i, 0), dataMap, contentType::data );
    }

    if ( stream2.status() != QDataStream::Ok ) {
        emitDecryptFailed();
        log("ItemEncrypt: Failed to decrypt item", LogError);
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
    c.icon = QString(QChar(IconUnlockKeyhole));
    c.input = mimeEncryptedData;
    c.inMenu = true;
    c.cmd = "copyq: plugins.itemencrypted.copyEncryptedItems()";
    c.shortcuts.append( toPortableShortcutText(ItemEncryptedLoader::tr("Ctrl+Shift+L")) );
    commands.append(c);

    c = Command();
    c.internalId = QStringLiteral("copyq_encrypted_decrypt_and_paste");
    c.name = ItemEncryptedLoader::tr("Decrypt and Paste");
    c.icon = QString(QChar(IconUnlockKeyhole));
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
