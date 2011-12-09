/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

static void usage()
{
    std::cout << QObject::tr(
"usage: copyq [command]\n"
"Starts server if no command is specified.\n"
"  commands:\n"
"    toggle  show/hide main window\n"
"    menu    open context menu\n"
"    exit    exit server\n"
"\n"
"    length, count, size\n"
"      print number of items in history\n"
"    select [row=0]\n"
"      move item in the row to clipboard\n"
"    add <text> ...\n"
"      add text into clipboard\n"
"    remove [row=0] ...\n"
"      remove item in given rows\n"
"    edit [row=0] ...\n"
"      edit clipboard item\n"
"    list [format=\"%1\\n\"] [row=0] ...\n"
"      print items in given rows\n"
"\n"
"    read [mime_type=\"text/plain\"|row=0] ...\n"
"      print raw data\n"
"    write mime_type data ...\n"
"      write raw data to clipboard\n"
"    - [mime_type=\"text/plain\"]\n"
"      copy text from standard input into clipboard\n"
"\n"
"    action [row=0] ...\n"
"      show action dialog\n"
"    action [row=0] ... \"command\" [separator=\\n]\n"
"      apply command on item text in the row\n"
"\n"
"    help, -h, --help\n"
"      print this help\n",
"command line help").toLocal8Bit().constData();
}

static int startServer(int argc, char *argv[])
{
    ClipboardServer app(argc, argv);
    if ( app.isListening() ) {
        try {
             return app.exec();
         } catch (const std::bad_alloc &e) {
             return 0;
         }
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
