/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "gui/screen.h"

#include <QApplication>
#include <QByteArray>
#include <QDesktopWidget>
#include <QDir>
#include <QRegularExpression>
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
            : screenNumberAt(QCursor::pos());
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
    const QRect screenGeometry = ::screenGeometry(i);
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
    for ( int i = 0; i < screenCount(); ++i )
        tag.append( resolutionTagForScreen(i) );

    return tag;
}

void ensureWindowOnScreen(QWidget *widget, QPoint pos)
{
    const QSize size = widget->size();
    const QRect availableGeometry = screenAvailableGeometry(pos);

    int x = pos.x();
    int y = pos.y();
    int w = size.width();
    int h = size.height();

    // Ensure that the window fits the screen, otherwise it may be moved
    // to a neighboring screen.
    w = qMin(w, availableGeometry.width());
    h = qMin(h, availableGeometry.height());

    if ( x + w > availableGeometry.right() )
        x = availableGeometry.right() - w;

    if ( x < availableGeometry.left() )
        x = availableGeometry.left();

    if ( y + h > availableGeometry.bottom() )
        y = availableGeometry.bottom() - h;

    if ( y < availableGeometry.top())
        y = availableGeometry.top();

    if ( size != QSize(w, h) ) {
        GEOMETRY_LOG( widget, QString("Resize window: %1x%2").arg(w).arg(h) );
        widget->resize(w, h);
    }

    if ( widget->pos() != QPoint(x, y) ) {
        GEOMETRY_LOG( widget, QString("Move window: %1, %2").arg(x).arg(y) );
        widget->move(x, y);
    }
}

void ensureWindowOnScreen(QWidget *w)
{
    const QPoint pos = w->pos();
    ensureWindowOnScreen(w, pos);
}

} // namespace

QString getConfigurationFilePath(const QString &suffix)
{
    const QSettings settings(
                QSettings::IniFormat, QSettings::UserScope,
                QCoreApplication::organizationName(),
                QCoreApplication::applicationName() );
    QString path = settings.fileName();
    return path.replace( QRegularExpression("\\.ini$"), suffix );
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
            const QRect availableGeometry = screenAvailableGeometry(QCursor::pos());
            const auto position = availableGeometry.center() - w->rect().center();
            w->move(position);

            GEOMETRY_LOG( w, QString("New geometry for \"%1%2\"").arg(optionName, tag) );
        }
    }

    if (w->saveGeometry() != geometry) {
        if ( openOnCurrentScreen ) {
            const int screenNumber = ::screenNumber(*w, GeometryAction::Restore);
            QScreen *screen = QGuiApplication::screens().value(screenNumber);
            if (screen) {
                // WORKAROUND: Fixes QWidget::restoreGeometry() for different monitor scaling.
                auto windowHandle = w->windowHandle();
                if ( windowHandle && windowHandle->screen() != screen )
                    windowHandle->setScreen(screen);

                const QRect availableGeometry = screen->availableGeometry();
                const auto position = availableGeometry.center() - w->rect().center();
                w->move(position);
            }
        }

        const auto oldGeometry = w->geometry();
        if ( !geometry.isEmpty() )
            w->restoreGeometry(geometry);

        ensureWindowOnScreen(w);

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
    ensureWindowOnScreen(w, pos);
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
