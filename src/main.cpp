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
#include <QSettings>
#include <QDebug>
#include <QTextStream>
#include <iostream>
#include <qtlocalpeer.h>

void usage()
{
    std::cout << QObject::tr(
"usage: copyq [command]\n"
"  command:\n"
"    toggle                 show/hide main window\n"
"    exit                   exit server\n"
"    select [row=0]         move item in the row to the clipboard\n"
"    action                 show action dialog\n"
"    action [row=0] \"command\" [separator=\\n]\n"
"                           apply command on item text in the row\n"
"    add <text> ...         add text into clipboard\n"
"    - [mime_type=\"text/plain\"]\n"
"                           add text from standard input into clipboard\n"
"    remove [row=0] ...     remove item in given rows\n"
"    edit                   edit clipboard item\n"
"    new                    create and edit new item\n"
"    menu                   open context menu\n"
"    list [format=\"%1\\n\"|row=0] ...\n"
"                           print items in given rows\n"
"    help,-h,--help         print this help\n",
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

int runClient(const QStringList &args) {
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
            QString mime;
            if ( args.length() == 2 ) {
                mime = args.last();
            } else {
                mime = QString("text/plain");
            }

            // read text from stdin
            QStringList args2;
            QTextStream stream(stdin);
            args2 << QString("write") << mime << stream.readAll();
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
    // if argument specified and server is running
    // then run this as client
    else {
        QStringList args;
        for (int i = 1; i < argc; ++i)
            args.append( QString(argv[i]) );
        return runClient(args);
    }

    // shouldn't get here
    return -1;
}
