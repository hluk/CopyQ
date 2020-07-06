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

#include "gui/shortcutbutton.h"

#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/shortcutdialog.h"

#include <QAction>
#include <QToolButton>
#include <QVariant>

namespace {

const char propertyShortcut[] = "CopyQ_shortcut";

} // namespace

ShortcutButton::ShortcutButton(QWidget *parent)
    : QToolBar(parent)
    , m_defaultShortcut()
{
    setFocusPolicy(Qt::WheelFocus);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_actionAddShortcut = addAction(QString());
    m_actionAddShortcut->setToolTip( tr("Add shortcut") );
    connect( m_actionAddShortcut, &QAction::triggered,
             this, &ShortcutButton::onButtonAddShortcutClicked );

    addShortcut(m_defaultShortcut);
}

void ShortcutButton::addShortcut(const QKeySequence &shortcut)
{
    const auto shortcuts = this->shortcuts();
    if ( shortcut.isEmpty() || shortcuts.contains(shortcut) )
        return;

    auto button = new QAction(this);
    insertAction(m_actionAddShortcut, button);
    connect( button, &QAction::triggered,
             this, &ShortcutButton::onShortcutButtonClicked );
    setButtonShortcut(button, shortcut);

    // Non-flat buttons
    auto toolButton = qobject_cast<QToolButton *>(widgetForAction(button));
    if (toolButton)
        toolButton->setAutoRaise(false);

    emit shortcutAdded(shortcut);
}

void ShortcutButton::addShortcut(const QString &shortcutPortableText)
{
    QKeySequence shortcut(shortcutPortableText, QKeySequence::PortableText);
    if ( !shortcut.isEmpty() )
        addShortcut(shortcut);
}

void ShortcutButton::clearShortcuts()
{
    for (auto action : actions()) {
        if (action == m_actionAddShortcut)
            continue;

        emit shortcutRemoved( shortcutForButton(*action) );
        action->deleteLater();
    }
}

void ShortcutButton::resetShortcuts()
{
    clearShortcuts();
    addShortcut(m_defaultShortcut);
}

void ShortcutButton::setDefaultShortcut(const QKeySequence &defaultShortcut)
{
    m_defaultShortcut = defaultShortcut;
}

QList<QKeySequence> ShortcutButton::shortcuts() const
{
    QList<QKeySequence> shortcuts;

    for (auto action : actions()) {
        if (action == m_actionAddShortcut)
            continue;

        shortcuts.append( shortcutForButton(*action) );
    }

    return shortcuts;
}

void ShortcutButton::checkAmbiguousShortcuts(const QList<QKeySequence> &ambiguousShortcuts,
                                             const QIcon &warningIcon, const QString &warningToolTip)
{
    QList<QKeySequence> shortcuts = this->shortcuts();
    for (auto action : actions()) {
        if ( ambiguousShortcuts.contains( shortcutForButton(*action) ) ) {
            action->setProperty("icon", warningIcon);
            action->setProperty("toolTip", warningToolTip);
        } else if ( action->property("toolTip").toString() == warningToolTip ) {
            action->setProperty("icon", QIcon());
            action->setProperty("toolTip", QString());
        }
    }
}

void ShortcutButton::showEvent(QShowEvent *event)
{
    if ( m_actionAddShortcut->icon().isNull() )
        m_actionAddShortcut->setIcon( getIcon("list-add", IconPlus) );

    QWidget::showEvent(event);
}

void ShortcutButton::focusInEvent(QFocusEvent *event)
{
    QToolBar::focusInEvent(event);

    if ( !hasFocus() )
        return;

    focusNextPrevChild(true);
}

bool ShortcutButton::focusNextPrevChild(bool next)
{
    const QList<QAction*> actions = this->actions();

    if ( actions.isEmpty() )
        return false;

    auto w = focusWidget();
    if (!w || w == this) {
        w = widgetForAction( next ? actions.first() : actions.last() );
    } else if (w && w->hasFocus()) {
        auto it = std::find_if(std::begin(actions), std::end(actions), [&](QAction *action) {
            return widgetForAction(action) == w;
        });
        if (next && it == std::end(actions))
            return focusNextPrevious(next);

        if (!next && it == std::begin(actions))
            return focusNextPrevious(next);

        if (next)
            ++it;
        else
            --it;

        if (it == std::end(actions))
            return false;

        w = widgetForAction(*it);
    }

    if (!w)
        return false;

    if (!w->isVisible())
        return focusNextPrevious(next);

    w->setFocus();

    return true;
}

void ShortcutButton::onShortcutButtonClicked()
{
    QAction *button = qobject_cast<QAction*>(sender());
    Q_ASSERT(button != nullptr);
    addShortcut(button);
}

void ShortcutButton::onButtonAddShortcutClicked()
{
    addShortcut(nullptr);
}

void ShortcutButton::addShortcut(QAction *shortcutButton)
{
    auto dialog = new ShortcutDialog(this);
    if (dialog->exec() == QDialog::Rejected)
        return;

    const QKeySequence newShortcut = dialog->shortcut();
    const QKeySequence oldShortcut = shortcutButton
            ? shortcutForButton(*shortcutButton)
            : QKeySequence();

    if (oldShortcut == newShortcut)
        return;

    // Remove shortcut button if shortcut is removed, unrecognized or already set.
    if ( newShortcut.isEmpty() || shortcuts().contains(newShortcut) ) {
        if (shortcutButton) {
            delete shortcutButton;
            emit shortcutRemoved(oldShortcut);
        }
    } else if (shortcutButton) {
        emit shortcutRemoved(oldShortcut);
        setButtonShortcut(shortcutButton, newShortcut);
        emit shortcutAdded(newShortcut);
    } else {
        addShortcut(newShortcut);
    }
}

void ShortcutButton::setButtonShortcut(QAction *shortcutButton, const QKeySequence &shortcut)
{
    QString label = shortcut.toString(QKeySequence::NativeText);
    label.replace( QChar('&'), QString("&&") );
    shortcutButton->setText(label);
    shortcutButton->setProperty(propertyShortcut, shortcut);
}

QKeySequence ShortcutButton::shortcutForButton(const QAction &w) const
{
    return w.property(propertyShortcut).value<QKeySequence>();
}

bool ShortcutButton::focusNextPrevious(bool next)
{
    auto w = next ? nextInFocusChain() : previousInFocusChain();
    if (w) {
        w->setFocus();
        return true;
    }
    return false;
}
