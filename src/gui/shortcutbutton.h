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

#ifndef SHORTCUTBUTTON_H
#define SHORTCUTBUTTON_H

#include <QKeySequence>
#include <QList>
#include <QToolBar>

class QAction;
class QHBoxLayout;

/**
 * Widget with buttons for defining shortcuts and single button for adding shortcuts.
 */
class ShortcutButton final : public QToolBar
{
    Q_OBJECT
public:
    explicit ShortcutButton(QWidget *parent = nullptr);

    /** Creates new shortcut button for @a shortcut if it's valid and same button doesn't exist. */
    void addShortcut(const QKeySequence &shortcut);

    /**
     * Overloaded method.
     *
     * Creates new shortcut from string formatted as QKeySequence::PortableText (if valid).
     */
    void addShortcut(const QString &shortcutPortableText);

    /** Remove all shortcut buttons. */
    void clearShortcuts();

    /** Remove all shortcut buttons and add button with default shortcut if valid. */
    void resetShortcuts();

    /** Set default shortcut (after reset()). */
    void setDefaultShortcut(const QKeySequence &defaultShortcut);

    /** Return valid shortcuts defined by buttons. */
    QList<QKeySequence> shortcuts() const;

    /** Add icon and tooltip to buttons that contain shortcut from @a ambiguousShortcuts list. */
    void checkAmbiguousShortcuts(const QList<QKeySequence> &ambiguousShortcuts,
                                 const QIcon &warningIcon, const QString &warningToolTip);

signals:
    /** Emitted if new @a shortcut (with button) was added. */
    void shortcutAdded(const QKeySequence &shortcut);
    /** Emitted if @a shortcut (with button) was removed. */
    void shortcutRemoved(const QKeySequence &shortcut);

protected:
    void showEvent(QShowEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void onShortcutButtonClicked();
    void onButtonAddShortcutClicked();
    void addShortcut(QAction *shortcutButton);
    void setButtonShortcut(QAction *shortcutButton, const QKeySequence &shortcut);

    QKeySequence shortcutForButton(const QAction &w) const;

    bool focusNextPrevious(bool next);

    QKeySequence m_defaultShortcut;
    QAction *m_actionAddShortcut;
};

#endif // SHORTCUTBUTTON_H
