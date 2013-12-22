/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/shortcutdialog.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVariant>

ShortcutButton::ShortcutButton(const QKeySequence &defaultShortcut, QWidget *parent)
    : QWidget(parent)
    , m_defaultShortcut(defaultShortcut)
    , m_layout(new QHBoxLayout(this))
    , m_buttonAddShortcut(new QPushButton(this))
    , m_expectModifier(false)
{
    m_layout->setMargin(0);
    m_layout->setSpacing(2);
    m_layout->setAlignment(Qt::AlignRight);

    m_layout->addWidget(m_buttonAddShortcut);

    m_buttonAddShortcut->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    connect( m_buttonAddShortcut, SIGNAL(clicked()),
             this, SLOT(onButtonAddShortcutClicked()) );

    addShortcut(m_defaultShortcut);

    setFocusPolicy(Qt::WheelFocus);
}

void ShortcutButton::addShortcut(const QKeySequence &shortcut)
{
    if ( shortcut.isEmpty() || shortcuts().contains(shortcut) )
        return;

    QPushButton *button = new QPushButton(this);
    button->setFlat(true);
    m_layout->insertWidget( shortcutButtonCount(), button, 1 );
    connect( button, SIGNAL(clicked()),
             this, SLOT(onShortcutButtonClicked()) );
    button->setText( shortcut.toString(QKeySequence::NativeText) );
    emit shortcutAdded(shortcut);
}

void ShortcutButton::clearShortcuts()
{
    while ( shortcutButtonCount() > 0 ) {
        QWidget *w = shortcutButton(0);
        emit shortcutRemoved( shortcutForButton(*w) );
        delete w;
    }
}

void ShortcutButton::resetShortcuts()
{
    clearShortcuts();
    addShortcut(m_defaultShortcut);
}

QList<QKeySequence> ShortcutButton::shortcuts() const
{
    QList<QKeySequence> shortcuts;

    for ( int i = 0; i < shortcutButtonCount(); ++i ) {
        QWidget *w = shortcutButton(i);
        shortcuts.append( shortcutForButton(*w) );
    }

    return shortcuts;
}

void ShortcutButton::updateIcons()
{
    const QColor color = getDefaultIconColor(*m_buttonAddShortcut, QPalette::Window);
    m_buttonAddShortcut->setIcon( getIcon("list-add", IconPlus, color, color) );
}

void ShortcutButton::checkAmbiguousShortcuts(const QList<QKeySequence> &ambiguousShortcuts,
                                             const QIcon &warningIcon, const QString &warningToolTip)
{
    QList<QKeySequence> shortcuts = this->shortcuts();
    for ( int i = 0; i < shortcuts.size(); ++i ) {
        QWidget *w = shortcutButton(i);
        if ( ambiguousShortcuts.contains(shortcuts[i]) ) {
            w->setProperty("icon", warningIcon);
            w->setProperty("toolTip", warningToolTip);
        } else if ( w->property("toolTip").toString() == warningToolTip ) {
            w->setProperty("icon", QIcon());
            w->setProperty("toolTip", QString());
        }
    }
}

bool ShortcutButton::focusNextPrevChild(bool next)
{
    if ( m_buttonAddShortcut->hasFocus() ) {
        if (next || shortcutButtonCount() == 0)
            return QWidget::focusNextPrevChild(next);
        shortcutButton(shortcutButtonCount() - 1)->setFocus();
        return true;
    } else if (shortcutButtonCount() == 0) {
        m_buttonAddShortcut->setFocus();
        return true;
    }

    int current = -1;
    for ( int i = 0; i < shortcutButtonCount(); ++i ) {
        if ( shortcutButton(i)->hasFocus() ) {
            current = i;
            break;
        }
    }

    if (current == 0 && !next)
        return QWidget::focusNextPrevChild(next);

    if (current == shortcutButtonCount() - 1 && next) {
        m_buttonAddShortcut->setFocus();
    } else if (current == -1 && !next) {
        shortcutButton(shortcutButtonCount() - 1)->setFocus();
    } else {
        shortcutButton(current + (next ? 1 : -1))->setFocus();
    }

    return true;
}

void ShortcutButton::onShortcutButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    Q_ASSERT(button != NULL);
    addShortcut(button);
}

void ShortcutButton::onButtonAddShortcutClicked()
{
    addShortcut(NULL);
}

void ShortcutButton::addShortcut(QPushButton *shortcutButton)
{
    QWidget *parent = this;
    // Destroy shortcut dialog, if its shortcut button is deleted.
    if (shortcutButton != NULL)
        parent = shortcutButton;

    ShortcutDialog *dialog = new ShortcutDialog(parent);
    dialog->setExpectModifier(m_expectModifier);

    if (dialog->exec() == QDialog::Accepted) {
        const QKeySequence shortcut = dialog->shortcut();
        const QString text = shortcut.toString(QKeySequence::NativeText);
        if ( shortcut.isEmpty() || shortcuts().contains(shortcut) ) {
            if (shortcutButton == NULL || shortcutButton->text() != text ) {
                if (shortcutButton != NULL)
                    emit shortcutRemoved(shortcutButton->text());
                delete shortcutButton;
            }
        } else {
            if (shortcutButton != NULL) {
                if ( shortcutButton->text() != text ) {
                    emit shortcutRemoved(shortcutButton->text());
                    shortcutButton->setText(text);
                    emit shortcutAdded(text);
                }
            } else {
                addShortcut(text);
            }
        }
    }
}

int ShortcutButton::shortcutButtonCount() const
{
    return m_layout->count() - 1;
}

QWidget *ShortcutButton::shortcutButton(int index) const
{
    return m_layout->itemAt(index)->widget();
}

QKeySequence ShortcutButton::shortcutForButton(const QWidget &w) const
{
    return QKeySequence(w.property("text").toString(), QKeySequence::NativeText);
}
