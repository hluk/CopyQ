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

#include "filewatcher.h"

#include "common/contenttype.h"
#include "common/log.h"
#include "common/regexp.h"
#include "item/serialize.h"

#include <QAbstractItemModel>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QMimeData>
#include <QRegularExpression>
#include <QUrl>

const char mimeExtensionMap[] = COPYQ_MIME_PREFIX_ITEMSYNC "mime-to-extension-map";
const char mimeBaseName[] = COPYQ_MIME_PREFIX_ITEMSYNC "basename";
const char mimeNoSave[] = COPYQ_MIME_PREFIX_ITEMSYNC "no-save";
const char mimeSyncPath[] = COPYQ_MIME_PREFIX_ITEMSYNC "sync-path";
const char mimeNoFormat[] = COPYQ_MIME_PREFIX_ITEMSYNC "no-format";
const char mimeUnknownFormats[] = COPYQ_MIME_PREFIX_ITEMSYNC "unknown-formats";

struct Ext {
    Ext() : extension(), format() {}

    Ext(const QString &extension, const QString &format)
        : extension(extension)
        , format(format)
    {}

    QString extension;
    QString format;
};

struct BaseNameExtensions {
    explicit BaseNameExtensions(const QString &baseName = QString(),
                                const QList<Ext> &exts = QList<Ext>())
        : baseName(baseName)
        , exts(exts) {}
    QString baseName;
    QList<Ext> exts;
};

namespace {

const char dataFileSuffix[] = "_copyq.dat";
const char noteFileSuffix[] = "_note.txt";

const int defaultUpdateFocusItemsIntervalMs = 10000;
const int batchItemUpdateIntervalMs = 100;

const qint64 sizeLimit = 10 << 20;

FileFormat getFormatSettingsFromFileName(const QString &fileName,
                                         const QList<FileFormat> &formatSettings,
                                         QString *foundExt = nullptr)
{
    for (const auto &format : formatSettings) {
        for ( const auto &ext : format.extensions ) {
            if ( fileName.endsWith(ext) ) {
                if (foundExt)
                    *foundExt = ext;
                return format;
            }
        }
    }

    return FileFormat();
}

void getBaseNameAndExtension(const QString &fileName, QString *baseName, QString *ext,
                             const QList<FileFormat> &formatSettings)
{
    ext->clear();

    const FileFormat fileFormat = getFormatSettingsFromFileName(fileName, formatSettings, ext);

    if ( !fileFormat.isValid() ) {
        const int i = fileName.lastIndexOf('.');
        if (i != -1)
            *ext = fileName.mid(i);
    }

    *baseName = fileName.left( fileName.size() - ext->size() );

    if ( baseName->endsWith('.') ) {
        baseName->chop(1);
        ext->prepend('.');
    }
}

QList<Ext> fileExtensionsAndFormats()
{
    static QList<Ext> exts;

    if ( exts.isEmpty() ) {
        exts.append( Ext(noteFileSuffix, mimeItemNotes) );

        exts.append( Ext(".bmp", "image/bmp") );
        exts.append( Ext(".gif", "image/gif") );
        exts.append( Ext(".html", mimeHtml) );
        exts.append( Ext("_inkscape.svg", "image/x-inkscape-svg-compressed") );
        exts.append( Ext(".jpg", "image/jpeg") );
        exts.append( Ext(".jpg", "image/jpeg") );
        exts.append( Ext(".png", "image/png") );
        exts.append( Ext(".txt", mimeText) );
        exts.append( Ext(".uri", mimeUriList) );
        exts.append( Ext(".xml", "application/xml") );
        exts.append( Ext(".svg", "image/svg+xml") );
        exts.append( Ext(".xml", "text/xml") );
    }

    return exts;
}

QString findByFormat(const QString &format, const QList<FileFormat> &formatSettings)
{
    // Find in default extensions.
    const QList<Ext> &exts = fileExtensionsAndFormats();

    for (const auto &ext : exts) {
        if (ext.format == format)
            return ext.extension;
    }

    // Find in user defined extensions.
    for (const auto &fileFormat : formatSettings) {
        if ( !fileFormat.extensions.isEmpty() && fileFormat.itemMime != "-"
             && format == fileFormat.itemMime )
        {
            return fileFormat.extensions.first();
        }
    }

    return QString();
}

Ext findByExtension(const QString &fileName, const QList<FileFormat> &formatSettings)
{
    // Is internal data format?
    if ( fileName.endsWith(dataFileSuffix) )
        return Ext(dataFileSuffix, mimeUnknownFormats);

    // Find in user defined formats.
    bool hasUserFormat = false;
    for (const auto &format : formatSettings) {
        for (const auto &ext : format.extensions) {
            if ( fileName.endsWith(ext) ) {
                if ( format.itemMime.isEmpty() )
                    hasUserFormat = true;
                else
                    return Ext(ext, format.itemMime);
            }
        }
    }

    // Find in default formats.
    const QList<Ext> &exts = fileExtensionsAndFormats();

    for (const auto &ext : exts) {
        if ( fileName.endsWith(ext.extension) )
            return ext;
    }

    return hasUserFormat ? Ext(QString(), mimeNoFormat) : Ext();
}

bool saveItemFile(const QString &filePath, const QByteArray &bytes,
                  QStringList *existingFiles, bool hashChanged = true)
{
    if ( !existingFiles->removeOne(filePath) || hashChanged ) {
        QFile f(filePath);
        if ( !f.open(QIODevice::WriteOnly) || f.write(bytes) == -1 ) {
            log( QString("ItemSync: %1").arg(f.errorString()), LogError );
            return false;
        }
    }

    return true;
}

bool canUseFile(const QFileInfo &info)
{
    return !info.isHidden() && !info.fileName().startsWith('.') && info.isReadable();
}

bool getBaseNameExtension(const QString &filePath, const QList<FileFormat> &formatSettings,
                          QString *baseName, Ext *ext)
{
    QFileInfo info(filePath);
    if ( !canUseFile(info) )
        return false;

    *ext = findByExtension(filePath, formatSettings);
    if ( ext->format.isEmpty() || ext->format == "-" )
        return false;

    const QString fileName = info.fileName();
    *baseName = fileName.left( fileName.size() - ext->extension.size() );

    return true;
}

BaseNameExtensionsList listFiles(const QStringList &files,
                                 const QList<FileFormat> &formatSettings)
{
    BaseNameExtensionsList fileList;
    QMap<QString, int> fileMap;

    for (const auto &filePath : files) {
        QString baseName;
        Ext ext;
        if ( getBaseNameExtension(filePath, formatSettings, &baseName, &ext) ) {
            int i = fileMap.value(baseName, -1);
            if (i == -1) {
                i = fileList.size();
                fileList.append( BaseNameExtensions(baseName) );
                fileMap.insert(baseName, i);
            }

            fileList[i].exts.append(ext);
        }
    }

    return fileList;
}

/// Load hash of all existing files to map (hash -> filename).
QStringList listFiles(const QDir &dir)
{
    QStringList files;

    const QDir::Filters itemFileFilter = QDir::Files | QDir::Readable | QDir::Writable;
    // Get files ordered by name to achive same result on multiple platforms.
    // NOTE: Sorting by modification time can be slow.
    for ( const auto &info : dir.entryInfoList(itemFileFilter, QDir::Name) ) {
        if ( canUseFile(info) )
            files.append(info.absoluteFilePath());
    }

    return files;
}

/// Return true only if no file name in @a fileNames starts with @a baseName.
bool isUniqueBaseName(const QString &baseName, const QStringList &fileNames,
                      const QStringList &baseNames = QStringList())
{
    if ( baseNames.contains(baseName) )
        return false;

    for (const auto &fileName : fileNames) {
        if ( fileName.startsWith(baseName) )
            return false;
    }

    return true;
}

void moveFormatFiles(const QString &oldPath, const QString &newPath,
                     const QVariantMap &mimeToExtension)
{
    for (const auto &extValue : mimeToExtension) {
        const QString ext = extValue.toString();
        QFile::rename(oldPath + ext, newPath + ext);
    }
}

void copyFormatFiles(const QString &oldPath, const QString &newPath,
                     const QVariantMap &mimeToExtension)
{
    for (const auto &extValue : mimeToExtension) {
        const QString ext = extValue.toString();
        QFile::copy(oldPath + ext, newPath + ext);
    }
}

void removeFormatFiles(const QString &path, const QVariantMap &mimeToExtension)
{
    for (const auto &extValue : mimeToExtension)
        QFile::remove(path + extValue.toString());
}

bool renameToUnique(
        const QDir &dir, const QStringList &baseNames, QString *name,
        const QList<FileFormat> &formatSettings)
{
    if ( name->isEmpty() ) {
        *name = "copyq_0000";
    } else {
        // Replace/remove unsafe characters.
        name->replace( QRegularExpression("/|\\\\|^\\."), QString("_") );
        name->remove( QRegularExpression("\\n|\\r") );
    }

    const QStringList fileNames = dir.entryList();

    if ( isUniqueBaseName(*name, fileNames, baseNames) )
        return true;

    QString ext;
    QString baseName;
    getBaseNameAndExtension(*name, &baseName, &ext, formatSettings);

    int i = 0;
    int fieldWidth = 0;

    QRegularExpression re("\\d+$");
    const auto m = re.match(baseName);
    if (m.hasMatch()) {
        const QString num = m.captured();
        i = num.toInt();
        fieldWidth = num.size();
        baseName = baseName.mid( 0, baseName.size() - fieldWidth );
    } else {
        baseName.append('-');
    }

    QString newName;
    do {
        if (i >= 99999)
            return false;
        newName = baseName + QString("%1").arg(++i, fieldWidth, 10, QChar('0')) + ext;
    } while ( !isUniqueBaseName(newName, fileNames, baseNames) );

    *name = newName;

    return true;
}

} // namespace

QString FileWatcher::getBaseName(const QModelIndex &index)
{
    return getBaseName( index.data(contentType::data).toMap() );
}

QString FileWatcher::getBaseName(const QVariantMap &data)
{
    return data.value(mimeBaseName).toString();
}

bool FileWatcher::isOwnBaseName(const QString &baseName)
{
    static const auto re = anchoredRegExp("copyq_\\d*");
    return baseName.contains(re);
}

void FileWatcher::removeFilesForRemovedIndex(const QString &tabPath, const QModelIndex &index)
{
    const QAbstractItemModel *model = index.model();
    if (!model)
        return;

    const QString baseName = FileWatcher::getBaseName(index);
    if ( baseName.isEmpty() )
        return;

    // Check if item is still present in list (drag'n'drop).
    bool remove = true;
    for (int i = 0; i < model->rowCount(); ++i) {
        const QModelIndex index2 = model->index(i, 0);
        if ( index2 != index && baseName == FileWatcher::getBaseName(index2) ) {
            remove = false;
            break;
        }
    }
    if (!remove)
        return;

    const QVariantMap itemData = index.data(contentType::data).toMap();
    const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
    if ( mimeToExtension.isEmpty() )
        QFile::remove(tabPath + '/' + baseName);
    else
        removeFormatFiles(tabPath + '/' + baseName, mimeToExtension);
}

Hash FileWatcher::calculateHash(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);
}

FileWatcher::FileWatcher(
        const QString &path,
        const QStringList &paths,
        QAbstractItemModel *model,
        int maxItems,
        const QList<FileFormat> &formatSettings,
        QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_formatSettings(formatSettings)
    , m_path(path)
    , m_valid(true)
    , m_indexData()
    , m_maxItems(maxItems)
{
    m_updateTimer.setSingleShot(true);

    bool ok;
    const int interval = qgetenv("COPYQ_SYNC_UPDATE_INTERVAL_MS").toInt(&ok);
    m_interval = ok && interval > 0 ? interval : defaultUpdateFocusItemsIntervalMs;

    connect( &m_updateTimer, &QTimer::timeout,
             this, &FileWatcher::updateItems );

    connect( m_model, &QAbstractItemModel::rowsInserted,
             this, &FileWatcher::onRowsInserted );
    connect( m_model, &QAbstractItemModel::rowsAboutToBeRemoved,
             this, &FileWatcher::onRowsRemoved );
    connect( m_model, &QAbstractItemModel::dataChanged,
             this, &FileWatcher::onDataChanged );

    if (model->rowCount() > 0)
        saveItems(0, model->rowCount() - 1);

    createItemsFromFiles( QDir(path), listFiles(paths, m_formatSettings) );
}

bool FileWatcher::lock()
{
    if ( !m_valid )
        return false;

    m_valid = false;
    return true;
}

void FileWatcher::unlock()
{
    m_valid = true;
}

void FileWatcher::createItemFromFiles(const QDir &dir, const BaseNameExtensions &baseNameWithExts, int targetRow)
{
    QVariantMap dataMap;
    QVariantMap mimeToExtension;

    updateDataAndWatchFile(dir, baseNameWithExts, &dataMap, &mimeToExtension);

    if ( !mimeToExtension.isEmpty() ) {
        dataMap.insert( mimeBaseName, QFileInfo(baseNameWithExts.baseName).fileName() );
        dataMap.insert(mimeExtensionMap, mimeToExtension);
        createItem(dataMap, targetRow);
    }
}

void FileWatcher::createItemsFromFiles(const QDir &dir, const BaseNameExtensionsList &fileList)
{
    for (const auto &baseNameWithExts : fileList) {
        if ( m_model->rowCount() >= m_maxItems )
            break;

        createItemFromFiles(dir, baseNameWithExts, 0);
    }
}

void FileWatcher::updateItems()
{
    if ( !lock() ) {
        m_updateTimer.start(m_interval);
        return;
    }

    QElapsedTimer t;
    t.start();

    m_lastUpdateTimeMs = QDateTime::currentMSecsSinceEpoch();

    const QDir dir(m_path);

    if ( m_batchIndexData.isEmpty() ) {
        const QStringList files = listFiles(dir);
        m_fileList = listFiles(files, m_formatSettings);
        m_batchIndexData = m_indexData;

        // Sort so that top rows get updated first.
        std::sort(
            std::begin(m_batchIndexData), std::end(m_batchIndexData),
            [](const IndexData &lhs, const IndexData &rhs){
                return lhs.index.row() < rhs.index.row();
            });

        m_lastBatchIndex = -1;
        if ( t.elapsed() > 100 )
            log( QString("ItemSync: Files listed in %1 ms").arg(t.elapsed()) );
    }

    for ( int i = m_lastBatchIndex + 1; i < m_batchIndexData.size(); ++i ) {
        const IndexData &indexData = m_batchIndexData[i];
        const QPersistentModelIndex &index = indexData.index;
        if ( !index.isValid() )
            continue;
        const QString baseName = indexData.baseName;
        if ( baseName.isEmpty() )
            continue;

        int j = 0;
        for ( j = 0; j < m_fileList.size() && m_fileList[j].baseName != baseName; ++j ) {}

        QVariantMap dataMap;
        QVariantMap mimeToExtension;

        if ( j < m_fileList.size() ) {
            updateDataAndWatchFile(dir, m_fileList[j], &dataMap, &mimeToExtension);
            m_fileList.removeAt(j);
        }

        if ( mimeToExtension.isEmpty() ) {
            m_model->removeRow(index.row());
        } else {
            dataMap.insert(mimeBaseName, baseName);
            dataMap.insert(mimeExtensionMap, mimeToExtension);
            updateIndexData(index, dataMap);
        }

        if ( t.elapsed() > 20 ) {
            COPYQ_LOG_VERBOSE( QString("ItemSync: Items updated in %1 ms, last row %2/%3")
                 .arg(t.elapsed())
                 .arg(i + 1)
                 .arg(m_batchIndexData.size()) );
            m_lastBatchIndex = i;
            unlock();
            m_updateTimer.start(batchItemUpdateIntervalMs);
            return;
        }
    }

    t.restart();

    createItemsFromFiles(dir, m_fileList);

    if ( t.elapsed() > 100 )
        log( QString("ItemSync: Items created in %1 ms").arg(t.elapsed()) );

    m_fileList.clear();
    m_batchIndexData.clear();

    unlock();

    if (m_updatesEnabled)
        m_updateTimer.start(m_interval);
}

void FileWatcher::updateItemsIfNeeded()
{
    const auto time = QDateTime::currentMSecsSinceEpoch();
    if (time < m_lastUpdateTimeMs + m_interval)
        return;

    updateItems();
}

void FileWatcher::setUpdatesEnabled(bool enabled)
{
    m_updatesEnabled = enabled;
    if (enabled)
        updateItems();
    else if ( m_batchIndexData.isEmpty() )
        m_updateTimer.stop();
}

void FileWatcher::onRowsInserted(const QModelIndex &, int first, int last)
{
    saveItems(first, last);
}

void FileWatcher::onDataChanged(const QModelIndex &a, const QModelIndex &b)
{
    saveItems(a.row(), b.row());
}

void FileWatcher::onRowsRemoved(const QModelIndex &, int first, int last)
{
    for ( const auto &index : indexList(first, last) ) {
        if ( !index.isValid() )
            continue;

        IndexDataList::iterator it = findIndexData(index);
        if ( it == m_indexData.end() )
            continue;

        if ( isOwnBaseName(it->baseName) )
            removeFilesForRemovedIndex(m_path, index);
        m_indexData.erase(it);
    }
}

FileWatcher::IndexDataList::iterator FileWatcher::findIndexData(const QModelIndex &index)
{
    return std::find(m_indexData.begin(), m_indexData.end(), index);
}

FileWatcher::IndexData &FileWatcher::indexData(const QModelIndex &index)
{
    IndexDataList::iterator it = findIndexData(index);
    if ( it == m_indexData.end() )
        return *m_indexData.insert( m_indexData.end(), IndexData(index) );
    return *it;
}

void FileWatcher::createItem(const QVariantMap &dataMap, int targetRow)
{
    const int row = qMax( 0, qMin(targetRow, m_model->rowCount()) );
    if ( !m_model->insertRow(row) )
        return;

    // Find index if it was moved after inserted.
    const int rows = m_model->rowCount();
    for ( int i = 0; i < rows; ++i ) {
        const int row2 = (row + i) % rows;
        auto index = m_model->index(row2, 0);
        if ( getBaseName(index).isEmpty() ) {
            updateIndexData(index, dataMap);
            return;
        }
    }
}

void FileWatcher::updateIndexData(const QModelIndex &index, const QVariantMap &itemData)
{
    m_model->setData(index, itemData, contentType::data);

    // Item base name is non-empty.
    const QString baseName = getBaseName(index);
    if ( baseName.isEmpty() )
        return;

    const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();

    IndexData &data = indexData(index);

    data.baseName = baseName;

    QMap<QString, Hash> &formatData = data.formatHash;
    formatData.clear();

    for ( const auto &format : mimeToExtension.keys() ) {
        if ( !format.startsWith(COPYQ_MIME_PREFIX_ITEMSYNC) )
            formatData.insert(format, calculateHash(itemData.value(format).toByteArray()) );
    }
}

QList<QPersistentModelIndex> FileWatcher::indexList(int first, int last)
{
    QList<QPersistentModelIndex> indexList;
    indexList.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
        indexList.append( m_model->index(i, 0) );
    return indexList;
}

void FileWatcher::saveItems(int first, int last)
{
    if ( !lock() )
        return;

    const auto indexList = this->indexList(first, last);

    // Create path if doesn't exist.
    QDir dir(m_path);
    if ( !dir.mkpath(".") ) {
        log( tr("Failed to create synchronization directory \"%1\"!").arg(m_path) );
        return;
    }

    if ( !renameMoveCopy(dir, indexList) )
        return;

    QStringList existingFiles = listFiles(dir);

    for (const auto &index : indexList) {
        if ( !index.isValid() )
            continue;

        const QString baseName = getBaseName(index);
        const QString filePath = dir.absoluteFilePath(baseName);
        QVariantMap itemData = index.data(contentType::data).toMap();
        QVariantMap oldMimeToExtension = itemData.value(mimeExtensionMap).toMap();
        QVariantMap mimeToExtension;
        QVariantMap dataMapUnknown;

        const QVariantMap noSaveData = itemData.value(mimeNoSave).toMap();

        for ( const auto &format : itemData.keys() ) {
            if ( format.startsWith(COPYQ_MIME_PREFIX_ITEMSYNC) )
                continue; // skip internal data

            const QByteArray bytes = itemData[format].toByteArray();
            const Hash hash = calculateHash(bytes);

            if ( noSaveData.contains(format) && noSaveData[format].toByteArray() == hash ) {
                itemData.remove(format);
                continue;
            }

            bool hasFile = oldMimeToExtension.contains(format);
            const QString ext = hasFile ? oldMimeToExtension[format].toString()
                                        : findByFormat(format, m_formatSettings);

            if ( !hasFile && ext.isEmpty() ) {
                dataMapUnknown.insert(format, bytes);
            } else {
                mimeToExtension.insert(format, ext);
                const Hash oldHash = indexData(index).formatHash.value(format);
                if ( !saveItemFile(filePath + ext, bytes, &existingFiles, hash != oldHash) )
                    return;
            }
        }

        for ( auto it = oldMimeToExtension.constBegin();
              it != oldMimeToExtension.constEnd(); ++it )
        {
            if ( it.key().startsWith(mimeNoFormat) )
                mimeToExtension.insert( it.key(), it.value() );
        }

        if ( mimeToExtension.isEmpty() || !dataMapUnknown.isEmpty() ) {
            mimeToExtension.insert(mimeUnknownFormats, dataFileSuffix);
            QByteArray data = serializeData(dataMapUnknown);
            if ( !saveItemFile(filePath + dataFileSuffix, data, &existingFiles) )
                return;
        }

        if ( !noSaveData.isEmpty() || mimeToExtension != oldMimeToExtension ) {
            itemData.remove(mimeNoSave);

            for ( const auto &format : mimeToExtension.keys() )
                oldMimeToExtension.remove(format);

            itemData.insert(mimeExtensionMap, mimeToExtension);
            updateIndexData(index, itemData);

            // Remove files of removed formats.
            removeFormatFiles(filePath, oldMimeToExtension);
        }
    }

    unlock();
}

bool FileWatcher::renameMoveCopy(const QDir &dir, const QList<QPersistentModelIndex> &indexList)
{
    QStringList baseNames;

    for (const auto &index : indexList) {
        if ( !index.isValid() )
            continue;

        IndexDataList::iterator it = findIndexData(index);
        const QString olderBaseName = (it != m_indexData.end()) ? it->baseName : QString();
        const QString oldBaseName = getBaseName(index);
        QString baseName = oldBaseName;

        bool newItem = olderBaseName.isEmpty();
        bool itemRenamed = olderBaseName != baseName;
        if ( newItem || itemRenamed ) {
            if ( !renameToUnique(dir, baseNames, &baseName, m_formatSettings) )
                return false;
            itemRenamed = olderBaseName != baseName;
            baseNames.append(baseName);
        }

        QVariantMap itemData = index.data(contentType::data).toMap();
        const QString syncPath = itemData.value(mimeSyncPath).toString();
        bool copyFilesFromOtherTab = !syncPath.isEmpty() && syncPath != m_path;

        if (copyFilesFromOtherTab || itemRenamed) {
            const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
            const QString newBasePath = m_path + '/' + baseName;

            if ( !syncPath.isEmpty() ) {
                copyFormatFiles(syncPath + '/' + oldBaseName, newBasePath, mimeToExtension);
            } else {
                // Move files.
                if ( !olderBaseName.isEmpty() )
                    moveFormatFiles(m_path + '/' + olderBaseName, newBasePath, mimeToExtension);
            }

            itemData.remove(mimeSyncPath);
            itemData.insert(mimeBaseName, baseName);
            updateIndexData(index, itemData);

            if ( oldBaseName.isEmpty() && itemData.contains(mimeUriList) ) {
                if ( copyFilesFromUriList(itemData[mimeUriList].toByteArray(), index.row(), baseNames) )
                     m_model->removeRow(index.row());
            }
        }
    }

    return true;
}

void FileWatcher::updateDataAndWatchFile(const QDir &dir, const BaseNameExtensions &baseNameWithExts,
                            QVariantMap *dataMap, QVariantMap *mimeToExtension)
{
    const QString basePath = dir.absoluteFilePath(baseNameWithExts.baseName);

    for (const auto &ext : baseNameWithExts.exts) {
        if ( ext.format.isEmpty() )
            continue;

        const QString fileName = basePath + ext.extension;

        QFile f( dir.absoluteFilePath(fileName) );
        if ( !f.open(QIODevice::ReadOnly) )
            continue;

        if ( ext.extension == dataFileSuffix ) {
            if ( deserializeData(dataMap, f.readAll()) )
                mimeToExtension->insert(mimeUnknownFormats, dataFileSuffix);
        } else if ( f.size() > sizeLimit || ext.format.startsWith(mimeNoFormat)
                    || dataMap->contains(ext.format) )
        {
            mimeToExtension->insert(mimeNoFormat + ext.extension, ext.extension);
        } else {
            dataMap->insert(ext.format, f.readAll());
            mimeToExtension->insert(ext.format, ext.extension);
        }
    }
}

bool FileWatcher::copyFilesFromUriList(const QByteArray &uriData, int targetRow, const QStringList &baseNames)
{
    QMimeData tmpData;
    tmpData.setData(mimeUriList, uriData);

    bool copied = false;

    const QDir dir(m_path);

    for ( const auto &url : tmpData.urls() ) {
        if ( url.isLocalFile() ) {
            QFile f(url.toLocalFile());

            if (f.exists()) {
                QString extName;
                QString baseName;
                getBaseNameAndExtension( QFileInfo(f).fileName(), &baseName, &extName,
                                         m_formatSettings );

                if ( renameToUnique(dir, baseNames, &baseName, m_formatSettings) ) {
                    const QString targetFilePath = dir.absoluteFilePath(baseName + extName);
                    f.copy(targetFilePath);
                    Ext ext;
                    if ( m_model->rowCount() < m_maxItems
                         && getBaseNameExtension(targetFilePath, m_formatSettings, &baseName, &ext) )
                    {
                            BaseNameExtensions baseNameExts(baseName, QList<Ext>() << ext);
                            createItemFromFiles( QDir(m_path), baseNameExts, targetRow );
                            copied = true;
                    }
                }
            }
        }
    }

    return copied;
}
