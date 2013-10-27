/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include <QModelIndex>
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
    typedef QList<QStringList> Commands;

    /** Create action with command line parameters. */
    Action(const Commands &cmd, //!< Program to run.
            const QByteArray &input, //!< Standard input contents.
            const QStringList &inputFormats,
            const QString &outputItemFormat, //!< If not empty, signal newItems() will be emitted.
            const QString &itemSeparator,
            //!< Separator for items on standard output.
            const QString &outputTabName, //!< Tab name for output items.
            const QModelIndex &index //!< Output item index.
            );

    /** Return true only if command execution failed. */
    bool actionFailed() const { return m_failed; }

    /** Return standard error output string. */
    const QString &errorOutput() const { return m_errstr; }

    /** Return command line. */
    QString command() const;

    /** Return command arguments. */
    const Commands &commandArguments() const { return m_cmds; }

    /** Return input. */
    const QByteArray &input() const { return m_input; }

    /** Return input formats. */
    const QStringList &inputFormats() const { return m_inputFormats; }

    /** Return output format. */
    QString outputFormat() const;

    /** Return destination index. */
    QModelIndex index() const { return m_index; }

    /** Execute command. */
    void start();

private:
    const QByteArray m_input;
    const QRegExp m_sep;
    const Commands m_cmds;
    const QString m_tab;
    const QStringList m_inputFormats;
    const QString m_outputFormat;
    const QPersistentModelIndex m_index;
    QString m_errstr;
    QString m_lastOutput;
    QByteArray m_outputData;
    bool m_failed;
    QProcess *m_firstProcess; //!< First process in pipe.

private slots:
    void actionError(QProcess::ProcessError error);
    void actionStarted();
    void actionFinished();
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
    void newItems(const QStringList, const QString &outputTabName);
    void newItems(const QStringList, const QModelIndex &index);
    /** Emitted after all standard output is read. */
    void newItem(const QByteArray &data, const QString &format,
                 const QString &outputTabName);
    void newItem(const QByteArray &data, const QString &format,
                 const QModelIndex &index);
};

#endif // ACTION_H
