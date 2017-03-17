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

#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include "common/mimetypes.h"

#include <QObject>
#include <QPointer>
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

class FileWatcher : public QObject {
    Q_OBJECT

public:
    static QString getBaseName(const QModelIndex &index);

    static void removeFormatFiles(const QString &path, const QVariantMap &mimeToExtension);

    static Hash calculateHash(const QByteArray &bytes);

    FileWatcher(const QString &path, const QStringList &paths, QAbstractItemModel *model,
                int maxItems, const QList<FileFormat> &formatSettings, QObject *parent);

    const QString &path() const { return m_path; }

    bool isValid() const { return m_valid; }

    QAbstractItemModel *model() const { return m_model; }

public slots:
    bool lock();

    void unlock();

    bool createItemFromFiles(const QDir &dir, const BaseNameExtensions &baseNameWithExts, int targetRow);

    void createItemsFromFiles(const QDir &dir, const BaseNameExtensionsList &fileList);

    /**
     * Check for new files.
     */
    void updateItems();

private slots:
    void onRowsInserted(const QModelIndex &, int first, int last);

    void onDataChanged(const QModelIndex &a, const QModelIndex &b);

    void onRowsRemoved(const QModelIndex &, int first, int last);

private:
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

    bool createItem(const QVariantMap &dataMap, int targetRow);

    void updateIndexData(const QModelIndex &index, const QVariantMap &itemData);

    QList<QModelIndex> indexList(int first, int last);

    void saveItems(int first, int last);

    bool renameToUnique(const QDir &dir, const QStringList &baseNames, QString *name);

    bool renameMoveCopy(const QDir &dir, const QList<QModelIndex> &indexList);

    void updateDataAndWatchFile(
            const QDir &dir, const BaseNameExtensions &baseNameWithExts,
            QVariantMap *dataMap, QVariantMap *mimeToExtension);

    bool copyFilesFromUriList(const QByteArray &uriData, int targetRow, const QStringList &baseNames);

    QPointer<QAbstractItemModel> m_model;
    QTimer m_updateTimer;
    const QList<FileFormat> &m_formatSettings;
    QString m_path;
    bool m_valid;
    IndexDataList m_indexData;
    int m_maxItems;
};

#endif // FILEWATCHER_H
