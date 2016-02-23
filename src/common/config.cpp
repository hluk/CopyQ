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

#include "config.h"

#include "common/log.h"

#include <QApplication>
#include <QByteArray>
#include <QDesktopWidget>
#include <QDir>
#include <QRegExp>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <QWidget>

namespace {

QString geometryOptionName(const QWidget *widget, bool save, bool openOnCurrentScreen)
{
    QString widgetName = widget->objectName();
    QString optionName = "Options/" + widgetName + "_geometry";

    // current screen number
    if (openOnCurrentScreen) {
        int n = save ? QApplication::desktop()->screenNumber(widget)
                     : QApplication::desktop()->screenNumber(QCursor::pos());
        if (n > 0)
            optionName.append( QString("_screen_%1").arg(n) );
    } else {
        optionName.append("_global");
    }

    return optionName;
}

} // namespace

QString getConfigurationFilePath(const QString &suffix)
{
    const QSettings settings(
                QSettings::IniFormat, QSettings::UserScope,
                QCoreApplication::organizationName(),
                QCoreApplication::applicationName() );
    QString path = settings.fileName();
    return path.replace( QRegExp("\\.ini$"), suffix );
}

QString settingsDirectoryPath()
{
    return QDir::cleanPath( getConfigurationFilePath("") + "/.." );
}

QByteArray geometryOptionValue(const QString &optionName)
{
    QSettings geometrySettings( getConfigurationFilePath("_geometry.ini"), QSettings::IniFormat );
    QVariant geometry = geometrySettings.value(optionName);
    return geometry.toByteArray();
}

void restoreWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    const QString optionName = geometryOptionName(w, false, openOnCurrentScreen);
    const QByteArray geometry = geometryOptionValue(optionName);
    if (w->saveGeometry() != geometry)
        w->restoreGeometry(geometry);
}

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    const QString optionName = geometryOptionName(w, true, openOnCurrentScreen);
    QSettings geometrySettings( getConfigurationFilePath("_geometry.ini"), QSettings::IniFormat );
    geometrySettings.setValue( optionName, w->saveGeometry() );
}
