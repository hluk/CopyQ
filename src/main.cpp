/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

#include "mainwindow.h"

#include "client_server.h"
#include "clipboardserver.h"
#include "clipboardclient.h"
#include "clipboardmonitor.h"

#include <QSettings>
#include <iostream>

#ifdef Q_WS_WIN
#include <windows.h>
#endif

static void usage()
{
    std::cout << QObject::tr(
"CopyQ Clipboard Manager v1.2.3 (hluk@email.cz)\n\n"
"usage: copyq [COMMAND]\n"
"Starts server if no command is specified.\n"
"  COMMANDs:\n"
"    show    Show main window.\n"
"    hide    Hide main window.\n"
"    toggle  Show or hide main window.\n"
"    menu    Open context menu.\n"
"    exit    Exit server.\n"
"\n"
"    clipboard [MIME_TYPE=\"text/plain\"]\n"
"      Print clipboard content.\n"
"      Use ? for mime_type to print mime types.\n"
#ifdef Q_WS_X11
"    selection [MIME_TYPE=\"text/plain\"]\n"
"      Print X11 selection content.\n"
"      Use ? for mime_type to print mime types.\n"
#endif
"\n"
"    tab\n"
"      List available tab names.\n"
"    tab <TAB_NAME> [COMMAND]\n"
"      Run command on tab with given name.\n"
"      Tab is created if it doesn't exist.\n"
"    removetab! <TAB_NAME>\n"
"      Remove tab with given name.\n"
"\n"
"    length, count, size\n"
"      Print number of items in history.\n"
"    select [ROW=0]\n"
"      Move item in the row to clipboard.\n"
"    add <TEXT> ...\n"
"      Add text into clipboard.\n"
"    remove [ROW=0] ...\n"
"      Remove item in given rows.\n"
"    edit [ROW=0] ...\n"
"      Edit clipboard item.\n"
"\n"
"    read [MIME_TYPE=\"text/plain\"|ROW] ...\n"
"      Print raw data of clipboard or item in row.\n"
"      Use ? for mime_type to print mime types.\n"
"    write <MIME_TYPE> <DATA> ...\n"
"      Write raw data to clipboard.\n"
"    - [MIME_TYPE=\"text/plain\"]\n"
"      Copy text from standard input into clipboard.\n"
"\n"
"    action [ROW=0] ...\n"
"      Show action dialog.\n"
"    action [ROW=0] ... \"command\" [separator=\\n]\n"
"      Apply command on item text in the row.\n"
"\n"
"    config\n"
"      List all options.\n"
"    config <OPTION>\n"
"      Get option value.\n"
"    config <OPTION> <VALUE>\n"
"      Set option value.\n"
"\n"
"    help, -h, --help\n"
"      Print this help.\n\n",
"command line help").toLocal8Bit().constData();
}

static int startServer(int argc, char *argv[])
{
    ClipboardServer app(argc, argv);
    if ( app.isListening() ) {
#ifdef Q_WS_WIN
    // FIXME: console window is still shown for a moment
    FreeConsole();
#endif
        return app.exec();
    } else {
        log( QObject::tr("CopyQ server is already running."), LogWarning );
        return 0;
    }
}

static int startMonitor(int argc, char *argv[])
{
    ClipboardMonitor app(argc, argv);
    return app.isConnected() ? app.exec() : 0;
}

static int startClient(int argc, char *argv[])
{
    ClipboardClient app(argc, argv);
    return app.exec();
}

int main(int argc, char *argv[])
{
    // arguments -h,--help,help: print help and exit
    if (argc == 2) {
        char *arg = argv[1];
        if (strcmp("-h",arg) == 0 ||
            strcmp("--help",arg) == 0 ||
            strcmp("help",arg) == 0) {
            usage();
            return 0;
        }
    }

    if (argc == 1) {
        // if server hasn't been run yet and no argument were specified
        // then run this as server
        return startServer(argc, argv);
    } else if (argc == 2 && strcmp(argv[1], "monitor") == 0) {
        // if argument specified and server is running
        // then run this as client
        return startMonitor(argc, argv);
    } else {
        // if argument specified and server is running
        // then run this as client
        return startClient(argc, argv);
    }
}
