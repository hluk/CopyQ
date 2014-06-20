/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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
#include <QVariantMap>

class QAction;

/**
 * Execute external program.
 */
class Action : public QProcess
{
    Q_OBJECT
public:
    /** Create action with command line parameters. */
    Action();

    /** Return true only if command execution failed. */
    bool actionFailed() const { return m_failed; }

    /** Return standard error output string. */
    const QString &errorOutput() const { return m_errstr; }

    /** Return command line. */
    QString command() const;

    /// Set command and texts to be placed as %2..%9 in command.
    void setCommand(const QString &command, const QStringList &arguments = QStringList());

    /// Set command with arguments.
    void setCommand(const QStringList &arguments);

    /** Return input. */
    const QByteArray &input() const { return m_input; }
    void setInput(const QByteArray &input) { m_input = input; }

    /** Return input formats. */
    const QStringList &inputFormats() const { return m_inputFormats; }
    void setInputFormats(const QStringList &inputFormats) { m_inputFormats = inputFormats; }

    /** Return output format. */
    QString outputFormat() const { return m_outputFormat; }
    void setOutputFormat(const QString &outputItemFormat) { m_outputFormat = outputItemFormat; }

    /// Set separator for items on standard output.
    void setItemSeparator(const QRegExp &itemSeparator) { m_sep = itemSeparator; }

    /// Return tab name for output items.
    QString outputTab() const { return m_tab; }
    void setOutputTab(const QString &outputTabName) { m_tab = outputTabName; }

    /** Return destination index. */
    QModelIndex index() const { return m_index; }
    void setIndex(const QModelIndex &index) { m_index = index; }

    /** Execute command. */
    bool start();

    /** Set human-readable name for action. */
    void setName(const QString &actionName) { m_name = actionName; }

    /** Return human-readable name for action. */
    QString name() const { return m_name; }

    QByteArray outputData() const { return m_outputData; }

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

private slots:
    void actionError(QProcess::ProcessError error);
    void actionStarted();
    void actionFinished();
    void actionOutput();
    void actionErrorOutput();

private:
    bool hasTextOutput() const;
    bool canEmitNewItems() const;

    QByteArray m_input;
    QRegExp m_sep;
    QList< QList<QStringList> > m_cmds;
    QString m_tab;
    QStringList m_inputFormats;
    QString m_outputFormat;
    QPersistentModelIndex m_index;
    QString m_errstr;
    QString m_lastOutput;
    QByteArray m_outputData;
    bool m_failed;
    QProcess *m_firstProcess; //!< First process in pipe.
    int m_currentLine;
    QString m_name;
    QStringList m_items;
};

#endif // ACTION_H
