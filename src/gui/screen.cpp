#include "screen.h"

#include <QApplication>
#include <QScreen>
#include <QWidget>
#include <QWindow>

namespace {

QScreen *screenFromNumber(int i)
{
    const auto screens = QGuiApplication::screens();
    if (i < 0 || i >= screens.size())
        return nullptr;
    return screens[i];
}

} // namespace

int screenCount()
{
    return QGuiApplication::screens().size();
}

int screenNumberAt(const QPoint &pos)
{
    auto screen = QGuiApplication::screenAt(pos);
    if (screen == nullptr)
        screen = QGuiApplication::primaryScreen();
    return QGuiApplication::screens().indexOf(screen);
}

QRect screenGeometry(int i)
{
    auto screen = screenFromNumber(i);
    return screen ? screen->availableGeometry() : QRect();
}

QRect screenAvailableGeometry(const QWidget &w)
{
    auto screen = QGuiApplication::screenAt(w.pos());
    return screen ? screen->availableGeometry() : screenGeometry(0);
}
