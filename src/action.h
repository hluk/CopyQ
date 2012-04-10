/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ACTION_H
#define ACTION_H

#include <QProcess>
#include <QStringList>

class QAction;

/**
 * Execute external program.
 */
class Action : public QProcess
{
    Q_OBJECT
public:
    /** Create action with command line parameters. */
    Action(
            const QString &cmd, //!< Program to run.
            const QStringList &args, //!< Program parameters.
            const QByteArray &input, //!< Standard input contents.
            bool outputItems, //!< If true signals newItems() will be emitted.
            const QString &itemSeparator,
            //!< Separator for items on standard output.
            const QString &outputTabName //!< Tab name for output items.
            );

    /** Return standard error output string. */
    const QString &errorOutput() const { return m_errstr; }

    /** Return command. */
    const QString &command() const { return m_cmd; }

    /** Return command arguments. */
    const QStringList &commandArguments() const { return m_args; }

    /** Return input. */
    const QByteArray &input() const { return m_input; }

    /** Execute command. */
    void start() { QProcess::start(m_cmd, m_args, QIODevice::ReadWrite); }

private:
    const QByteArray m_input;
    const QRegExp m_sep;
    const QString m_cmd;
    const QStringList m_args;
    const QString m_tab;
    QString m_errstr;
    QString m_lastOutput;

private slots:
    void actionError(QProcess::ProcessError error);
    void actionStarted();
    void actionFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void actionOutput();
    void actionErrorOutput();

public slots:
    /** Terminate (kill) process. */
    void terminate();

signals:
    /** Emitted on error. */
    void actionError(Action *act);
    /** Emitted when finished. */
    void actionFinished(Action *act);
    /** Emitter when started. */
    void actionStarted(Action *act);
    /** Emitted if standard output has some items available. */
    void newItems(const QStringList &items, const QString &outputTabName);
};

#endif // ACTION_H
