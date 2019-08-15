/*
    Copyright (c) 2019, Lukas Holecek <hluk@email.cz>

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
#include "log.h"

#include <QApplication>
#include <QByteArray>
#include <QDesktopWidget>
#include <QDir>
#include <QRegExp>
#include <QScreen>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <QWidget>
#include <QWindow>

#define GEOMETRY_LOG(window, message) \
    COPYQ_LOG( QString("Geometry: Window \"%1\": %2").arg(window->objectName(), message) )

namespace {

const char propertyGeometryLockedUntilHide[] = "CopyQ_geometry_locked_until_hide";

enum class GeometryAction {
    Save,
    Restore
};

QString toString(const QRect &geometry)
{
    return QString("%1x%2,%3,%4")
            .arg(geometry.width())
            .arg(geometry.height())
            .arg(geometry.x())
            .arg(geometry.y());
}

int screenNumber(const QWidget &widget, GeometryAction geometryAction)
{
    return geometryAction == GeometryAction::Save
            ? QApplication::desktop()->screenNumber(&widget)
            : QApplication::desktop()->screenNumber(QCursor::pos());
}

QString geometryOptionName(const QWidget &widget, GeometryAction geometryAction, bool openOnCurrentScreen)
{
    QString widgetName = widget.objectName();
    QString optionName = "Options/" + widgetName + "_geometry";

    // current screen number
    if (openOnCurrentScreen) {
        const int n = screenNumber(widget, geometryAction);
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

QString resolutionTagForScreen(int i)
{
    const QRect screenGeometry = QApplication::desktop()->screenGeometry(i);
    return QString("_%1x%2")
            .arg(screenGeometry.width())
            .arg(screenGeometry.height());
}

QString resolutionTag(const QWidget &widget, GeometryAction geometryAction, bool openOnCurrentScreen)
{
    if (openOnCurrentScreen) {
        const int i = screenNumber(widget, geometryAction);
        return resolutionTagForScreen(i);
    }

    QString tag;
    const auto desktop = QApplication::desktop();
    for ( int i = 0; i < desktop->screenCount(); ++i )
        tag.append( resolutionTagForScreen(i) );

    return tag;
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
    if ( isGeometryGuardBlockedUntilHidden(w) )
        return;

    const QString optionName = geometryOptionName(*w, GeometryAction::Restore, openOnCurrentScreen);
    const QString tag = resolutionTag(*w, GeometryAction::Restore, openOnCurrentScreen);
    QByteArray geometry = geometryOptionValue(optionName + tag).toByteArray();

    // If geometry for screen resolution doesn't exist, use last saved one.
    const bool hasGeometryTag = geometry.isEmpty();
    if (hasGeometryTag) {
        geometry = geometryOptionValue(optionName).toByteArray();

        // If geometry for the screen doesn't exist, move window to the middle of the screen.
        if (geometry.isEmpty()) {
            const QRect availableGeometry = QApplication::desktop()->availableGeometry(QCursor::pos());
            const auto position = availableGeometry.center() - w->rect().center();
            w->move(position);

            geometry = w->saveGeometry();

            GEOMETRY_LOG( w, QString("New geometry for \"%1%2\"").arg(optionName, tag) );
        }
    }

    if (w->saveGeometry() != geometry) {
        // WORKAROUND: Fixes QWidget::restoreGeometry() for different monitor scaling.
        if ( openOnCurrentScreen ) {
            const int screenNumber = ::screenNumber(*w, GeometryAction::Restore);
            QScreen *screen = QGuiApplication::screens().value(screenNumber);
            if (screen) {
                if ( w->windowHandle() )
                    w->windowHandle()->setScreen(screen);
                else
                    w->move(screen->geometry().topLeft());
            }
        }

        const auto oldGeometry = w->geometry();
        w->restoreGeometry(geometry);

        const auto newGeometry = w->geometry();
        GEOMETRY_LOG( w, QString("Restore geometry \"%1%2\": %3 -> %4").arg(
                          optionName,
                          hasGeometryTag ? tag : QString(),
                          toString(oldGeometry),
                          toString(newGeometry)) );
    }
}

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    const QString optionName = geometryOptionName(*w, GeometryAction::Save, openOnCurrentScreen);
    const QString tag = resolutionTag(*w, GeometryAction::Save, openOnCurrentScreen);
    QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    geometrySettings.setValue( optionName + tag, w->saveGeometry() );
    geometrySettings.setValue( optionName, w->saveGeometry() );
    GEOMETRY_LOG( w, QString("Save geometry \"%1%2\": %3").arg(optionName, tag, toString(w->geometry())) );
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

void moveToCurrentWorkspace(QWidget *w)
{
#ifdef COPYQ_WS_X11
    /* Re-initialize window in window manager so it can popup on current workspace. */
    if (w->isVisible()) {
        GEOMETRY_LOG(w, "Move to current workspace");
        const auto blockUntilHide = isGeometryGuardBlockedUntilHidden(w);
        w->hide();
        if (blockUntilHide)
            setGeometryGuardBlockedUntilHidden(w);
        w->show();
    }
#else
    Q_UNUSED(w);
#endif
}

void moveWindowOnScreen(QWidget *w, QPoint pos)
{
    const QRect availableGeometry = QApplication::desktop()->availableGeometry(pos);
    const int x = qMax(availableGeometry.left(), qMin(pos.x(), availableGeometry.right() - w->width()));
    const int y = qMax(availableGeometry.top(), qMin(pos.y(), availableGeometry.bottom() - w->height()));
    GEOMETRY_LOG( w, QString("Move window on screen %1x%2")
                  .arg(availableGeometry.right())
                  .arg(availableGeometry.bottom()) );
    w->move(x, y);
    moveToCurrentWorkspace(w);
}

void setGeometryGuardBlockedUntilHidden(QWidget *w, bool blocked)
{
    if ( isGeometryGuardBlockedUntilHidden(w) != blocked ) {
        GEOMETRY_LOG( w, QString("Geometry blocked until hidden: %1").arg(blocked) );
        w->setProperty(propertyGeometryLockedUntilHide, blocked);
    }
}

bool isGeometryGuardBlockedUntilHidden(const QWidget *w)
{
    return w->property(propertyGeometryLockedUntilHide).toBool();
}
