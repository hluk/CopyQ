// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACTIONDIALOG_H
#define ACTIONDIALOG_H

#include <QDialog>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QRegularExpression>
#include <QVariantMap>

class QAbstractButton;
struct Command;

namespace Ui {
    class ActionDialog;
}

/** Dialog class for executing Command objects. */
class ActionDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ActionDialog(QWidget *parent = nullptr);
    ~ActionDialog();

    /** Restore command history from configuration. */
    void restoreHistory();

    /** Save command history. */
    void saveHistory();

    /** Return filename for storing command history. */
    const QString dataFilename() const;

    /** Set action input data. */
    void setInputData(const QVariantMap &data);
    /** Set command for dialog. */
    void setCommand(const Command &cmd);
    /** Set texts for tabs in combo box. */
    void setOutputTabs(const QStringList &tabs);

    void setCurrentTab(const QString &currentTabName);

    /** Load settings. */
    void loadSettings();

    /** Return current command. */
    Command command() const;

signals:
    /** Emitted if dialog was accepted. */
    void commandAccepted(const Command &command, const QStringList &arguments, const QVariantMap &data);

private:
    void onButtonBoxClicked(QAbstractButton* button);
    void onComboBoxCommandsCurrentIndexChanged(int index);
    void onComboBoxInputFormatCurrentTextChanged(const QString &format);
    void onComboBoxOutputFormatEditTextchanged(const QString &text);
    void onComboBoxOutputTabEditTextChanged(const QString &text);
    void onSeparatorEditTextEdited(const QString &text);

    void previousCommand();
    void nextCommand();

    void acceptCommand();
    QVariant createCurrentItemData();
    void saveCurrentCommandToHistory();

    Ui::ActionDialog *ui;
    QVariantMap m_data;

    int m_currentCommandIndex;
};

#endif // ACTIONDIALOG_H
