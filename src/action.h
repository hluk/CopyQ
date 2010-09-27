#ifndef ACTION_H
#define ACTION_H

#include <QProcess>

class QStringList;
class QAction;

class Action : public QProcess
{
    Q_OBJECT
public:
    Action(const QString &cmd, const QByteArray &processInput,
           bool outputItems, const QString &itemSeparator);
    const QString &errorOutput() const { return m_errstr; }
    const QString &command() const { return m_cmd; }
    QAction *menuItem() { return m_menuItem; }
    void start() { QProcess::start(m_cmd, QIODevice::ReadWrite); }

private:
    const QByteArray m_input;
    const QRegExp m_sep;
    const QString m_cmd;
    QAction *m_menuItem;
    QString m_errstr;

    // ID used in menu item
    int m_id;

private slots:
    void actionError(QProcess::ProcessError error);
    void actionStarted();
    void actionFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void actionOutput();
    void actionErrorOutput();

public slots:
    void terminate();

signals:
    void actionError(const QString &msg);
    void actionFinished(Action *act);
    void actionStarted(Action *act);
    void newItems(const QStringList &items);
    void addMenuItem(QAction *menuItem);
    void removeMenuItem(QAction *menuItem);
};

#endif // ACTION_H
