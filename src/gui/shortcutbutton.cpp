/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

ShortcutButton::ShortcutButton(QWidget *parent)
    : QWidget(parent)
    , m_defaultShortcut()
    , m_layout(new QHBoxLayout(this))
    , m_buttonAddShortcut(new QPushButton(this))
    , m_expectModifier(false)
{
    m_layout->setMargin(0);
    m_layout->setSpacing(2);
    m_layout->setAlignment(Qt::AlignRight);

    m_buttonAddShortcut->setFlat(true);
    m_buttonAddShortcut->setToolTip( tr("Add shortcut") );
    m_layout->addWidget(m_buttonAddShortcut);

    connect( m_buttonAddShortcut, SIGNAL(clicked()),
             this, SLOT(onButtonAddShortcutClicked()) );

    addShortcut(m_defaultShortcut);
}

void ShortcutButton::addShortcut(const QKeySequence &shortcut)
{
    if ( shortcut.isEmpty() || shortcuts().contains(shortcut) )
        return;

    QPushButton *button = new QPushButton(this);
    m_layout->insertWidget( shortcutCount(), button, 1 );
    connect( button, SIGNAL(clicked()),
             this, SLOT(onShortcutButtonClicked()) );
    button->setText( shortcut.toString(QKeySequence::NativeText) );
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
    while ( shortcutCount() > 0 ) {
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

void ShortcutButton::setDefaultShortcut(const QKeySequence &defaultShortcut)
{
    m_defaultShortcut = defaultShortcut;
}

QList<QKeySequence> ShortcutButton::shortcuts() const
{
    QList<QKeySequence> shortcuts;

    for ( int i = 0; i < shortcutCount(); ++i ) {
        QWidget *w = shortcutButton(i);
        shortcuts.append( shortcutForButton(*w) );
    }

    return shortcuts;
}

void ShortcutButton::updateIcons()
{
    const QColor color = getDefaultIconColor(*m_buttonAddShortcut, QPalette::Window);
    m_buttonAddShortcut->setIcon( getIcon("list-add", IconPlus, color, color) );
    const int h = m_buttonAddShortcut->sizeHint().height();
    m_buttonAddShortcut->setMaximumSize(h, h);
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

int ShortcutButton::shortcutCount() const
{
    return m_layout->count() - 1;
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
                if (shortcutButton != NULL) {
                    const QString shortcut = shortcutButton->text();
                    delete shortcutButton;
                    emit shortcutRemoved(shortcut);
                }
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

QWidget *ShortcutButton::shortcutButton(int index) const
{
    return m_layout->itemAt(index)->widget();
}

QKeySequence ShortcutButton::shortcutForButton(const QWidget &w) const
{
    return QKeySequence(w.property("text").toString(), QKeySequence::NativeText);
}
