// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemstore.h"

#include "common/config.h"
#include "common/log.h"
#include "common/textdata.h"
#include "item/itemfactory.h"
#include "item/serialize.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSaveFile>
#include <QSet>

namespace {

/// @return File name for data file with items.
QString itemFileName(const QString &id)
{
    QString part( id.toUtf8().toBase64() );
    part.replace( QChar('/'), QChar('-') );
    return tabDataFileBasePath() + part + QLatin1String(".dat");
}

void printItemFileError(
        const QString &action, const QString &id, const QFileDevice &file)
{
    log( QString("Tab %1: Failed to %2, file %3: %4").arg(
             quoteString(id),
             action,
             quoteString(file.fileName()),
             file.errorString()
         ), LogError );
}

ItemSaverPtr loadItems(
        const QString &tabName, const QString &tabFileName,
        QAbstractItemModel &model, ItemFactory *itemFactory, int maxItems)
{
    COPYQ_LOG( QString("Tab \"%1\": Loading items from: %2").arg(tabName, tabFileName) );

    QFile tabFile(tabFileName);
    if ( !tabFile.open(QIODevice::ReadOnly) ) {
        printItemFileError("load tab", tabName, tabFile);
        return nullptr;
    }

    return itemFactory->loadItems(tabName, &model, &tabFile, maxItems);
}

ItemSaverPtr createTab(
        const QString &tabName, QAbstractItemModel &model, ItemFactory *itemFactory, int maxItems)
{
    COPYQ_LOG( QString("Tab \"%1\": Creating new tab").arg(tabName) );

    auto saver = itemFactory->initializeTab(tabName, &model, maxItems);
    if (!saver) {
        log( QString("Tab \"%1\": Failed to create new tab"), LogError );
        return nullptr;
    }

    if ( !saveItems(tabName, model, saver) )
        return nullptr;

    return saver;
}

bool itemDataFiles(const QString &tabName, QStringList *files, const Encryption::EncryptionKey *encryptionKey = nullptr)
{
    const QString tabFileName = itemFileName(tabName);
    if ( !QFile::exists(tabFileName) )
        return true;

    QFile tabFile(tabFileName);
    if ( !tabFile.open(QIODevice::ReadOnly) ) {
        printItemFileError("read tab", tabName, tabFile);
        return false;
    }

    return itemDataFiles(&tabFile, files, encryptionKey);
}

void cleanDataDir(QDir *dir)
{
    if ( dir->isEmpty() )
        QDir().rmdir( dir->absolutePath() );
}

bool migrateFiles(const QString &oldDir, const QString &newDir, const QStringList &patterns,
                  LogLevel successLogLevel = LogNote)
{
    if (oldDir == newDir)
        return true;

    QDir src(oldDir);
    QDir dst(newDir);

    QStringList files;
    for (const auto &pattern : patterns)
        files.append(src.entryList({pattern}, QDir::Files));
    if (files.isEmpty())
        return true;

    bool ok = true;
    for (const QString &fileName : files) {
        const QString oldPath = src.absoluteFilePath(fileName);
        const QString newPath = dst.absoluteFilePath(fileName);

        if (QFile::exists(newPath)) {
            log(QStringLiteral("Migration: Skipping %1 — destination already exists: %2")
                .arg(oldPath, newPath), LogWarning);
            continue;
        }

        QFile file(oldPath);
        if (file.rename(newPath)) {
            log(QStringLiteral("Migration: Moved %1 to %2")
                .arg(oldPath, newPath), successLogLevel);
        } else {
            const QString renameError = file.errorString();
            // Cross-device fallback
            if (file.copy(newPath)) {
                QFile::remove(oldPath);
                log(QStringLiteral("Migration: Copied %1 to %2 (cross-device)")
                    .arg(oldPath, newPath), successLogLevel);
            } else {
                log(QStringLiteral("Migration: Failed to move %1 to %2: rename: %3, copy: %4")
                    .arg(oldPath, newPath, renameError, file.errorString()), LogWarning);
                ok = false;
            }
        }
    }
    return ok;
}

} // namespace

ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel &model, ItemFactory *itemFactory, int maxItems)
{
    const QString tabFileName = itemFileName(tabName);
    if ( !QFile::exists(tabFileName) )
        return createTab(tabName, model, itemFactory, maxItems);

    ItemSaverPtr saver = loadItems(tabName, tabFileName, model, itemFactory, maxItems);
    if (saver) {
        COPYQ_LOG( QStringLiteral("Tab \"%1\": %2 items loaded from: %3")
                      .arg(tabName, QString::number(model.rowCount()), tabFileName) );
        return saver;
    }

    log( QStringLiteral("Tab \"%1\": Cannot load tab file: %2")
            .arg(tabName, tabFileName) );
    model.removeRows(0, model.rowCount());
    return nullptr;
}

bool saveItems(const QString &tabName, const QAbstractItemModel &model, const ItemSaverPtr &saver)
{
    const QString tabFileName = itemFileName(tabName);

    if ( !QFileInfo(itemDataPath()).dir().mkpath(QStringLiteral(".")) )
        return false;

    // Save tab data to a new temporary file.
    QSaveFile tabFile(tabFileName);
    tabFile.setDirectWriteFallback(false);
    if ( !tabFile.open(QIODevice::WriteOnly) ) {
        printItemFileError("save tab (open temporary file)", tabName, tabFile);
        return false;
    }

    COPYQ_LOG( QStringLiteral("Tab \"%1\": Saving %2 items")
                  .arg(tabName, QString::number(model.rowCount())) );

    if ( !saver->saveItems(tabName, model, &tabFile) ) {
        tabFile.cancelWriting();
        printItemFileError("save tab (save items to temporary file)", tabName, tabFile);
        return false;
    }

    if ( !tabFile.flush() ) {
        tabFile.cancelWriting();
        printItemFileError("save tab (flush to temporary file)", tabName, tabFile);
        return false;
    }

    if ( !tabFile.commit() ) {
        printItemFileError("save tab (commit)", tabName, tabFile);
        return false;
    }

    COPYQ_LOG( QStringLiteral("Tab \"%1\": Items saved").arg(tabName) );

    return true;
}

void removeItems(const QString &tabName)
{
    const QString tabFileName = itemFileName(tabName);
    QFile::remove(tabFileName);
}

bool moveItems(const QString &oldId, const QString &newId)
{
    const QString oldFileName = itemFileName(oldId);
    const QString newFileName = itemFileName(newId);

    QString error;
    if (oldFileName == newFileName) {
        error = QStringLiteral("Cannot move to the same destination");
    } else {
        QFile source(oldFileName);

        // Skip if source tab was not yet saved
        if (!source.exists())
            return true;

        if ( source.open(QFile::ReadOnly) && source.copy(newFileName) ) {
            QFile::remove(oldFileName);
            return true;
        }
        error = source.errorString();
    }

    log( QStringLiteral("Failed to move \"%1\" (tab \"%2\") to \"%3\" (tab \"%4\"): %5")
         .arg(oldFileName, oldId, newFileName, newId, error), LogError );
    return false;
}

void cleanDataFiles(const QStringList &tabNames, const Encryption::EncryptionKey *encryptionKey)
{
    QDir dir(itemDataPath());
    if ( !dir.exists() )
        return;

    QStringList files;
    for (const QString &tabName : tabNames) {
        if ( !itemDataFiles(tabName, &files, encryptionKey) ) {
            COPYQ_LOG( QStringLiteral("Stopping data file cleanup: failed to load or decrypt some files for tab %1")
                    .arg(tabName) );
            return;
        }
    }

    const QSet<QString> fileSet(files.constBegin(), files.constEnd());
    for ( const auto &i1 : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot) ) {
        QDir d1(i1.absoluteFilePath());
        for ( const auto &i2 : d1.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot) ) {
            QDir d2(i2.absoluteFilePath());
            for ( const auto &i3 : d2.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot) ) {
                QDir d3(i3.absoluteFilePath());
                for ( const auto &f : d3.entryInfoList(QDir::Files) ) {
                    const QString path = f.absoluteFilePath();
                    if ( !fileSet.contains(path) ) {
                        COPYQ_LOG( QStringLiteral("Cleaning: %1").arg(path) );
                        QFile::remove(path);
                    }
                }
                cleanDataDir(&d3);
            }
            cleanDataDir(&d2);
        }
        cleanDataDir(&d1);
    }
}

void migrateDataFiles()
{
    const QString oldDir = settingsDirectoryPath();
    const QString sentinelPath = statePath() + QLatin1String("/copyq_migrated");
    if (QFile::exists(sentinelPath))
        return;

    const QString dataDir = QFileInfo(itemDataPath()).path();
    QDir(dataDir).mkpath(QStringLiteral("."));
    if ( !ensureStateDirectoryExists() )
        return;

    const QString appName = QCoreApplication::applicationName();

    bool ok = true;

    // Tab data -> data directory
    if ( !migrateFiles(oldDir, dataDir, {
             appName + QLatin1String("_tab_*.dat"),
             appName + QLatin1String("_tab_*.dat.tmp")}) )
    {
        ok = false;
    }

    // State files -> state directory
    if ( !migrateFiles(oldDir, statePath(), {
             appName + QLatin1String("_geometry.ini"),
             appName + QLatin1String("_tabs.ini"),
             appName + QLatin1String("-filter.ini"),
             appName + QLatin1String("-monitor.ini"),
             appName + QLatin1String("_cmds.dat")}) )
    {
        ok = false;
    }

    // Log files -> logs subdirectory (previously in data directory)
    const QString logDir = statePath() + QLatin1String("/logs");
    QDir(logDir).mkpath(QStringLiteral("."));
    if ( !migrateFiles(
             QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation),
             logDir, {
                 QStringLiteral("copyq-*.log"),
                 QStringLiteral("copyq-*.log.*")},
             LogDebug) )
    {
        ok = false;
    }

    if (!ok) {
        log(QStringLiteral("Migration: Some files failed to migrate; will retry on next startup"),
            LogWarning);
        return;
    }

    QFile sentinel(sentinelPath);
    if (sentinel.open(QIODevice::WriteOnly)) {
        sentinel.close();
    } else {
        log(QStringLiteral("Migration: Failed to create sentinel file %1: %2")
            .arg(sentinelPath, sentinel.errorString()), LogWarning);
    }
}
