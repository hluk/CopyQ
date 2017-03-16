/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include <QHBoxLayout>
#include <QPushButton>
#include <QVariant>

namespace {

const char propertyShortcut[] = "CopyQ_shortcut";

} // namespace

ShortcutButton::ShortcutButton(QWidget *parent)
    : QWidget(parent)
    , m_defaultShortcut()
    , m_layout(new QHBoxLayout(this))
    , m_buttonAddShortcut(new QPushButton(this))
{
    m_layout->setMargin(0);
    m_layout->setSpacing(2);
    m_layout->setAlignment(Qt::AlignRight);

    m_buttonAddShortcut->setFlat(true);
    m_buttonAddShortcut->setToolTip( tr("Add shortcut") );
    const int h = m_buttonAddShortcut->sizeHint().height();
    m_buttonAddShortcut->setMaximumSize(h, h);
    m_layout->addWidget(m_buttonAddShortcut);
    setFocusProxy(m_buttonAddShortcut);

    connect( m_buttonAddShortcut, SIGNAL(clicked()),
             this, SLOT(onButtonAddShortcutClicked()) );

    addShortcut(m_defaultShortcut);
}

void ShortcutButton::addShortcut(const QKeySequence &shortcut)
{
    if ( shortcut.isEmpty() || shortcuts().contains(shortcut) )
        return;

    auto button = new QPushButton(this);
    const int buttonIndex = shortcutCount();
    m_layout->insertWidget(buttonIndex, button, 1);

    setTabOrder(m_buttonAddShortcut, button);

    connect( button, SIGNAL(clicked()),
             this, SLOT(onShortcutButtonClicked()) );
    setButtonShortcut(button, shortcut);
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

void ShortcutButton::showEvent(QShowEvent *event)
{
    if ( m_buttonAddShortcut->icon().isNull() )
        m_buttonAddShortcut->setIcon( getIcon("list-add", IconPlus) );

    QWidget::showEvent(event);
}

void ShortcutButton::onShortcutButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    Q_ASSERT(button != nullptr);
    addShortcut(button);
}

void ShortcutButton::onButtonAddShortcutClicked()
{
    addShortcut(nullptr);
}

void ShortcutButton::addShortcut(QPushButton *shortcutButton)
{
    QWidget *parent = this;
    // Destroy shortcut dialog, if its shortcut button is deleted.
    if (shortcutButton != nullptr)
        parent = shortcutButton;

    auto dialog = new ShortcutDialog(parent);
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

void ShortcutButton::setButtonShortcut(QPushButton *shortcutButton, const QKeySequence &shortcut)
{
    QString label = shortcut.toString(QKeySequence::NativeText);
    label.replace( QChar('&'), QString("&&") );
    shortcutButton->setText(label);
    shortcutButton->setProperty(propertyShortcut, shortcut);
}

QWidget *ShortcutButton::shortcutButton(int index) const
{
    return m_layout->itemAt(index)->widget();
}

QKeySequence ShortcutButton::shortcutForButton(const QWidget &w) const
{
    return w.property(propertyShortcut).value<QKeySequence>();
}
