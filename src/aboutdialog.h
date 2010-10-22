#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include "configurationmanager.h"
#include <QDialog>

namespace Ui {
    class AboutDialog;
}

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = 0);
    ~AboutDialog();

private:
    Ui::AboutDialog *ui;

protected:
    void showEvent(QShowEvent *);

private slots:
    void onFinished(int result);
};

#endif // ABOUTDIALOG_H
