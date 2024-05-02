// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include "common/mimetypes.h"

#include <QLockFile>
#include <QObject>
#include <QPersistentModelIndex>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVector>

class QAbstractItemModel;
class QDir;

struct Ext;
struct BaseNameExtensions;

#define COPYQ_MIME_PREFIX_ITEMSYNC COPYQ_MIME_PREFIX "itemsync-"
#define COPYQ_MIME_PREFIX_ITEMSYNC_PRIVATE COPYQ_MIME_PRIVATE_PREFIX "itemsync-"
extern const QLatin1String mimeExtensionMap;
extern const QLatin1String mimeBaseName;
extern const QLatin1String mimeNoSave;
extern const QLatin1String mimeSyncPath;
extern const QLatin1String mimeNoFormat;
extern const QLatin1String mimeUnknownFormats;

extern const QLatin1String mimePrivateSyncPrefix;
extern const QLatin1String mimeOldBaseName;
extern const QLatin1String mimeHashPrefix;

enum class UpdateType {
    Inserted,
    Changed,
};

struct FileFormat {
    bool isValid() const { return !extensions.isEmpty(); }
    QStringList extensions;
    QString itemMime;
    QString icon;
};

using BaseNameExtensionsList = QList<BaseNameExtensions>;

using Hash = QByteArray;

class SyncDataFile;
QDataStream &operator<<(QDataStream &out, SyncDataFile value);
QDataStream &operator>>(QDataStream &in, SyncDataFile &value);
void registerSyncDataFileConverter();

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

    FileWatcher(
        const QString &path,
        const QStringList &paths,
        QAbstractItemModel *model,
        int maxItems,
        const QList<FileFormat> &formatSettings,
        int itemDataThreshold,
        QObject *parent = nullptr
    );

    const QString &path() const { return m_path; }

    bool isValid() const { return m_valid; }

    bool lock();

    void unlock();

    QVariantMap itemDataFromFiles(const QDir &dir, const BaseNameExtensions &baseNameWithExts);

    void prependItemsFromFiles(const QDir &dir, const BaseNameExtensionsList &fileList);
    void insertItemsFromFiles(const QDir &dir, const BaseNameExtensionsList &fileList);

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

    void onRowsMoved(const QModelIndex &, int start, int end, const QModelIndex &, int destinationRow);

    QString oldBaseName(const QModelIndex &index) const;

    void createItems(const QVector<QVariantMap> &dataMaps, int targetRow);

    void updateIndexData(const QModelIndex &index, QVariantMap *itemData);

    QList<QPersistentModelIndex> indexList(int first, int last);

    void saveItems(int first, int last, UpdateType updateType);

    bool renameMoveCopy(
        const QDir &dir, const QList<QPersistentModelIndex> &indexList, UpdateType updateType);

    void updateDataAndWatchFile(
            const QDir &dir, const BaseNameExtensions &baseNameWithExts,
            QVariantMap *dataMap, QVariantMap *mimeToExtension);

    bool copyFilesFromUriList(const QByteArray &uriData, int targetRow, const QSet<QString> &baseNames);

    void updateMovedRows();

    QAbstractItemModel *m_model;
    QTimer m_updateTimer;
    QTimer m_moveTimer;
    int m_moveEnd = -1;
    int m_interval = 0;
    const QList<FileFormat> &m_formatSettings;
    QString m_path;
    bool m_valid;
    int m_maxItems;
    bool m_updatesEnabled = false;
    qint64 m_lastUpdateTimeMs = 0;

    QList<QPersistentModelIndex> m_batchIndexData;
    BaseNameExtensionsList m_fileList;
    int m_lastBatchIndex = -1;
    int m_itemDataThreshold = -1;

    QLockFile m_lock;
};

#endif // FILEWATCHER_H
