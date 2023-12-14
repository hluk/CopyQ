// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACTION_H
#define ACTION_H

#include <QStringList>
#include <QVariantMap>

class QAction;
class QProcess;

/**
 * Terminate process or kill if it takes too long.
 */
void terminateProcess(QProcess *p);

/**
 * Execute external program and emits signals
 * to create or change items from the program's stdout.
 */
class Action final : public QObject
{
    Q_OBJECT
public:
    /** Create action with command line parameters. */
    explicit Action(QObject *parent = nullptr);

    ~Action();

    /** Return true only if command execution failed. */
    bool actionFailed() const { return m_failed; }

    /** Return standard error output string. */
    const QByteArray &errorOutput() const { return m_errorOutput; }

    /** Return command line. */
    QString commandLine() const;

    /// Set command and texts to be placed as %2..%9 in command.
    void setCommand(const QString &commandLine, const QStringList &arguments = QStringList());

    /// Set command with arguments.
    void setCommand(const QStringList &arguments);

    /// Return programs and arguments.
    const QList<QList<QStringList>> &command() const { return m_cmds; }

    /** Return input. */
    const QByteArray &input() const { return m_input; }
    void setInput(const QByteArray &input) { m_input = input; }

    /** Set data for input and input format. */
    void setInputWithFormat(const QVariantMap &data, const QString &inputFormat);

    /** Return input formats. */
    const QStringList &inputFormats() const { return m_inputFormats; }

    /** Set working directory path (default is empty so it doesn't change working directory). */
    void setWorkingDirectory(const QString &path) { m_workingDirectoryPath = path; }

    /** Execute command. */
    void start();

    bool waitForFinished(int msecs = -1);

    bool isRunning() const;

    /** Set human-readable name for action. */
    void setName(const QString &actionName) { m_name = actionName; }

    /** Return human-readable name for action. */
    QString name() const { return m_name; }

    void setExitCode(int exitCode) { m_exitCode = exitCode; }
    int exitCode() const { return m_exitCode; }
    QString errorString() const { return m_errorString; }

    void setData(const QVariantMap &data);
    const QVariantMap &data() const;

    void setId(int actionId) { m_id = actionId; }
    int id() const { return m_id; }

    void setReadOutput(bool read) { m_readOutput = read; }

    void appendOutput(const QByteArray &output);
    void appendErrorOutput(const QByteArray &errorOutput);

    /** Terminate (kill) process. */
    void terminate();

signals:
    /** Emitted when finished. */
    void actionFinished(Action *act);
    /** Emitter when started. */
    void actionStarted(Action *act);

    void actionOutput(const QByteArray &output);

private:
    void onSubProcessError(int error);
    void onSubProcessStarted();
    void onSubProcessFinished();
    void onSubProcessOutput();
    void onSubProcessErrorOutput();
    void writeInput();
    void onBytesWritten();

    void closeSubCommands();
    void finish();

    QByteArray m_input;
    QList< QList<QStringList> > m_cmds;
    QStringList m_inputFormats;
    QString m_workingDirectoryPath;
    QByteArray m_errorOutput;
    bool m_failed;
    bool m_readOutput = false;
    int m_currentLine;
    QString m_name;
    QVariantMap m_data;
    QList<QProcess*> m_processes;

    int m_exitCode;
    QString m_errorString;

    int m_id = -1;
};

#endif // ACTION_H
