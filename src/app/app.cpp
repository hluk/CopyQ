// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"

#include "common/command.h"
#include "common/commandstore.h"
#include "common/log.h"
#include "common/settings.h"
#include "common/textdata.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"
#ifdef Q_OS_UNIX
#   include "platform/unix/unixsignalhandler.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>
#include <QVariant>

#include <memory>

namespace {

#ifdef HAS_TESTS
/**
 * Return true only this session was started from tests.
 */
bool isTesting()
{
    return !qEnvironmentVariableIsEmpty("COPYQ_TEST_ID");
}

/**
 * Change application name for tests and set "CopyQ_test_id" property of application
 * to current test ID. The ID is "CORE" for core tests and ItemLoaderInterface::id() for plugins.
 *
 * This function does nothing if isTesting() returns false.
 */
void initTests()
{
    if ( !isTesting() )
        return;

    const QString session = QStringLiteral("copyq.test");
    QCoreApplication::setOrganizationName(session);
    QCoreApplication::setApplicationName(session);

    const QString testId = getTextData( qgetenv("COPYQ_TEST_ID") );
    qApp->setProperty("CopyQ_test_id", testId);
}
#endif // HAS_TESTS

void installTranslator(const QString &filename, const QString &directory)
{
    std::unique_ptr<QTranslator> translator( new QTranslator(qApp) );
    if ( translator->load(filename, directory) )
        QCoreApplication::installTranslator(translator.release());
}

void installTranslator()
{
    QString locale = QString::fromUtf8( qgetenv("COPYQ_LOCALE") );
    if (locale.isEmpty()) {
        locale = QSettings().value(QStringLiteral("Options/language")).toString();
        if (locale.isEmpty())
            locale = QLocale::system().name();
        qputenv("COPYQ_LOCALE", locale.toUtf8());
    }

#ifdef COPYQ_TRANSLATION_PREFIX
    const QString translationPrefix = COPYQ_TRANSLATION_PREFIX;
#else
    const QString translationPrefix = platformNativeInterface()->translationPrefix();
#endif

    QStringList translationDirectories;
    translationDirectories.prepend(translationPrefix);

    // 1. Qt translations
    installTranslator(QLatin1String("qt_") + locale, translationPrefix);
    installTranslator(QLatin1String("qt_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath));

    // 2. installed translations
    installTranslator(QLatin1String("copyq_") + locale, translationPrefix);

    // 3. custom translations
    const QByteArray customPath = qgetenv("COPYQ_TRANSLATION_PREFIX");
    if ( !customPath.isEmpty() ) {
        const QString customDir = QDir::fromNativeSeparators( getTextData(customPath) );
        installTranslator(QLatin1String("copyq_") + locale, customDir);
        translationDirectories.prepend(customDir);
    }

    // 4. compiled, non-installed translations in debug builds
#ifdef COPYQ_DEBUG
    const QString compiledTranslations = QCoreApplication::applicationDirPath() + QLatin1String("/src");
    installTranslator(QLatin1String("copyq_") + locale, compiledTranslations);
    translationDirectories.prepend(compiledTranslations);
#endif

    qApp->setProperty("CopyQ_translation_directories", translationDirectories);

#ifdef HAS_TESTS
    // Keep texts and shortcuts untranslated for tests.
    if ( isTesting() )
        return;
#endif

    QLocale::setDefault(QLocale(locale));
}

} // namespace

App::App(QCoreApplication *application,
        const QString &sessionName)
    : m_app(application)
    , m_exitCode(0)
    , m_started(false)
    , m_closed(false)
{
    registerDataFileConverter();

    QObject::connect(m_app, &QCoreApplication::aboutToQuit, [this]() { exit(); });

#ifdef Q_OS_UNIX
    startUnixSignalHandler();
#endif

    QString session("copyq");
    if ( !sessionName.isEmpty() )
        session += "-" + sessionName;
    m_app->setProperty( "CopyQ_session_name", QVariant(sessionName) );

    qputenv("COPYQ_SESSION_NAME", sessionName.toUtf8());
    qputenv("COPYQ", QCoreApplication::applicationFilePath().toUtf8());

    const auto settingsPath = qgetenv("COPYQ_SETTINGS_PATH");
    if ( !settingsPath.isEmpty() ) {
        const auto path = QString::fromUtf8(settingsPath);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, path);

        // Setting the NativeFormat paths on Windows, macOS, and iOS has no effect.
        QSettings::setDefaultFormat(QSettings::IniFormat);
    }

    QCoreApplication::setOrganizationName(session);
    QCoreApplication::setApplicationName(session);

    if ( qEnvironmentVariableIsEmpty("COPYQ_ITEM_DATA_PATH") ) {
        if ( !m_app->property("CopyQ_item_data_path").isValid() ) {
            m_app->setProperty(
                "CopyQ_item_data_path",
                QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QLatin1String("/items"));
        }
    } else {
        m_app->setProperty(
            "CopyQ_item_data_path",
#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
            qEnvironmentVariable("COPYQ_ITEM_DATA_PATH")
#else
            QString::fromLocal8bit(qgetenv("COPYQ_ITEM_DATA_PATH"))
#endif
        );
    }

#ifdef HAS_TESTS
    initTests();
#endif

    initLogging();
}

App::~App()
{
    QCoreApplication::processEvents();
    App::exit();
    delete m_app;
}

void App::installTranslator()
{
    ::installTranslator();
}

int App::exec()
{
    if ( wasClosed() ) {
        m_app->processEvents();
        return m_exitCode;
    }

    m_started = true;

    return m_app->exec();
}

void App::exit(int exitCode)
{
    if ( wasClosed() )
        return;

    if (!m_started)
        ::exit(exitCode);

    m_exitCode = exitCode;
    m_closed = true;
    QCoreApplication::exit(exitCode);
}

bool App::wasClosed() const
{
    return m_closed;
}
