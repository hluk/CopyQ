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

#include "app/app.h"
#include "app/applicationexceptionhandler.h"
#include "app/clipboardclient.h"
#include "common/log.h"
#include "common/messagehandlerforqt.h"
#include "common/textdata.h"
#include "platform/platformnativeinterface.h"
#ifdef Q_OS_UNIX
#   include "platform/unix/unixsignalhandler.h"
#endif

#include <exception>

namespace {

int startClient(int argc, char *argv[], const QStringList &arguments, const QString &sessionName)
{
    ClipboardClient app(argc, argv, arguments, sessionName);
    return app.exec();
}

QString getSessionName(const QStringList &arguments, int *skipArguments)
{
    const QString firstArgument = arguments.value(0);
    *skipArguments = 0;

    if (firstArgument == "-s" || firstArgument == "--session" || firstArgument == "session") {
        *skipArguments = 2;
        return arguments.value(1);
    }

    if ( firstArgument.startsWith("--session=") ) {
        *skipArguments = 1;
        return firstArgument.mid( firstArgument.indexOf('=') + 1 );
    }

    // Skip session arguments passed from session manager.
    if (arguments.size() == 2 && firstArgument == "-session")
        *skipArguments = 2;

    return getTextData( qgetenv("COPYQ_SESSION_NAME") );
}

int startApplication(int argc, char **argv)
{
    installMessageHandlerForQt();

#ifdef Q_OS_UNIX
    if ( !initUnixSignalHandler() )
        log( QString("Failed to create handler for Unix signals!"), LogError );
#endif

    const QStringList arguments =
            platformNativeInterface()->getCommandLineArguments(argc, argv);

    // Get session name (default is empty).
    int skipArguments;
    const QString sessionName = getSessionName(arguments, &skipArguments);

    return startClient(argc, argv, arguments.mid(skipArguments), sessionName);
}

} // namespace

int main(int argc, char **argv)
{
    try {
        return startApplication(argc, argv);
    } catch (const std::exception &e) {
        logException(e.what());
        throw;
    } catch (...) {
        logException();
        throw;
    }
}
