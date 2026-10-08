// SPDX-License-Identifier: GPL-3.0-or-later

#include "hiddenmenubar.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLayout>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>

HiddenMenuBar::HiddenMenuBar(QMainWindow *window)
    : QObject(window)
    , m_window(window)
    , m_menuBar(window->menuBar())
{
    connect( qApp, &QApplication::focusChanged,
             this, &HiddenMenuBar::collapseLaterIfUnused );

    for ( const QAction *action : m_menuBar->actions() ) {
        if (action->menu()) {
            connect( action->menu(), &QMenu::aboutToHide,
                     this, &HiddenMenuBar::collapseLaterIfUnused );
        }
    }
}

void HiddenMenuBar::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    if (enabled) {
        qApp->installEventFilter(this);
        m_window->centralWidget()->installEventFilter(this);
        collapse();
    } else {
        qApp->removeEventFilter(this);
        m_window->centralWidget()->removeEventFilter(this);
        expand();
    }
}

bool HiddenMenuBar::isCollapsed() const
{
    return m_collapsed;
}

void HiddenMenuBar::show()
{
    if ( !m_menuBar->hasFocus() )
        m_focusBefore = QApplication::focusWidget();

    expand();

    // Activating the first menu also gives the menu bar the keyboard focus.
    const auto actions = m_menuBar->actions();
    if ( !actions.isEmpty() )
        m_menuBar->setActiveAction( actions.first() );
}

void HiddenMenuBar::leave()
{
    m_menuBar->setActiveAction(nullptr);

    if (m_focusBefore && m_focusBefore->isVisible())
        m_focusBefore->setFocus();
    else
        m_menuBar->clearFocus();

    collapseLaterIfUnused();
}

void HiddenMenuBar::setFallbackActions(const QList<QAction*> &actions)
{
    m_fallbackActions.clear();
    for (QAction *action : actions)
        m_fallbackActions.append(action);
}

void HiddenMenuBar::addFallbackActions(QMenu *menu) const
{
    if (!m_collapsed)
        return;

    // Copies, because menus rebuilt with new actions delete the old ones.
    if ( !menu->isEmpty() )
        menu->addSeparator();

    for (const auto &action : m_fallbackActions) {
        if (!action)
            continue;
        QAction *copy = menu->addAction( action->icon(), action->text() );
        connect(copy, &QAction::triggered, action, &QAction::trigger);
    }
}

void HiddenMenuBar::showFallbackMenu(QPoint position)
{
    if (!m_collapsed)
        return;

    QMenu menu(m_window);
    addFallbackActions(&menu);
    menu.exec(position);
}

void HiddenMenuBar::collapseLaterIfUnused()
{
    if (!m_enabled)
        return;

    // Wait for the focus to settle: a menu closes before its action is
    // triggered and before the focus returns to the menu bar.
    QTimer::singleShot(0, this, &HiddenMenuBar::collapseIfUnused);
}

bool HiddenMenuBar::eventFilter(QObject *object, QEvent *event)
{
    switch ( event->type() ) {
    case QEvent::ShortcutOverride: {
        // Alt+<letter> opens the menu with that mnemonic, so the menu bar
        // has to be in place before the menu pops up.
        const auto keyEvent = static_cast<QKeyEvent*>(event);
        if ( m_collapsed && QApplication::activeWindow() == m_window
             && keyEvent->modifiers() == Qt::AltModifier
             && isMnemonicOfMenu(keyEvent->key()) )
        {
            m_focusBefore = QApplication::focusWidget();
            expand();
        }
        break;
    }
    case QEvent::KeyPress: {
        const auto keyEvent = static_cast<QKeyEvent*>(event);
        if ( !keyEvent->isAutoRepeat() ) {
            m_altPressed = keyEvent->key() == Qt::Key_Alt
                && (keyEvent->modifiers() & ~Qt::AltModifier) == Qt::NoModifier;
        }

        if ( object == m_menuBar && keyEvent->key() == Qt::Key_Escape
             && !QApplication::activePopupWidget() )
        {
            leave();
            return true;
        }
        break;
    }
    case QEvent::KeyRelease: {
        // Alt pressed and released on its own shows the menu bar.
        const auto keyEvent = static_cast<QKeyEvent*>(event);
        if ( m_altPressed && keyEvent->key() == Qt::Key_Alt && !keyEvent->isAutoRepeat() ) {
            m_altPressed = false;
            if ( m_collapsed && QApplication::activeWindow() == m_window )
                QTimer::singleShot(0, this, &HiddenMenuBar::show);
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        m_altPressed = false;
        // A click on an area that does not take focus (for example with
        // a tab group selected) would leave the focus in the menu bar.
        const auto widget = qobject_cast<QWidget*>(object);
        if ( widget && m_menuBar->hasFocus() && widget->window() == m_window
             && widget != m_menuBar && !m_menuBar->isAncestorOf(widget) )
        {
            leave();
        }
        break;
    }
    case QEvent::ContextMenu: {
        // The main window prevents context menus, so the one for the empty
        // area while the menu bar is collapsed is caught on the central widget.
        if ( object == m_window->centralWidget() && m_collapsed ) {
            showFallbackMenu( static_cast<QContextMenuEvent*>(event)->globalPos() );
            event->accept();
            return true;
        }
        break;
    }
    case QEvent::Wheel:
    case QEvent::WindowDeactivate:
        m_altPressed = false;
        break;
    default:
        break;
    }

    return false;
}

void HiddenMenuBar::collapse()
{
    if (m_collapsed)
        return;

    m_collapsed = true;
    m_menuBar->setMaximumHeight(0);
    emit collapsedChanged();
}

void HiddenMenuBar::expand()
{
    if (!m_collapsed)
        return;

    m_collapsed = false;
    m_menuBar->setMaximumHeight(QWIDGETSIZE_MAX);
    // The menu pops up at the position of the menu, which needs the layout.
    m_window->layout()->activate();
    emit collapsedChanged();
}

void HiddenMenuBar::collapseIfUnused()
{
    if (!m_enabled || m_collapsed)
        return;

    if ( m_menuBar->hasFocus() || m_menuBar->activeAction() || QApplication::activePopupWidget() )
        return;

    collapse();
}

bool HiddenMenuBar::isMnemonicOfMenu(int key) const
{
    const QKeySequence pressed(Qt::ALT | key);
    for ( const QAction *action : m_menuBar->actions() ) {
        if ( QKeySequence::mnemonic(action->text()) == pressed )
            return true;
    }
    return false;
}
