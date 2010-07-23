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
"    get                    print clipboard contents\n"
"    edit                   edit clipboard item\n"
"    new                    create and edit new item\n"
"    menu                   open context menu\n"
"    show [title] [row=0]   show popup message with text of item in the row\n"
"    list [format=\"%1\\n\"|row=0] ...\n"
"                           print items in given rows\n"
"    help,-h,--help         print this help\n";
}

int main(int argc, char *argv[])
{
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

    // TODO: different session id for other users
    QtSingleApplication app( QString("CopyQ"), argc, argv );

    // if server is running, run this as client
    if ( app.isRunning() ) {
        if (argc == 1) {
            std::cout << "CopyQ server is already running\n";
            exit(0);
        }

        // serialize arguments
        QString msg;
        QStringList args;
        for (int i = 1; i < argc; ++i)
            args.append( QString(argv[i]) );
        serialize_args(args,msg);

        Client client;
        client.connect();

        // try to send a message if application already running
        // -1 means wait forever for app to respond (if instance found)
        app.sendMessage(msg, -1);

        return client.exec();
    }
    if (argc > 1) {
        std::cout << "ERROR: CopyQ server is not running. To run a server just execute this program without parameters.\n";
        usage();
        exit(1);
    }
    // if server hasn't been run yet, run this as server
    else {
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

        return app.exec();
    }
}
