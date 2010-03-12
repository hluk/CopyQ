#ifndef ACTIONDIALOG_H
#define ACTIONDIALOG_H

#include <QDialog>

class QCompleter;
class Action;

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

private:
    Ui::ActionDialog *ui;
    int m_maxitems;
    QCompleter *m_completer;
    QStringList m_history;

signals:
    void addItems(const QStringList &lst);
    void error(const QString &msg);
    void message(const QString &title, const QString &msg);
    void actionFinished(Action *act);
    void addMenuItem(QAction *menuItem);
    void removeMenuItem(QAction *menuItem);

private slots:
    void closeAction(Action *act);

public slots:
    void accept();
};

#endif // ACTIONDIALOG_H
