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
#include "clipboardmonitor.h"
#include <QSettings>
#include <QDebug>
#include <iostream>
#include <qtlocalpeer.h>

void usage()
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
"    edit\n"
"      edit clipboard item\n"
"    list [format=\"%1\\n\"|row=0] ...\n"
"      print items in given rows\n"
"\n"
"    read [mime_type=\"text/plain\"|row=0] ...\n"
"      print raw data\n"
"    write mime_type data ...\n"
"      write raw data to clipboard\n"
"    - [mime_type=\"text/plain\"]\n"
"      copy text from standard input into clipboard\n"
"\n"
"    action\n"
"      show action dialog\n"
"    action [row=0] \"command\" [separator=\\n]\n"
"      apply command on item text in the row\n"
"\n"
"    help, -h, --help\n"
"      print this help\n",
"command line help").toLocal8Bit().constData();
}

int runServer(int argc, char *argv[]) {
    Q_INIT_RESOURCE(copyq);
    QCoreApplication::setOrganizationName("copyq");
    QCoreApplication::setApplicationName("copyq");

    QtSingleApplication app( QString("CopyQ"), argc, argv );

    if ( app.isRunning() ) {
        std::cout << QObject::tr("CopyQ server is already running.\n",
                                 "warning: running"
                                 ).toLocal8Bit().constData();
        return 0;
    }

    MainWindow wnd;

    wnd.connect(&app, SIGNAL(messageReceived(const QString&)),
                      SLOT(handleMessage(const QString&)));

    // don't exit when about or action dialog is closed
    app.setQuitOnLastWindowClosed(false);

    return app.exec();
}

int runMonitor(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName("copyq");
    QCoreApplication::setApplicationName("copyq");

    QtSingleApplication app( QString("CopyQmonitor"), argc, argv );

    if ( app.isRunning() ) {
        return 0;
    }

    ClipboardMonitor *monitor = new ClipboardMonitor;

    monitor->connect(&app, SIGNAL(messageReceived(const QString&)),
                           SLOT(handleMessage(const QString&)));

    return app.exec();
}

int runClient(const DataList &args) {
    Client client;
    bool ok = false;

    QCoreApplication::setOrganizationName("copyq");
    QCoreApplication::setApplicationName("copyq");

    QtLocalPeer peer( NULL, QString("CopyQ") );
    // is server running?
    if ( peer.isClient() ) {
        // serialize arguments to create msg for server
        QString msg;
        if ( !args.isEmpty() && args.first() == QString("-") ) {
            QByteArray mime;
            if ( args.length() == 2 ) {
                mime = args.last();
            } else {
                mime = QByteArray("text/plain");
            }

            // read text from stdin
            DataList args2;
            QFile in;
            in.open(stdin, QIODevice::ReadOnly);
            args2 << QByteArray("write") << mime << in.readAll();
            serialize_args(args2, msg);
        } else {
            serialize_args(args, msg);
        }

        // create client -- wait for others to close
        client.connect();

        ok = peer.sendMessage(msg, 3000);
    };

    if ( ok ) {
        return client.exec();
    } else {
        std::cout << QObject::tr(
                "ERROR: Sending command to server failed. "
                "Is CopyQ server running? To run a server "
                "just execute this program without parameters.\n",
                "error: not running"
                ).toLocal8Bit().constData();
        return 1;
    }
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

    // if server hasn't been run yet and no argument were specified
    // then run this as server
    if (argc == 1) {
        return runServer(argc, argv);
    }
    else if (argc == 2 && strcmp(argv[1], "monitor") == 0) {
        return runMonitor(argc, argv);
    }
    // if argument specified and server is running
    // then run this as client
    else {
        DataList args;
        for (int i = 1; i < argc; ++i)
            args.append( QByteArray(argv[i]) );
        return runClient(args);
    }

    // shouldn't get here
    return -1;
}
