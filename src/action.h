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

class QStringList;
class QAction;
class QIcon;

class Action : public QProcess
{
    Q_OBJECT
public:
    Action(const QString &cmd, const QStringList &args,
           const QByteArray &processInput,
           bool outputItems, const QString &itemSeparator,
           const QString &outputTabName);
    const QString &errorOutput() const { return m_errstr; }
    const QString &command() const { return m_cmd; }
    QAction *menuItem() { return m_menuItem; }
    void start() { QProcess::start(m_cmd, m_args, QIODevice::ReadWrite); }

private:
    const QByteArray m_input;
    const QRegExp m_sep;
    const QString m_cmd;
    const QStringList m_args;
    const QString m_tab;
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
    void actionError(Action *act);
    void actionFinished(Action *act);
    void actionStarted(Action *act);
    void newItems(const QStringList &items, const QString &outputTabName);
    void addMenuItem(QAction *menuItem);
    void removeMenuItem(QAction *menuItem);
};

#endif // ACTION_H
