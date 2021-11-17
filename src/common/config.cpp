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
#include <QDir>
#include <QScreen>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <QWidget>
#include <QWindow>

#define GEOMETRY_LOG(window, message) \
    COPYQ_LOG( QString::fromLatin1("Geometry: Window \"%1\": %2").arg(window->objectName(), message) )

namespace {

const char propertyGeometryLockedUntilHide[] = "CopyQ_geometry_locked_until_hide";

enum class GeometryAction {
    Save,
    Restore
};

bool isMousePositionSupported()
{
    // On Wayland, getting mouse position can return
    // the last known mouse position in an own Qt application window.
    static const bool supported = !QCursor::pos().isNull();
    return supported;
}

QString toString(const QRect &geometry)
{
    return QString::fromLatin1("%1x%2,%3,%4")
            .arg(geometry.width())
            .arg(geometry.height())
            .arg(geometry.x())
            .arg(geometry.y());
}

int screenNumber(const QWidget &widget, GeometryAction geometryAction)
{
    if (geometryAction == GeometryAction::Restore) {
        if ( !isMousePositionSupported() )
            return -1;
        const int n = screenNumberAt(QCursor::pos());
        if (n != -1)
            return n;
    }

    QWindow *windowHandle = widget.windowHandle();
    if (windowHandle) {
        QScreen *screen = windowHandle->screen();
        if (screen)
            return QGuiApplication::screens().indexOf(screen);
    }

    return -1;
}

QString geometryOptionName(const QWidget &widget)
{
    return QString::fromLatin1("Options/%1_geometry").arg(widget.objectName());
}

QString geometryOptionName(const QWidget &widget, GeometryAction geometryAction, bool openOnCurrentScreen)
{
    const QString baseGeometryName = geometryOptionName(widget);

    if (!openOnCurrentScreen)
        return QString::fromLatin1("%1_global").arg(baseGeometryName);

    const int n = screenNumber(widget, geometryAction);
    if (n > 0)
        return QString::fromLatin1("%1_screen_%2").arg(baseGeometryName).arg(n);

    return baseGeometryName;
}

QString getGeometryConfigurationFilePath()
{
    return getConfigurationFilePath("_geometry.ini");
}

QString resolutionTagForScreen(int i)
{
    const QRect screenGeometry = ::screenGeometry(i);
    return QString::fromLatin1("_%1x%2")
            .arg(screenGeometry.width())
            .arg(screenGeometry.height());
}

QString resolutionTag(const QWidget &widget, GeometryAction geometryAction, bool openOnCurrentScreen)
{
    if (openOnCurrentScreen) {
        const int i = screenNumber(widget, geometryAction);
        if (i == -1)
            return QString();
        return resolutionTagForScreen(i);
    }

    QString tag;
    for ( int i = 0; i < screenCount(); ++i )
        tag.append( resolutionTagForScreen(i) );

    return tag;
}

void ensureWindowOnScreen(QWidget *widget, QPoint pos)
{
    const QSize size = widget->frameSize();
    const QRect availableGeometry = screenAvailableGeometry(pos);

    int x = pos.x();
    int y = pos.y();
    int w = size.width();
    int h = size.height();

    // Ensure that the window fits the screen, otherwise it would be moved
    // to a neighboring screen automatically.
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
        GEOMETRY_LOG( widget, QString::fromLatin1("Resize window: %1x%2").arg(w).arg(h) );
        widget->resize(w, h);
    }

    if ( widget->pos() != QPoint(x, y) ) {
        GEOMETRY_LOG( widget, QString::fromLatin1("Move window: %1, %2").arg(x).arg(y) );
        widget->move(x, y);
    }
}

void ensureWindowOnScreen(QWidget *w)
{
    const QPoint pos = w->pos();
    ensureWindowOnScreen(w, pos);
}

QString getConfigurationFilePathHelper()
{
    const QSettings settings(
                QSettings::IniFormat, QSettings::UserScope,
                QCoreApplication::organizationName(),
                QCoreApplication::applicationName() );
    return settings.fileName();
}

} // namespace

const QString &getConfigurationFilePath()
{
    static const QString path = getConfigurationFilePathHelper();
    return path;
}

QString getConfigurationFilePath(const char *suffix)
{
    QString path = getConfigurationFilePath();
    // Replace suffix.
    const int i = path.lastIndexOf(QLatin1Char('.'));
    Q_ASSERT(i != -1);
    Q_ASSERT( path.endsWith(QLatin1String(".ini")) );
    return path.leftRef(i) + QLatin1String(suffix);
}

const QString &settingsDirectoryPath()
{
    static const QString path =
        QDir::cleanPath( getConfigurationFilePath() + QLatin1String("/..") );
    return path;
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
    const QString optionName = geometryOptionName(*w, GeometryAction::Restore, openOnCurrentScreen);
    const QString tag = resolutionTag(*w, GeometryAction::Restore, openOnCurrentScreen);
    QByteArray geometry = geometryOptionValue(optionName + tag).toByteArray();

    // If geometry for screen resolution doesn't exist, use last saved one.
    const bool restoreUntaggedGeometry = geometry.isEmpty();
    if (restoreUntaggedGeometry) {
        geometry = geometryOptionValue(optionName).toByteArray();

        // If geometry for the screen doesn't exist, move window to the middle of the screen.
        if (geometry.isEmpty()) {
            const QRect availableGeometry = screenAvailableGeometry(w->pos());
            const QPoint position = availableGeometry.center() - w->rect().center();
            w->move(position);
        }
    }

    if (openOnCurrentScreen) {
        const int screenNumber = ::screenNumber(*w, GeometryAction::Restore);
        QScreen *screen = QGuiApplication::screens().value(screenNumber);
        if (screen) {
            // WORKAROUND: Fixes QWidget::restoreGeometry() for different monitor scaling.
            QWindow *windowHandle = w->windowHandle();
            if ( windowHandle && windowHandle->screen() != screen )
                windowHandle->setScreen(screen);

            const QRect availableGeometry = screen->availableGeometry();
            const QPoint position = availableGeometry.center() - w->rect().center();
            w->move(position);
        }
    }

    const QRect oldGeometry = w->geometry();
    if ( !geometry.isEmpty() )
        w->restoreGeometry(geometry);

    ensureWindowOnScreen(w);

    const QRect newGeometry = w->geometry();
    GEOMETRY_LOG( w,
        QString::fromLatin1("%5 geometry \"%1%2\": %3 -> %4").arg(
            optionName,
            restoreUntaggedGeometry ? QString() : tag,
            toString(oldGeometry),
            toString(newGeometry),
            geometry.isEmpty() ? QLatin1String("New") : QLatin1String("Restore")) );
}

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    const QString optionName = geometryOptionName(*w, GeometryAction::Save, openOnCurrentScreen);
    const QString tag = resolutionTag(*w, GeometryAction::Save, openOnCurrentScreen);
    QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    const auto geometry = w->saveGeometry();
    geometrySettings.setValue(optionName + tag, geometry);
    geometrySettings.setValue(optionName, geometry);
    geometrySettings.setValue(geometryOptionName(*w), geometry);
    GEOMETRY_LOG( w, QString::fromLatin1("Save geometry \"%1%2\": %3")
                  .arg(optionName, tag, toString(w->geometry())) );
}

QByteArray mainWindowState(const QString &mainWindowObjectName)
{
    const QString optionName = QString::fromLatin1("Options/%1_state").arg(mainWindowObjectName);
    return geometryOptionValue(optionName).toByteArray();
}

void saveMainWindowState(const QString &mainWindowObjectName, const QByteArray &state)
{
    const QString optionName = QString::fromLatin1("Options/%1_state").arg(mainWindowObjectName);
    setGeometryOptionValue(optionName, state);
}

void moveToCurrentWorkspace(QWidget *w)
{
#ifdef COPYQ_WS_X11
    /* Re-initialize window in window manager so it can popup on current workspace. */
    if (w->isVisible()) {
        GEOMETRY_LOG( w, QLatin1String("Move to current workspace") );
        const bool blockUntilHide = isGeometryGuardBlockedUntilHidden(w);
        w->hide();
        if (blockUntilHide)
            setGeometryGuardBlockedUntilHidden(w, true);
        w->show();
    }
#else
    Q_UNUSED(w)
#endif
}

void moveWindowOnScreen(QWidget *w, QPoint pos)
{
    ensureWindowOnScreen(w, pos);
    moveToCurrentWorkspace(w);
}

void setGeometryGuardBlockedUntilHidden(QWidget *w, bool blocked)
{
    GEOMETRY_LOG( w, QString::fromLatin1("Geometry blocked until hidden: %1").arg(blocked) );
    w->setProperty(propertyGeometryLockedUntilHide, blocked);
}

bool isGeometryGuardBlockedUntilHidden(const QWidget *w)
{
    return w->property(propertyGeometryLockedUntilHide).toBool();
}
