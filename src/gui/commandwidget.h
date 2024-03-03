// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMANDWIDGET_H
#define COMMANDWIDGET_H

#include <QWidget>

namespace Ui {
class CommandWidget;
}

class QComboBox;
struct Command;

/** Widget (set of widgets) for creating or modifying Command object. */
class CommandWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit CommandWidget(QWidget *parent = nullptr);
    ~CommandWidget();

    /** Return command for the widget. */
    Command command() const;

    /** Set current command. */
    void setCommand(const Command &c);

    /** Set formats for format selection combo boxes. */
    void setFormats(const QStringList &formats);

signals:
    void iconChanged();

    void nameChanged(const QString &name);

    void commandTextChanged(const QString &command);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void onLineEditNameTextChanged(const QString &text);

    void onButtonIconCurrentIconChanged();

    void onCheckBoxShowAdvancedStateChanged(int state);

    void onCommandEditCommandTextChanged(const QString &command);

    void init();

    void updateWidgets();

    void updateShowAdvanced();

    void setShowAdvanced(bool showAdvanced);

    void emitIconChanged();

    QString description() const;

    Ui::CommandWidget *ui;
    bool m_showAdvanced = true;
    QString m_internalId;
};

#endif // COMMANDWIDGET_H
