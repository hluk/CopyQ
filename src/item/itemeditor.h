/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include <QPersistentModelIndex>
#include <QString>

class QModelIndex;
class QProcess;
class QTimer;

class ItemEditor final : public QObject
{
    Q_OBJECT

    public:
        ItemEditor(const QByteArray &data, const QString &mime, const QString &editor,
                   QObject *parent = nullptr);
        ~ItemEditor();

        /**
         * Set index to edited item (will be emitted with fileModified()).
         */
        void setIndex(const QModelIndex &index);

        /**
         * Execute editor process.
         * @retval true   Editor successfully opened.
         * @retval false  An error occurred (failed to create temporary file), editor was not opened.
         */
        Q_SLOT bool start();

    signals:
        /**
         * File was modified.
         * @param data  modified data
         * @param mime  MIME type of the data
         * @param index  index of edited item or invalid
         */
        void fileModified(const QByteArray &data, const QString &mime, const QModelIndex &index);

        /**
         * Editor was closed.
         * @param who  pointer to this object
         */
        void closed(QObject *who, const QModelIndex &index);

        /**
         * Failed to run editor command.
         */
        void error(const QString &errorString);

    private:
        /**
         * Close editor.
         */
        void close();

        void onError();

        void onTimer();

        /** Return true only if file was modified and reset this status. */
        bool wasFileModified();

        void emitError(const QString &errorString);

        QByteArray m_data;
        QString m_mime;
        // hash of original string (saves some memory)
        uint m_hash;

        QString m_editorcmd;
        QProcess *m_editor;
        QTimer *m_timer;

        QFileInfo m_info;
        QDateTime m_lastmodified;
        qint64 m_lastSize;
        bool m_modified;

        QPersistentModelIndex m_index;
};

#endif // ITEMEDITOR_H
