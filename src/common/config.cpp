// SPDX-License-Identifier: GPL-3.0-or-later

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
    COPYQ_LOG( QStringLiteral("Geometry: Window \"%1\": %2").arg(window->objectName(), message) )

namespace {

const char propertyGeometryLockedUntilHide[] = "CopyQ_geometry_locked_until_hide";

constexpr int windowMinWidth = 50;
constexpr int windowMinHeight = 50;

QSize frameSize(QWidget *widget) {
    const QRect frame = widget->frameGeometry();
    const QSize size = widget->size();
    const int w = qMax(windowMinWidth, qMax(frame.width(), size.width()));
    const int h = qMax(windowMinHeight, qMax(frame.height(), size.height()));
    return QSize(w, h);
}

QString toString(const QRect &geometry)
{
    return QStringLiteral("%1x%2,%3,%4")
            .arg(geometry.width())
            .arg(geometry.height())
            .arg(geometry.x())
            .arg(geometry.y());
}

int screenNumber(const QWidget &widget)
{
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
    return QStringLiteral("Options/%1_geometry").arg(widget.objectName());
}

QString geometryOptionName(const QWidget &widget, bool openOnCurrentScreen)
{
    const QString baseGeometryName = geometryOptionName(widget);

    if (!openOnCurrentScreen)
        return QStringLiteral("%1_global").arg(baseGeometryName);

    const int n = screenNumber(widget);
    if (n > 0)
        return QStringLiteral("%1_screen_%2").arg(baseGeometryName).arg(n);

    return baseGeometryName;
}

QString getGeometryConfigurationFilePath()
{
    return getConfigurationFilePath("_geometry.ini");
}

QString resolutionTagForScreen(int i)
{
    const QRect screenGeometry = ::screenGeometry(i);
    return QStringLiteral("_%1x%2")
            .arg(screenGeometry.width())
            .arg(screenGeometry.height());
}

QString resolutionTag(const QWidget &widget, bool openOnCurrentScreen)
{
    if (openOnCurrentScreen) {
        const int i = screenNumber(widget);
        if (i == -1)
            return QString();
        return resolutionTagForScreen(i);
    }

    QString tag;
    for ( int i = 0; i < screenCount(); ++i )
        tag.append( resolutionTagForScreen(i) );

    return tag;
}

void ensureWindowOnScreen(QWidget *widget)
{
    const QSize frame  = frameSize(widget);
    int x = widget->x();
    int y = widget->y();
    int w = qMax(windowMinWidth, frame.width());
    int h = qMax(windowMinHeight, frame.height());

    const QRect availableGeometry = screenAvailableGeometry(*widget);
    if ( availableGeometry.isValid() ) {
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
    }

    if ( frame != QSize(w, h) ) {
        GEOMETRY_LOG(
            widget, QStringLiteral("Resize window: %1x%2 -> %3x%4")
            .arg(frame.width())
            .arg(frame.height())
            .arg(w)
            .arg(h) );
        widget->resize(w, h);
    }

    if ( widget->pos() != QPoint(x, y) ) {
        GEOMETRY_LOG( widget, QStringLiteral("Move window: %1, %2").arg(x).arg(y) );
        widget->move(x, y);
    }
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

bool ensureSettingsDirectoryExists()
{
    QDir settingsDir( settingsDirectoryPath() );
    if ( !settingsDir.mkpath(QStringLiteral(".")) ) {
        log( QStringLiteral("Failed to create the directory for settings: %1")
             .arg(settingsDir.path()),
             LogError );

        return false;
    }

    return true;
}

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
    return path.left(i) + QLatin1String(suffix);
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
    const QString optionName = geometryOptionName(*w, openOnCurrentScreen);
    const QString tag = resolutionTag(*w, openOnCurrentScreen);
    QByteArray geometry = geometryOptionValue(optionName + tag).toByteArray();

    // If geometry for screen resolution doesn't exist, use last saved one.
    const bool restoreUntaggedGeometry = geometry.isEmpty();
    if (restoreUntaggedGeometry) {
        geometry = geometryOptionValue(optionName).toByteArray();

        // If geometry for the screen doesn't exist, move window to the middle of the screen.
        if (geometry.isEmpty()) {
            const QRect availableGeometry = screenAvailableGeometry(*w);
            if ( availableGeometry.isValid() ) {
                const QPoint position = availableGeometry.center() - w->rect().center();
                w->move(position);
            }
        }
    }

    const QRect oldGeometry = w->geometry();
    if ( !geometry.isEmpty() )
        w->restoreGeometry(geometry);

    ensureWindowOnScreen(w);

    const QRect newGeometry = w->geometry();
    GEOMETRY_LOG( w,
        QStringLiteral("%5 geometry \"%1%2\": %3 -> %4").arg(
            optionName,
            restoreUntaggedGeometry ? QString() : tag,
            toString(oldGeometry),
            toString(newGeometry),
            geometry.isEmpty() ? QLatin1String("New") : QLatin1String("Restore")) );
}

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    const QString optionName = geometryOptionName(*w, openOnCurrentScreen);
    const QString tag = resolutionTag(*w, openOnCurrentScreen);
    QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    const auto geometry = w->saveGeometry();
    geometrySettings.setValue(optionName + tag, geometry);
    geometrySettings.setValue(optionName, geometry);
    geometrySettings.setValue(geometryOptionName(*w), geometry);
    GEOMETRY_LOG( w, QStringLiteral("Save geometry \"%1%2\": %3")
                  .arg(optionName, tag, toString(w->geometry())) );
}

QByteArray mainWindowState(const QString &mainWindowObjectName)
{
    const QString optionName = QStringLiteral("Options/%1_state").arg(mainWindowObjectName);
    return geometryOptionValue(optionName).toByteArray();
}

void saveMainWindowState(const QString &mainWindowObjectName, const QByteArray &state)
{
    const QString optionName = QStringLiteral("Options/%1_state").arg(mainWindowObjectName);
    setGeometryOptionValue(optionName, state);
}

void moveToCurrentWorkspace(QWidget *w)
{
#ifdef COPYQ_MOVE_TO_WORKSPACE
    /* Re-initialize window in window manager so it can popup on current workspace. */
    if (w->isVisible()) {
        GEOMETRY_LOG( w, QLatin1String("Move to current workspace") );
        const bool blockUntilHide = isGeometryGuardBlockedUntilHidden(w);
        w->hide();
        if (blockUntilHide)
            setGeometryGuardBlockedUntilHidden(w, true);
    }
#else
    Q_UNUSED(w)
#endif
}

void moveWindowOnScreen(QWidget *w, QPoint pos)
{
    w->move(pos);
    ensureWindowOnScreen(w);
    moveToCurrentWorkspace(w);
}

void setGeometryGuardBlockedUntilHidden(QWidget *w, bool blocked)
{
    GEOMETRY_LOG( w, QStringLiteral("Geometry blocked until hidden: %1").arg(blocked) );
    w->setProperty(propertyGeometryLockedUntilHide, blocked);
}

bool isGeometryGuardBlockedUntilHidden(const QWidget *w)
{
    return w->property(propertyGeometryLockedUntilHide).toBool();
}
