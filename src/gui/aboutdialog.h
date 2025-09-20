// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QDialog>

namespace Ui {
    class AboutDialog;
}

class AboutDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog();

private:
    static QString aboutPage();

    Ui::AboutDialog *ui;
};
