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

#include "filewatcher.h"

#include "common/contenttype.h"
#include "common/log.h"
#include "item/serialize.h"

#include <QAbstractItemModel>
#include <QCryptographicHash>
#include <QDir>
#include <QMimeData>
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

const int updateItemsIntervalMs = 5000; // Interval to update items after a file has changed.

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
                    return Ext( QString(), format.itemMime );
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

bool canUseFile(QFileInfo &info)
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
QStringList listFiles(const QDir &dir, const QDir::SortFlags &sortFlags = QDir::NoSort)
{
    QStringList files;

    const QDir::Filters itemFileFilter = QDir::Files | QDir::Readable | QDir::Writable;
    for ( const auto &fileName : dir.entryList(itemFileFilter, sortFlags) ) {
        const QString path = dir.absoluteFilePath(fileName);
        QFileInfo info(path);
        if ( canUseFile(info) )
            files.append(path);
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
    for ( const auto &extValue : mimeToExtension.values() ) {
        const QString ext = extValue.toString();
        QFile::rename(oldPath + ext, newPath + ext);
    }
}

void copyFormatFiles(const QString &oldPath, const QString &newPath,
                     const QVariantMap &mimeToExtension)
{
    for ( const auto &extValue : mimeToExtension.values() ) {
        const QString ext = extValue.toString();
        QFile::copy(oldPath + ext, newPath + ext);
    }
}

} // namespace

QString FileWatcher::getBaseName(const QModelIndex &index)
{
    return index.data(contentType::data).toMap().value(mimeBaseName).toString();
}

void FileWatcher::removeFormatFiles(const QString &path, const QVariantMap &mimeToExtension)
{
    for ( const auto &extValue : mimeToExtension.values() )
        QFile::remove(path + extValue.toString());
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
#ifdef HAS_TESTS
    // Use smaller update interval for tests.
    if ( qgetenv("COPYQ_TEST_ID").isEmpty() )
        m_updateTimer.setInterval(updateItemsIntervalMs);
    else
        m_updateTimer.setInterval(100);
#else
    m_updateTimer.setInterval(updateItemsIntervalMs);
#endif
    m_updateTimer.setSingleShot(true);
    connect( &m_updateTimer, SIGNAL(timeout()),
             SLOT(updateItems()) );

    connect( m_model.data(), SIGNAL(rowsInserted(QModelIndex, int, int)),
             this, SLOT(onRowsInserted(QModelIndex, int, int)), Qt::UniqueConnection );
    connect( m_model.data(), SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)),
             this, SLOT(onRowsRemoved(QModelIndex, int, int)), Qt::UniqueConnection );
    connect( m_model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(onDataChanged(QModelIndex,QModelIndex)), Qt::UniqueConnection );

    if (model->rowCount() > 0)
        saveItems(0, model->rowCount() - 1);

    createItemsFromFiles( QDir(path), listFiles(paths, m_formatSettings) );

    updateItems();
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

bool FileWatcher::createItemFromFiles(const QDir &dir, const BaseNameExtensions &baseNameWithExts, int targetRow)
{
    QVariantMap dataMap;
    QVariantMap mimeToExtension;

    updateDataAndWatchFile(dir, baseNameWithExts, &dataMap, &mimeToExtension);

    if ( !mimeToExtension.isEmpty() ) {
        dataMap.insert( mimeBaseName, QFileInfo(baseNameWithExts.baseName).fileName() );
        dataMap.insert(mimeExtensionMap, mimeToExtension);

        if ( !createItem(dataMap, targetRow) )
            return false;
    }

    return true;
}

void FileWatcher::createItemsFromFiles(const QDir &dir, const BaseNameExtensionsList &fileList)
{
    for (const auto &baseNameWithExts : fileList) {
        if ( !createItemFromFiles(dir, baseNameWithExts, 0)
             || m_model->rowCount() >= m_maxItems )
        {
            break;
        }
    }
}

void FileWatcher::updateItems()
{
    m_updateTimer.stop();

    if ( !lock() )
        return;

    QDir dir(m_path);
    const QStringList files = listFiles(dir, QDir::Time | QDir::Reversed);
    BaseNameExtensionsList fileList = listFiles(files, m_formatSettings);

    for ( int row = 0; row < m_model->rowCount(); ++row ) {
        const QModelIndex index = m_model->index(row, 0);
        const QString baseName = getBaseName(index);

        int i = 0;
        for ( i = 0; i < fileList.size() && fileList[i].baseName != baseName; ++i ) {}

        QVariantMap dataMap;
        QVariantMap mimeToExtension;

        if ( i < fileList.size() ) {
            updateDataAndWatchFile(dir, fileList[i], &dataMap, &mimeToExtension);
            fileList.removeAt(i);
        }

        if ( mimeToExtension.isEmpty() ) {
            m_model->removeRow(row--);
        } else {
            dataMap.insert(mimeBaseName, baseName);
            dataMap.insert(mimeExtensionMap, mimeToExtension);
            updateIndexData(index, dataMap);
        }
    }

    createItemsFromFiles(dir, fileList);

    unlock();

    m_updateTimer.start();
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
        Q_ASSERT(index.isValid());
        IndexDataList::iterator it = findIndexData(index);
        Q_ASSERT( it != m_indexData.end() );
        m_indexData.erase(it);
    }
}

FileWatcher::IndexDataList::iterator FileWatcher::findIndexData(const QModelIndex &index)
{
    return qFind(m_indexData.begin(), m_indexData.end(), index);
}

FileWatcher::IndexData &FileWatcher::indexData(const QModelIndex &index)
{
    IndexDataList::iterator it = findIndexData(index);
    if ( it == m_indexData.end() )
        return *m_indexData.insert( m_indexData.end(), IndexData(index) );
    return *it;
}

bool FileWatcher::createItem(const QVariantMap &dataMap, int targetRow)
{
    const int row = qMax( 0, qMin(targetRow, m_model->rowCount()) );
    if ( m_model->insertRow(row) ) {
        const QModelIndex &index = m_model->index(row, 0);
        updateIndexData(index, dataMap);
        return true;
    }

    return false;
}

void FileWatcher::updateIndexData(const QModelIndex &index, const QVariantMap &itemData)
{
    m_model->setData(index, itemData, contentType::data);

    // Item base name is non-empty.
    const QString baseName = getBaseName(index);
    Q_ASSERT( !baseName.isEmpty() );

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

QList<QModelIndex> FileWatcher::indexList(int first, int last)
{
    QList<QModelIndex> indexList;
    for (int i = first; i <= last; ++i)
        indexList.append( m_model->index(i, 0) );
    return indexList;
}

void FileWatcher::saveItems(int first, int last)
{
    if ( !lock() )
        return;

    const QList<QModelIndex> indexList = this->indexList(first, last);

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

        for ( QVariantMap::const_iterator it = oldMimeToExtension.begin();
              it != oldMimeToExtension.end(); ++it )
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

bool FileWatcher::renameToUnique(const QDir &dir, const QStringList &baseNames, QString *name)
{
    if ( name->isEmpty() ) {
        *name = "copyq_0000";
    } else {
        // Replace/remove unsafe characters.
        name->replace( QRegExp("/|\\\\|^\\."), QString("_") );
        name->remove( QRegExp("\\n|\\r") );
    }

    const QStringList fileNames = dir.entryList();

    if ( isUniqueBaseName(*name, fileNames, baseNames) )
        return true;

    QString ext;
    QString baseName;
    getBaseNameAndExtension(*name, &baseName, &ext, m_formatSettings);

    int i = 0;
    int fieldWidth = 0;

    QRegExp re("\\d+$");
    if ( baseName.indexOf(re) != -1 ) {
        const QString num = re.cap(0);
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

bool FileWatcher::renameMoveCopy(const QDir &dir, const QList<QModelIndex> &indexList)
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
            if ( !renameToUnique(dir, baseNames, &baseName) )
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
        Q_ASSERT( !ext.format.isEmpty() );

        const QString fileName = basePath + ext.extension;

        QFile f( dir.absoluteFilePath(fileName) );
        if ( !f.open(QIODevice::ReadOnly) )
            continue;

        if ( ext.extension == dataFileSuffix && deserializeData(dataMap, f.readAll()) ) {
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

                if ( renameToUnique(dir, baseNames, &baseName) ) {
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
