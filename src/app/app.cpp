/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "common/common.h"
#include "common/log.h"
#include "common/settings.h"
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

#ifndef COPYQ_TRANSLATION_PREFIX
#   ifdef Q_OS_WIN
#       define COPYQ_TRANSLATION_PREFIX QCoreApplication::applicationDirPath() + "/translations"
#   elif defined(Q_OS_MAC)
#       define COPYQ_TRANSLATION_PREFIX QCoreApplication::applicationDirPath() + "/../Resources/translations"
#   else
#       define COPYQ_TRANSLATION_PREFIX ""
#   endif
#endif

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

/**
 * Read base64-encoded settings from "COPYQ_TEST_SETTINGS" enviroment variable if not empty.
 *
 * The settings are initially taken from "CopyQ_test_settings" property of test object returned by
 * ItemLoaderInterface::tests().
 *
 * This function does nothing if isTesting() returns false.
 */
void initTestsSettings()
{
    if ( !isTesting() )
        return;

    const QByteArray settingsData = qgetenv("COPYQ_TEST_SETTINGS");
    if ( settingsData.isEmpty() )
        return;

    // Reset settings on first run of each test case.
    Settings settings;
    settings.clear();

    QVariant testSettings;
    const QByteArray data = QByteArray::fromBase64(settingsData);
    QDataStream input(data);
    input >> testSettings;
    const QVariantMap testSettingsMap = testSettings.toMap();

    const QString testId = qApp->property("CopyQ_test_id").toString();
    bool pluginsTest = testId != "CORE";

    if (pluginsTest) {
        settings.beginGroup("Plugins");
        settings.beginGroup(testId);
    }

    foreach (const QString &key, testSettingsMap.keys())
        settings.setValue(key, testSettingsMap[key]);

    if (pluginsTest) {
        settings.endGroup();
        settings.endGroup();
    }

    settings.setValue("CopyQ_test_id", testId);
}
#endif // HAS_TESTS

void installTranslator(const QString &filename, const QString &directory)
{
    QScopedPointer<QTranslator> translator( new QTranslator(qApp) );
    if ( translator->load(filename, directory) )
        QCoreApplication::installTranslator(translator.take());
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

    QStringList translationDirectories;
#ifdef COPYQ_TRANSLATION_PREFIX
    translationDirectories.prepend(COPYQ_TRANSLATION_PREFIX);
#endif

    // 1. Qt translations
#ifdef COPYQ_TRANSLATION_PREFIX
    installTranslator("qt_" + locale, COPYQ_TRANSLATION_PREFIX);
#endif
    installTranslator("qt_" + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath));

    // 2. installed translations
#ifdef COPYQ_TRANSLATION_PREFIX
    installTranslator("copyq_" + locale, COPYQ_TRANSLATION_PREFIX);
#endif

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

App::App(QCoreApplication *application,
        const QString &sessionName)
    : m_app(application)
    , m_exitCode(0)
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

#ifdef HAS_TESTS
    initTests();
#endif

#ifdef Q_OS_UNIX
    if ( !UnixSignalHandler::create(m_app.data()) )
        log( QString("Failed to create handler for Unix signals!"), LogError );
#endif
}

App::~App()
{
}

void App::restoreSettings(bool canModifySettings)
{
    m_app->setProperty("CopyQ_server", canModifySettings);

    createPlatformNativeInterface()->loadSettings();

    Settings::restore();

    installTranslator();

#ifdef HAS_TESTS
    initTestsSettings();
#endif
}

int App::exec()
{
    if ( wasClosed() ) {
        m_app->processEvents();
        return m_exitCode;
    }

    return m_app->exec();
}

void App::exit(int exitCode)
{
    if ( wasClosed() )
        return;

    QCoreApplication::exit(exitCode);
    m_exitCode = exitCode;
    m_closed = true;
}

bool App::wasClosed() const
{
    return m_closed;
}
