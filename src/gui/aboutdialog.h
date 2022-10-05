// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

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

#endif // ABOUTDIALOG_H
