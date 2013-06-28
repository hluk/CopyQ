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

#include "app.h"

#include "common/client_server.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#ifdef HAS_TESTS
#   include <QProcessEnvironment>
#endif

#ifdef Q_OS_UNIX
#   include <QSocketNotifier>
#   include <signal.h>
#   include <sys/socket.h>
#   include <unistd.h>

namespace {

int signalFd[2];

/**
 * Unix signal handler (TERM, HUP).
 */
void exitSignalHandler(int)
{
    char a = 1;
    ::write(signalFd[0], &a, sizeof(a));
}

} // namespace

#endif // Q_OS_UNIX

App::App(QCoreApplication *application, const QString &sessionName)
    : m_app(application)
    , m_exitCode(0)
    , m_closed(false)
{
    QString session("copyq");
    if ( !sessionName.isEmpty() )
        session += "-" + sessionName;

#ifdef HAS_TESTS
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if ( env.value("COPYQ_TESTING") == "1" )
        session += ".test";
#endif

    QCoreApplication::setOrganizationName(session);
    QCoreApplication::setApplicationName(session);

    const QString locale = QLocale::system().name();
    QTranslator *translator = new QTranslator(m_app);
    translator->load("qt_" + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    QCoreApplication::installTranslator(translator);

    translator = new QTranslator(m_app);
    translator->load("copyq_" + locale, ":/translations");
    QCoreApplication::installTranslator(translator);

#ifdef Q_OS_UNIX
    // Safely quit application on TERM and HUP signals.
    if ( ::socketpair(AF_UNIX, SOCK_STREAM, 0, signalFd) != 0 ) {
        log( QObject::tr("socketpair() failed!"), LogError );
    } else {
        QSocketNotifier *sn = new QSocketNotifier(signalFd[1], QSocketNotifier::Read, m_app);
        sn->connect( sn, SIGNAL(activated(int)), m_app, SLOT(quit()) );

        struct sigaction sigact;

        sigact.sa_handler = ::exitSignalHandler;
        sigemptyset(&sigact.sa_mask);
        sigact.sa_flags = 0;
        sigact.sa_flags |= SA_RESTART;

        if ( sigaction(SIGHUP, &sigact, 0) > 0 || sigaction(SIGTERM, &sigact, 0) > 0 )
            log( QObject::tr("sigaction() failed!"), LogError );
    }
#endif
}

int App::exec()
{
    if (m_closed) {
        m_app->processEvents();
        return m_exitCode;
    } else {
        return m_app->exec();
    }
}

void App::exit(int exitCode)
{
    QCoreApplication::exit(exitCode);
    m_exitCode = exitCode;
    m_closed = true;
}
