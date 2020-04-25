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

#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include "common/mimetypes.h"

#include <QObject>
#include <QPersistentModelIndex>
#include <QStringList>
#include <QTimer>
#include <QVector>

class QAbstractItemModel;
class QDir;

struct Ext;
struct BaseNameExtensions;

#define COPYQ_MIME_PREFIX_ITEMSYNC COPYQ_MIME_PREFIX "itemsync-"
extern const char mimeExtensionMap[];
extern const char mimeBaseName[];
extern const char mimeNoSave[];
extern const char mimeSyncPath[];
extern const char mimeNoFormat[];
extern const char mimeUnknownFormats[];

struct FileFormat {
    bool isValid() const { return !extensions.isEmpty(); }
    QStringList extensions;
    QString itemMime;
    QString icon;
};

using BaseNameExtensionsList = QList<BaseNameExtensions>;

using Hash = QByteArray;

class FileWatcher final : public QObject {
public:
    static QString getBaseName(const QModelIndex &index);

    static QString getBaseName(const QVariantMap &data);

    /**
     * Return true only if base name is empty or it matches the internal format.
     */
    static bool isOwnBaseName(const QString &baseName);

    static void removeFilesForRemovedIndex(const QString &tabPath, const QModelIndex &index);

    static Hash calculateHash(const QByteArray &bytes);

    FileWatcher(const QString &path, const QStringList &paths, QAbstractItemModel *model,
                int maxItems, const QList<FileFormat> &formatSettings, QObject *parent);

    const QString &path() const { return m_path; }

    bool isValid() const { return m_valid; }

    bool lock();

    void unlock();

    void createItemFromFiles(const QDir &dir, const BaseNameExtensions &baseNameWithExts, int targetRow);

    void createItemsFromFiles(const QDir &dir, const BaseNameExtensionsList &fileList);

    /**
     * Check for new files.
     */
    void updateItems();

    void updateItemsIfNeeded();

    void setUpdatesEnabled(bool enabled);

private:
    void onRowsInserted(const QModelIndex &, int first, int last);

    void onDataChanged(const QModelIndex &a, const QModelIndex &b);

    void onRowsRemoved(const QModelIndex &, int first, int last);

    struct IndexData {
        QPersistentModelIndex index;
        QString baseName;
        QMap<QString, Hash> formatHash;

        IndexData() {}
        explicit IndexData(const QModelIndex &index) : index(index) {}
        bool operator==(const QModelIndex &otherIndex) const { return otherIndex == index; }
    };

    using IndexDataList = QVector<IndexData>;

    IndexDataList::iterator findIndexData(const QModelIndex &index);

    IndexData &indexData(const QModelIndex &index);

    void createItem(const QVariantMap &dataMap, int targetRow);

    void updateIndexData(const QModelIndex &index, const QVariantMap &itemData);

    QList<QPersistentModelIndex> indexList(int first, int last);

    void saveItems(int first, int last);

    bool renameMoveCopy(const QDir &dir, const QList<QPersistentModelIndex> &indexList);

    void updateDataAndWatchFile(
            const QDir &dir, const BaseNameExtensions &baseNameWithExts,
            QVariantMap *dataMap, QVariantMap *mimeToExtension);

    bool copyFilesFromUriList(const QByteArray &uriData, int targetRow, const QStringList &baseNames);

    QAbstractItemModel *m_model;
    QTimer m_updateTimer;
    int m_interval = 0;
    const QList<FileFormat> &m_formatSettings;
    QString m_path;
    bool m_valid;
    IndexDataList m_indexData;
    int m_maxItems;
    bool m_updatesEnabled = false;
    qint64 m_lastUpdateTimeMs = 0;

    IndexDataList m_batchIndexData;
    BaseNameExtensionsList m_fileList;
    int m_lastBatchIndex = -1;
};

#endif // FILEWATCHER_H
