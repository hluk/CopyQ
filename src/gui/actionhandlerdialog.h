// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QDialog>

class ActionHandler;
class QAbstractItemModel;

namespace Ui {
class ActionHandlerDialog;
}

class ActionHandlerDialog final : public QDialog
{
public:
    explicit ActionHandlerDialog(ActionHandler *actionHandler, QAbstractItemModel *model, QWidget *parent = nullptr);
    ~ActionHandlerDialog();

private:
    Ui::ActionHandlerDialog *ui;
};
