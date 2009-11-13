/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

#ifndef QEDITOR_H
#define QEDITOR_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QBasicTimer>
#include <QDateTime>

class QEditor : public QObject
{
    Q_OBJECT

    public:
        QEditor(const QString &txt, const QString &editor, QObject *parent = 0);
        ~QEditor();
        bool start();
        bool fileModified();
        uint getHash() const { return m_hash; };
        const QString &getText() const { return m_txt; };

    private:
        QString m_txt;
        uint m_hash;
        QTemporaryFile m_tmpfile;
        QString m_editorcmd;
        QProcess *m_editor;
        QBasicTimer timer;
        QFileInfo m_info;
        QDateTime m_lastmodified;

    protected:
        void timerEvent(QTimerEvent *event);

    public slots:
        void close() { emit closed(this); };

    signals:
        void fileModified(uint oldhash, const QString &str);
        void closed(QEditor *who);
};

#endif // QEDITOR_H
