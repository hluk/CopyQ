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
#include <iostream>
#include <qtlocalpeer.h>

inline bool readCssFile(QIODevice &device, QSettings::SettingsMap &map)
{
    map.insert( "css", device.readAll() );
    return true;
}

inline bool writeCssFile(QIODevice &, const QSettings::SettingsMap &)
{
    return true;
}

void usage()
{
    std::cout <<
"usage: copyq [command]\n"
"  command:\n"
"    toggle                 show/hide main window\n"
"    exit                   exit server\n"
"    select [row=0]         move item in the row to the clipboard\n"
"    action                 show action dialog\n"
"    action [row=0] \"command\" [separator=\\n]\n"
"                           apply command on item text in the row\n"
"    add text               add text into clipboard\n"
"    remove [row=0] ...     remove item in given rows\n"
"    edit                   edit clipboard item\n"
"    new                    create and edit new item\n"
"    menu                   open context menu\n"
"    list [format=\"%1\\n\"|row=0] ...\n"
"                           print items in given rows\n"
"    help,-h,--help         print this help\n";
}

int main(int argc, char *argv[])
{
    // arguments -h,--help,help: print help and exit
    if (argc > 1) {
        char *arg = argv[1];
        if (strcmp("-h",arg) == 0 ||
            strcmp("--help",arg) == 0 ||
            strcmp("help",arg) == 0) {
            usage();
            return 0;
        }
    }

    Q_INIT_RESOURCE(copyq);
    QCoreApplication::setOrganizationName("copyq");
    QCoreApplication::setApplicationName("copyq");

    // if server hasn't been run yet and no argument were specified
    // then run this as server
    if (argc == 1) {
        QtSingleApplication app( QString("CopyQ"), argc, argv );

        if ( app.isRunning() ) {
            std::cout << "CopyQ server is already running\n";
            return 0;
        }

        // style
        QSettings::Format cssFormat = QSettings::registerFormat(
                "css", readCssFile, writeCssFile);
        QSettings cssSettings( cssFormat, QSettings::UserScope,
                               QCoreApplication::organizationName(),
                               QCoreApplication::applicationName() );
        QString css = cssSettings.value("css", "").toString();

        MainWindow wnd(css);

        QObject::connect(&app, SIGNAL(messageReceived(const QString&)),
                         &wnd, SLOT(handleMessage(const QString&)));

        // don't exit when about or action dialog is closed
        app.setQuitOnLastWindowClosed(false);

        return app.exec();
    }
    // if argument specified and server is running
    // then run this as client
    else {
        QString msg;
        QStringList args;

        QtLocalPeer peer( NULL, QString("CopyQ") );

        // serialize arguments
        for (int i = 1; i < argc; ++i)
            args.append( QString(argv[i]) );
        serialize_args(args,msg);

        // create client -- wait for others to close
        Client client;
        client.connect();

        // error: when arguments specified and no server running
        if ( !peer.sendMessage(msg, 3000) ) {
            // TODO: tr()
            std::cout << "ERROR: Sending command to server failed. Is CopyQ server running? To run a server just execute this program without parameters.\n";
            usage();
            return 1;
        }

        return client.exec();
    }

    // shouldn't get here
    return -1;
}
