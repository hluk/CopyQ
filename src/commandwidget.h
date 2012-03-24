#ifndef COMMANDWIDGET_H
#define COMMANDWIDGET_H

#include <QWidget>
#include <QIcon>

struct Command {
    QString name;
    QRegExp re;
    QString cmd;
    QString sep;
    bool input;
    bool output;
    bool wait;
    bool automatic;
    bool ignore;
    bool enable;
    QString icon;
    QString shortcut;
    QString tab;
};

namespace Ui {
class CommandWidget;
}

class CommandWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommandWidget(QWidget *parent = 0);
    ~CommandWidget();

    Command command() const;
    void setCommand(const Command &command) const;

    void setTabs(const QStringList &tabs);

private slots:
    void on_pushButtonBrowse_clicked();

    void on_lineEditIcon_textChanged(const QString &arg1);

    void on_pushButtonShortcut_clicked();

    void on_lineEditCommand_textChanged(const QString &arg1);

private:
    Ui::CommandWidget *ui;
};

#endif // COMMANDWIDGET_H
