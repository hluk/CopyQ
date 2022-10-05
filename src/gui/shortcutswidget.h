// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHORTCUTSWIDGET_H
#define SHORTCUTSWIDGET_H

#include "common/command.h"

#include <QIcon>
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
    ushort iconId{};
    QString text;

    QString settingsKey;
    Command command;

    QTableWidgetItem *tableItem{};
    ShortcutButton *shortcutButton{};
};

/**
 * Widget with list of modifiable shortcuts and filter field.
 */
class ShortcutsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ShortcutsWidget(QWidget *parent = nullptr);

    ~ShortcutsWidget();

    /** Load shortcuts from settings file. */
    void loadShortcuts(const QSettings &settings);
    /** Save shortcuts to settings file. */
    void saveShortcuts(QSettings *settings);

    void addCommands(const QVector<Command> &commands);

signals:
    void commandsSaved();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void onShortcutAdded(const QKeySequence &shortcut);
    void onShortcutRemoved(const QKeySequence &shortcut);
    void checkAmbiguousShortcuts();

    void onLineEditFilterTextChanged(const QString &text);

    void addShortcutRow(MenuAction &action);

    Ui::ShortcutsWidget *ui;
    QTimer m_timerCheckAmbiguous;

    QVector<MenuAction> m_actions;
    QList<QKeySequence> m_shortcuts;
};

#endif // SHORTCUTSWIDGET_H
