#ifndef ACTION_H
#define ACTION_H

#include <QProcess>

class QStringList;

class Action : public QProcess
{
    Q_OBJECT
public:
    Action(const QString &cmd, const QByteArray &processInput,
           bool outputItems, const QString &itemSeparator);
    const QString &getError() const { return m_errstr; };

private:
    const QByteArray m_input;
    const QRegExp m_sep;
    QString m_errstr;

private slots:
    void actionError(QProcess::ProcessError error);
    void actionStarted();
    void actionFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void actionOutput();
    void actionErrorOutput();

signals:
    void actionError(const QString &msg);
    void actionFinished(Action *act);
    void newItems(const QStringList &items);
};

#endif // ACTION_H
