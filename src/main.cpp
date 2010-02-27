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

#include <qtsingleapplication.h> 
#include "mainwindow.h"
#include <QSettings>
#include <QDebug>

inline bool readCssFile(QIODevice &device, QSettings::SettingsMap &map)
{
    map.insert( "css", device.readAll() );
    return true;
}

inline bool writeCssFile(QIODevice &, const QSettings::SettingsMap &)
{
    return true;
}

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(copyq);
    QCoreApplication::setOrganizationName("copyq");
    QCoreApplication::setApplicationName("copyq");

    QtSingleApplication app(argc, argv);

    // try to send a message if application already running
    if ( app.sendMessage(argc > 1 ? argv[1] : "toggle") )
        return 0;

    app.processEvents();

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
