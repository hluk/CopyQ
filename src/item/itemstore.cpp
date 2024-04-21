// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemstore.h"

#include "common/config.h"
#include "common/log.h"
#include "common/textdata.h"
#include "item/itemfactory.h"
#include "item/serialize.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>

namespace {

/// @return File name for data file with items.
QString itemFileName(const QString &id)
{
    QString part( id.toUtf8().toBase64() );
    part.replace( QChar('/'), QChar('-') );
    return getConfigurationFilePath("_tab_") + part + QLatin1String(".dat");
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

bool itemDataFiles(const QString &tabName, QStringList *files)
{
    const QString tabFileName = itemFileName(tabName);
    if ( !QFile::exists(tabFileName) )
        return true;

    QFile tabFile(tabFileName);
    if ( !tabFile.open(QIODevice::ReadOnly) ) {
        printItemFileError("read tab", tabName, tabFile);
        return false;
    }

    return itemDataFiles(&tabFile, files);
}

void cleanDataDir(QDir *dir)
{
    if ( dir->isEmpty() )
        QDir().rmdir( dir->absolutePath() );
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

    log( QStringLiteral("Tab \"%1\": Failed to load tab file: %2")
            .arg(tabName, tabFileName), LogError );
    model.removeRows(0, model.rowCount());
    return nullptr;
}

bool saveItems(const QString &tabName, const QAbstractItemModel &model, const ItemSaverPtr &saver)
{
    const QString tabFileName = itemFileName(tabName);

    if ( !ensureSettingsDirectoryExists() )
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

    if ( oldFileName != newFileName && QFile::copy(oldFileName, newFileName) ) {
        QFile::remove(oldFileName);
        return true;
    }

    log( QString("Failed to move items from \"%1\" (tab \"%2\") to \"%3\" (tab \"%4\")").arg(
             oldFileName,
             oldId,
             newFileName,
             newId
       ), LogError );

    return false;
}

void cleanDataFiles(const QStringList &tabNames)
{
    QDir dir(itemDataPath());
    if ( !dir.exists() )
        return;

    QStringList files;
    for (const QString &tabName : tabNames) {
        if ( !itemDataFiles(tabName, &files) ) {
            COPYQ_LOG( QStringLiteral("Stopping cleanup due to corrupted file: %1")
                    .arg(tabName) );
            return;
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
    const QSet<QString> fileSet(files.constBegin(), files.constEnd());
#else
    const QSet<QString> fileSet = files.toSet();
#endif
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
