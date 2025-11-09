// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QDialog>

class Theme;

namespace Ui {
    class AboutDialog;
}

class AboutDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(const Theme &theme, QWidget *parent = nullptr);
    ~AboutDialog();

private:
    static QString aboutPage(const Theme &theme);

    Ui::AboutDialog *ui;
};
