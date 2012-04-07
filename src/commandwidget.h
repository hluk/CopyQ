#ifndef COMMANDWIDGET_H
#define COMMANDWIDGET_H

#include <QWidget>
#include <QIcon>

struct Command {
    Command()
        : name()
        , re()
        , cmd()
        , sep()
        , input(false)
        , output(false)
        , wait(false)
        , automatic(false)
        , ignore(false)
        , enable(true)
        , icon()
        , shortcut()
        , tab()
        , outputTab()
        {}

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
    QString outputTab;
};

namespace Ui {
class CommandWidget;
}

class QComboBox;

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

    void on_checkBoxOutput_toggled(bool checked);

private:
    Ui::CommandWidget *ui;

    void setTabs(const QStringList &tabs, QComboBox *w);
};

#endif // COMMANDWIDGET_H
