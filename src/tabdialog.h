#ifndef TAGDIALOG_H
#define TAGDIALOG_H

#include <QDialog>

namespace Ui {
    class TabDialog;
}

class TabDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TabDialog(QWidget *parent = 0);
    ~TabDialog();

private:
    Ui::TabDialog *ui;

signals:
    void addTab(const QString name);

private slots:
    void onAccepted();
};

#endif // TAGDIALOG_H
