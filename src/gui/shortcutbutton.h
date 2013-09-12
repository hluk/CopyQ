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

#ifndef SHORTCUTBUTTON_H
#define SHORTCUTBUTTON_H

#include <QKeySequence>
#include <QList>
#include <QWidget>

class QHBoxLayout;
class QPushButton;

class ShortcutButton : public QWidget
{
    Q_OBJECT
public:
    explicit ShortcutButton(const QKeySequence &defaultShortcut, QWidget *parent = NULL);

    /** Expect modifier or accept shortcuts without one. */
    void setExpectModifier(bool expectModifier) { m_expectModifier = expectModifier; }

    void addShortcut(const QKeySequence &shortcut);

    void clearShortcuts();

    void resetShortcuts();

    QList<QKeySequence> shortcuts() const;

    void updateIcons();

    void checkAmbiguousShortcuts(const QList<QKeySequence> &ambiguousShortcuts,
                                 const QIcon &warningIcon, const QString &warningToolTip);

protected:
    bool focusNextPrevChild(bool next);

signals:
    void shortcutAdded(const QKeySequence &shortcut);
    void shortcutRemoved(const QKeySequence &shortcut);

private slots:
    void onShortcutButtonClicked();
    void onButtonAddShortcutClicked();
    void addShortcut(QPushButton *shortcutButton);

    int shortcutButtonCount() const;
    QWidget *shortcutButton(int index) const;

private:
    QKeySequence shortcutForButton(const QWidget &w) const;

    QKeySequence m_defaultShortcut;
    QHBoxLayout *m_layout;
    QPushButton *m_buttonAddShortcut;
    bool m_expectModifier;
};

#endif // SHORTCUTBUTTON_H
