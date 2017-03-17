/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "app.h"

#include "common/log.h"
#include "common/settings.h"
#include "common/textdata.h"
#include "platform/platformnativeinterface.h"
#ifdef Q_OS_UNIX
#   include "platform/unix/unixsignalhandler.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
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
    return !qgetenv("COPYQ_TEST_ID").isEmpty();
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

    const QString session = QCoreApplication::organizationName() + ".test";
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
#ifdef HAS_TESTS
    // Keep texts and shortcuts untranslated for tests.
    if ( isTesting() )
        return;
#endif

    QString locale = QSettings().value("Options/language").toString();
    if (locale.isEmpty())
        locale = QLocale::system().name();

#ifdef COPYQ_TRANSLATION_PREFIX
    const QString translationPrefix = COPYQ_TRANSLATION_PREFIX;
#else
    const QString translationPrefix = createPlatformNativeInterface()->translationPrefix();
#endif

    QStringList translationDirectories;
    translationDirectories.prepend(translationPrefix);

    // 1. Qt translations
    installTranslator("qt_" + locale, translationPrefix);
    installTranslator("qt_" + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath));

    // 2. installed translations
    installTranslator("copyq_" + locale, translationPrefix);

    // 3. custom translations
    const QByteArray customPath = qgetenv("COPYQ_TRANSLATION_PREFIX");
    if ( !customPath.isEmpty() ) {
        const QString customDir = QDir::fromNativeSeparators( getTextData(customPath) );
        installTranslator("copyq_" + locale, customDir);
        translationDirectories.prepend(customDir);
    }

    // 4. compiled, non-installed translations in debug builds
#ifdef COPYQ_DEBUG
    const QString compiledTranslations = QCoreApplication::applicationDirPath() + "/src";
    installTranslator("copyq_" + locale, compiledTranslations);
    translationDirectories.prepend(compiledTranslations);
#endif

    qApp->setProperty("CopyQ_translation_directories", translationDirectories);

    QLocale::setDefault(QLocale(locale));
}

} // namespace

App::App(
        const QString &threadName,
        QCoreApplication *application,
        const QString &sessionName)
    : m_app(application)
    , m_exitCode(0)
    , m_started(false)
    , m_closed(false)
{
    QString session("copyq");
    if ( !sessionName.isEmpty() ) {
        session += "-" + sessionName;
        m_app->setProperty( "CopyQ_session_name", QVariant(sessionName) );
    }

    qputenv("COPYQ_SESSION_NAME", sessionName.toUtf8());
    qputenv("COPYQ", QCoreApplication::applicationFilePath().toUtf8());

    QCoreApplication::setOrganizationName(session);
    QCoreApplication::setApplicationName(session);

    if ( !threadName.isEmpty() )
        setCurrentThreadName(threadName);

#ifdef HAS_TESTS
    initTests();
#endif

#ifdef Q_OS_UNIX
    if ( !UnixSignalHandler::create(m_app) )
        log( QString("Failed to create handler for Unix signals!"), LogError );
#endif
}

App::~App()
{
    QCoreApplication::processEvents();
    App::exit();
}

void App::restoreSettings(bool canModifySettings)
{
    m_app->setProperty("CopyQ_server", canModifySettings);

    createPlatformNativeInterface()->loadSettings();

    Settings::restore();

    installTranslator();
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

    QCoreApplication::exit(exitCode);
    m_exitCode = exitCode;
    m_closed = true;
}

bool App::wasClosed() const
{
    return m_closed;
}
