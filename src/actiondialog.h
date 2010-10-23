#ifndef ACTIONDIALOG_H
#define ACTIONDIALOG_H

#include <QDialog>

class QCompleter;
class Action;
class QAbstractButton;

namespace Ui {
    class ActionDialog;
}

class ActionDialog : public QDialog {
    Q_OBJECT
public:
    ActionDialog(QWidget *parent = 0);
    ~ActionDialog();
    void setInput(const QString &input);
    void restoreHistory();
    void saveHistory();
    void add(const QString &command);
    const QString dataFilename() const;

    void setCommand(const QString &cmd);
    void setSeparator(const QString &sep);
    void setInput(bool value);
    void setOutput(bool value);

protected:
    void changeEvent(QEvent *e);
    void showEvent(QShowEvent *e);

private:
    Ui::ActionDialog *ui;
    int m_maxitems;
    QCompleter *m_completer;
    QStringList m_history;
    int m_actions;

signals:
    void addItems(const QStringList &lst);
    void error(const QString &msg);
    void message(const QString &title, const QString &msg);
    void actionFinished(Action *act);
    void addMenuItem(QAction *menuItem);
    void removeMenuItem(QAction *menuItem);
    void changeTrayIcon(const QIcon &icon);

private slots:
    void on_buttonBox_clicked(QAbstractButton* button);
    void on_outputCheckBox_toggled(bool checked);
    void closeAction(Action *act);
    void onFinnished(int);

public slots:
    void runCommand();
    void accept() {}
};

#endif // ACTIONDIALOG_H
