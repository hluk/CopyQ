/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include <QSharedPointer>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace Ui {
class ShortcutsWidget;
}

class ShortcutButton;
class QSettings;
class QTableWidgetItem;

struct MenuAction {
    QString iconName;
    ushort iconId;
    QString text;
    QString settingsKey;
    QTableWidgetItem *tableItem;
    ShortcutButton *shortcutButton;
};

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

protected:
    void showEvent(QShowEvent *event);

private slots:
    void onShortcutAdded(const QKeySequence &shortcut);
    void onShortcutRemoved(const QKeySequence &shortcut);
    void checkAmbiguousShortcuts();

    void on_lineEditFilter_textChanged(const QString &text);

private:
    Ui::ShortcutsWidget *ui;
    QTimer m_timerCheckAmbiguous;

    QVector<MenuAction> m_actions;
    QList<QKeySequence> m_shortcuts;
};

#endif // SHORTCUTSWIDGET_H
