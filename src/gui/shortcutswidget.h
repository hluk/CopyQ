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

#ifndef SHORTCUTSWIDGET_H
#define SHORTCUTSWIDGET_H

#include <QIcon>
#include <QMap>
#include <QSharedPointer>
#include <QWidget>
#include <QTimer>

namespace Ui {
class ShortcutsWidget;
}

class QSettings;

class MenuAction;

/**
 * Widget with list of modifiable shortcuts and filter field.
 */
class ShortcutsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ShortcutsWidget(QWidget *parent = NULL);

    ~ShortcutsWidget();

    /** Load shortcuts from settings file. */
    void loadShortcuts(QSettings &settings);
    /** Save shortcuts to settings file. */
    void saveShortcuts(QSettings &settings) const;

    /** Create action with @a id (must be unique) and add it to the list. */
    void addAction(int id, const QString &text, const QString &settingsKey,
                   const QKeySequence &shortcut = QKeySequence());

    /** Return true if action with given @a id is in the list. */
    bool hasAction(int id) const { return m_actions.contains(id); }

    /** Return action with given @a id (must exist), set context and return the action. */
    QAction *action(int id, QWidget *parent, Qt::ShortcutContext context);

    /** Return list of shortcuts defined for given @a id (must exist). */
    QList<QKeySequence> shortcuts(int id) const;

    void updateIcons(int id, const QIcon &icon = QIcon());

    /** Disable shortcuts for actions that are currently in list. */
    void setDisabledShortcuts(const QList<QKeySequence> &shortcuts);

protected:
    void showEvent(QShowEvent *event);

private slots:
    void onShortcutAdded(const QKeySequence &shortcut);
    void onShortcutRemoved(const QKeySequence &shortcut);
    void checkAmbiguousShortcuts();

    void on_lineEditFilter_textChanged(const QString &text);

private:
    MenuAction *action(int id) const;

    Ui::ShortcutsWidget *ui;
    QTimer m_timerCheckAmbiguous;

    typedef QSharedPointer<MenuAction> MenuActionPtr;
    QMap<int, MenuActionPtr> m_actions;
    QList<QKeySequence> m_shortcuts;
};

#endif // SHORTCUTSWIDGET_H
