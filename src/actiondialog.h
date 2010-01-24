#ifndef ACTIONDIALOG_H
#define ACTIONDIALOG_H

#include <QDialog>

namespace Ui {
    class ActionDialog;
}

class ActionDialog : public QDialog {
    Q_OBJECT
public:
    ActionDialog(QWidget *parent = 0);
    ~ActionDialog();
    void setInput(QString input);

protected:
    void changeEvent(QEvent *e);

private:
    Ui::ActionDialog *ui;

signals:
    void addItems(const QStringList);
    void error(const QString);

public slots:
    void accept();
};

#endif // ACTIONDIALOG_H
