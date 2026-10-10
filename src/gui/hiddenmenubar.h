// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

class QAction;
class QMainWindow;
class QMenu;
class QMenuBar;
class QPoint;
class QWidget;

/**
 * Behavior of the main window menu bar with the "hide menu bar" option.
 *
 * The menu bar is collapsed by overriding its maximum height instead of
 * hiding it, so Alt+<letter> still opens the menu (the menu bar is expanded
 * first) and the arrow keys still move between the menus. It collapses again
 * once it loses focus and no menu is open.
 */
class HiddenMenuBar final : public QObject
{
    Q_OBJECT

public:
    explicit HiddenMenuBar(QMainWindow *window);

    /** Enable or disable the option (collapses or restores the menu bar). */
    void setEnabled(bool enabled);

    /** True if the option is on and the menu bar is currently collapsed. */
    bool isCollapsed() const;

    /** Expand the menu bar and activate its first menu for keyboard use. */
    void show();

    /** Give the focus back to where it was before the menu bar. */
    void leave();

    /** Actions that stand in for the menu bar in context menus. */
    void setFallbackActions(const QList<QAction*> &actions);

    /** Append the stand-in actions to a menu while the menu bar is collapsed. */
    void addFallbackActions(QMenu *menu) const;

    /** Show a menu with only the stand-in actions. */
    void showFallbackMenu(QPoint position);

    /** Collapse the menu bar once the focus settles, unless it is in use. */
    void collapseLaterIfUnused();

signals:
    void collapsedChanged();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void collapse();
    void expand();
    void collapseIfUnused();
    bool isMnemonicOfMenu(int key) const;

    QMainWindow *m_window;
    QMenuBar *m_menuBar;
    bool m_enabled = false;
    bool m_collapsed = false;
    bool m_altPressed = false;
    QPointer<QWidget> m_focusBefore;
    QList<QPointer<QAction>> m_fallbackActions;
};
