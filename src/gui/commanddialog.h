// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMANDDIALOG_H
#define COMMANDDIALOG_H

#include "common/command.h"
#include "common/commandstore.h"

#include <QDialog>

namespace Ui {
class CommandDialog;
}

class QAbstractButton;

class CommandDialog final : public QDialog
{
    Q_OBJECT

    friend class CommandItem;

public:
    CommandDialog(
            const Commands &pluginCommands, const QStringList &formats,
            QWidget *parent = nullptr);
    ~CommandDialog();

    /** Create new commands. */
    void addCommands(const Commands &commands);

    void apply();

    bool maybeClose(QWidget *saveMessageBoxParent);

    void reject() override;

signals:
    void commandsSaved();

private:
    void tryPasteCommandFromClipboard();
    void copySelectedCommandsToClipboard();
    void onCommandDropped(const QString &text, int row);
    void onCommandEnabledDisabled(int row);

    void onCurrentCommandWidgetIconChanged();
    void onCurrentCommandWidgetNameChanged(const QString &name);

    void onFinished(int result);

    void onAddCommands(const QVector<Command> &commands);

    void onCommandTextChanged(const QString &command);

    void onItemOrderListCommandsAddButtonClicked();
    void onItemOrderListCommandsItemSelectionChanged();
    void onPushButtonLoadCommandsClicked();
    void onPushButtonSaveCommandsClicked();
    void onPushButtonCopyCommandsClicked();
    void onPushButtonPasteCommandsClicked();
    void onLineEditFilterCommandsTextChanged(const QString &text);
    void onButtonBoxClicked(QAbstractButton* button);

    Command currentCommand(int row) const;
    Commands currentCommands() const;

    void addCommandsWithoutSave(const Commands &commands, int targetRow);
    Commands selectedCommands() const;
    QString serializeSelectedCommands();
    bool hasUnsavedChanges() const;

    void updateIcon(int row);

    QString commandsToPaste();

    Ui::CommandDialog *ui;
    Commands m_savedCommands;

    Commands m_pluginCommands;
    QStringList m_formats;
};

#endif // COMMANDDIALOG_H
