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

#ifndef ITEMEDITOR_H
#define ITEMEDITOR_H

#include <QObject>
#include <QString>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDateTime>

class QProcess;
class QTimer;

class ItemEditor : public QObject
{
    Q_OBJECT

    public:
        ItemEditor(const QString &txt, const QString &editor, QObject *parent = 0);
        ~ItemEditor();
        bool start();
        bool fileModified();
        const QString &getText() const { return m_txt; };

    private:
        QString m_txt;
        // hash of original string (saves some memory)
        uint m_hash;

        QString m_editorcmd;
        QProcess *m_editor;
        QTimer *m_timer;

        QTemporaryFile m_tmpfile;
        QFileInfo m_info;
        QDateTime m_lastmodified;

    public slots:
        void close() { emit closed(this); };

    private slots:
        void onTimer();

    signals:
        void fileModified(const QString &str);
        void closed(ItemEditor *who);
};

#endif // ITEMEDITOR_H
