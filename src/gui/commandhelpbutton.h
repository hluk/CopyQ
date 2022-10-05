// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMANDHELPBUTTON_H
#define COMMANDHELPBUTTON_H

#include <QWidget>

class QToolButton;
class QDialog;

class CommandHelpButton final : public QWidget
{
    Q_OBJECT
public:
    explicit CommandHelpButton(QWidget *parent = nullptr);

public:
    void showHelp();

signals:
    void hidden();

private:
    QToolButton *m_button;
    QDialog *m_help;
};

#endif // COMMANDHELPBUTTON_H
