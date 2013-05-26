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

#include <QDateTime>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QTemporaryFile>

class QProcess;
class QTimer;

class ItemEditor : public QObject
{
    Q_OBJECT

    public:
        ItemEditor(const QByteArray &data, const QString &mime, const QString &editor,
                   QObject *parent = NULL);
        ~ItemEditor();

    signals:
        /**
         * File was modified.
         * @param data  modified data
         * @param mime  MIME type of the data
         */
        void fileModified(const QByteArray &data, const QString &mime);

        /**
         * Editor was closed.
         * @param who  pointer to this object
         */
        void closed(QObject *who);

    public slots:
        /**
         * Execute editor process.
         * @retval true   Editor successfully opened.
         * @retval false  An error occured (failed to create temporary file), editor was not opened.
         */
        bool start();

    private slots:
        /**
         * Close editor.
         */
        void close();

        void onTimer();

    private:
        /** Return true only if file was modified and reset this status. */
        bool fileModified();

        QByteArray m_data;
        QString m_mime;
        // hash of original string (saves some memory)
        uint m_hash;

        QString m_editorcmd;
        QProcess *m_editor;
        QTimer *m_timer;

        QTemporaryFile m_tmpfile;
        QFileInfo m_info;
        QDateTime m_lastmodified;
        bool m_modified;
};

#endif // ITEMEDITOR_H
