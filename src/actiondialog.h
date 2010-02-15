#ifndef ACTIONDIALOG_H
#define ACTIONDIALOG_H

#include <QDialog>

class QCompleter;

namespace Ui {
    class ActionDialog;
}

class ActionDialog : public QDialog {
    Q_OBJECT
public:
    ActionDialog(QWidget *parent = 0);
    ~ActionDialog();
    void setInput(QString input);
    void restoreHistory();
    void saveHistory();
    void add(QString command);
    const QString dataFilename() const;

protected:
    void changeEvent(QEvent *e);

private:
    Ui::ActionDialog *ui;
    int m_maxitems;
    QCompleter *m_completer;
    QStringList m_history;

signals:
    void addItems(const QStringList);
    void error(const QString);

public slots:
    void accept();
};

#endif // ACTIONDIALOG_H
