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

QString geometryOptionName(const QWidget &widget, bool save, bool openOnCurrentScreen)
{
    QString widgetName = widget.objectName();
    QString optionName = "Options/" + widgetName + "_geometry";

    // current screen number
    if (openOnCurrentScreen) {
        int n = save ? QApplication::desktop()->screenNumber(&widget)
                     : QApplication::desktop()->screenNumber(QCursor::pos());
        if (n > 0)
            optionName.append( QString("_screen_%1").arg(n) );
    } else {
        optionName.append("_global");
    }

    return optionName;
}

QString getGeometryConfigurationFilePath()
{
    return getConfigurationFilePath("_geometry.ini");
}

QString resolutionTag(const QWidget &widget)
{
    const QRect screenGeometry = QApplication::desktop()->screenGeometry(&widget);
    return QString("_%1x%2")
            .arg(screenGeometry.width())
            .arg(screenGeometry.height());
}

QString windowGeometryToString(const QWidget &widget)
{
    const QRect geometry = widget.geometry();
    return QString("%1,%2 %3x%4")
            .arg(geometry.x())
            .arg(geometry.y())
            .arg(geometry.width())
            .arg(geometry.height());
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

QVariant geometryOptionValue(const QString &optionName)
{
    const QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    return geometrySettings.value(optionName);
}

void setGeometryOptionValue(const QString &optionName, const QVariant &value)
{
    QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    geometrySettings.setValue(optionName, value);
}

void restoreWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    const QString optionName = geometryOptionName(*w, false, openOnCurrentScreen);
    const QString tag = resolutionTag(*w);
    QByteArray geometry = geometryOptionValue(optionName + tag).toByteArray();

    // If geometry for screen resolution doesn't exist, use last saved one.
    if (geometry.isEmpty())
        geometry = geometryOptionValue(optionName).toByteArray();

    if (w->saveGeometry() != geometry) {
        w->restoreGeometry(geometry);

        // Workaround for broken geometry restore.
        if (w->width() <= 0 || w->height() <= 0) {
            COPYQ_LOG("Fixing broken window geometry " + optionName + tag + ": " + windowGeometryToString(*w));
            w->showNormal();
            w->restoreGeometry(geometry);
            w->showMinimized();
        }

        COPYQ_LOG("Restored window geometry " + optionName + tag + ": " + windowGeometryToString(*w));
    }
}

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    const QString optionName = geometryOptionName(*w, true, openOnCurrentScreen);
    const QString tag = resolutionTag(*w);
    QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    geometrySettings.setValue( optionName + tag, w->saveGeometry() );
    geometrySettings.setValue( optionName, w->saveGeometry() );
    COPYQ_LOG("Saved window geometry " + optionName + tag + ": " + windowGeometryToString(*w));
}

QByteArray mainWindowState(const QString &mainWindowObjectName)
{
    const QString optionName = "Options/" + mainWindowObjectName + "_state";
    return geometryOptionValue(optionName).toByteArray();
}

void saveMainWindowState(const QString &mainWindowObjectName, const QByteArray &state)
{
    const QString optionName = "Options/" + mainWindowObjectName + "_state";
    setGeometryOptionValue(optionName, state);
}
