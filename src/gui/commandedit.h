// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMANDEDIT_H
#define COMMANDEDIT_H

#include <QWidget>

namespace Ui {
class CommandEdit;
}

class CommandEdit final : public QWidget
{
    Q_OBJECT

public:
    explicit CommandEdit(QWidget *parent = nullptr);
    ~CommandEdit();

    void setCommand(const QString &command) const;
    QString command() const;

    bool isEmpty() const;

    void setReadOnly(bool readOnly);

signals:
    void changed();
    void commandTextChanged(const QString &command);

private:
    void onPlainTextEditCommandTextChanged();

    Ui::CommandEdit *ui;
};

#endif // COMMANDEDIT_H
