/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "itemstore.h"

#include "common/config.h"
#include "common/log.h"
#include "common/textdata.h"
#include "item/itemfactory.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>

namespace {

/// @return File name for data file with items.
QString itemFileName(const QString &id)
{
    QString part( id.toUtf8().toBase64() );
    part.replace( QChar('/'), QString('-') );
    return getConfigurationFilePath("_tab_") + part + QString(".dat");
}

bool createItemDirectory()
{
    QDir settingsDir( settingsDirectoryPath() );
    if ( !settingsDir.mkpath(".") ) {
        log( QString("Cannot create directory for settings %1!")
             .arg(quoteString(settingsDir.path()) ),
             LogError );

        return false;
    }

    return true;
}

void printItemFileError(
        const QString &action, const QString &id, const QString &fileName, const QFile &file)
{
    log( QString("Cannot %1 tab %2 to %3 (%4)!")
         .arg(action)
         .arg( quoteString(id) )
         .arg( quoteString(fileName) )
         .arg( file.errorString() )
         , LogError );
}

void printSaveItemFileError(const QString &id, const QString &fileName, const QFile &file)
{
    printItemFileError("save", id, fileName, file);
}

void printLoadItemFileError(const QString &id, const QString &fileName, const QFile &file)
{
    printItemFileError("load", id, fileName, file);
}

ItemSaverPtr loadItems(
        const QString &tabName, const QString &tabFileName,
        QAbstractItemModel &model, ItemFactory *itemFactory, int maxItems)
{
    COPYQ_LOG( QString("Tab \"%1\": Loading items").arg(tabName) );

    QFile tabFile(tabFileName);
    if ( !tabFile.open(QIODevice::ReadOnly) ) {
        printLoadItemFileError(tabName, tabFileName, tabFile);
        return nullptr;
    }

    auto loader = itemFactory->loadItems(tabName, &model, &tabFile, maxItems);
    if (!loader) {
        const QString errorString =
                QObject::tr("Item file %1 is corrupted or some CopyQ plugins are missing!")
                .arg( quoteString(tabFileName) );
        itemFactory->emitError(errorString);
        return nullptr;
    }

    return loader;
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

} // namespace

ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel &model, ItemFactory *itemFactory, int maxItems)
{
    if ( !createItemDirectory() )
        return nullptr;

    const QString tabFileName = itemFileName(tabName);

    // If tab file doesn't exist, try to restore data from temporary file.
    if ( !QFile::exists(tabFileName) ) {
        QFile tmpFile(tabFileName + ".tmp");
        if ( tmpFile.exists() ) {
            log( QString("Tab \"%1\": Restoring items (previous save failed)"), LogWarning );
            if ( !tmpFile.rename(tabFileName) ) {
                printLoadItemFileError(tabName, tabFileName, tmpFile);
                return nullptr;
            }
        }
    }

    // Load file with items or create new file.
    auto saver = QFile::exists(tabFileName)
            ? loadItems(tabName, tabFileName, model, itemFactory, maxItems)
            : createTab(tabName, model, itemFactory, maxItems);

    if (!saver) {
        model.removeRows(0, model.rowCount());
        return nullptr;
    }

    COPYQ_LOG( QString("Tab \"%1\": %2 items loaded").arg(tabName).arg(model.rowCount()) );

    return saver;
}

bool saveItems(const QString &tabName, const QAbstractItemModel &model, const ItemSaverPtr &saver)
{
    const QString tabFileName = itemFileName(tabName);

    if ( !createItemDirectory() )
        return false;

    // Save to temp file.
    QFile tmpFile( tabFileName + ".tmp" );
    if ( !tmpFile.open(QIODevice::WriteOnly) ) {
        printSaveItemFileError(tabName, tabFileName, tmpFile);
        return false;
    }

    COPYQ_LOG( QString("Tab \"%1\": Saving %2 items").arg(tabName).arg(model.rowCount()) );

    if ( !saver->saveItems(tabName, model, &tmpFile) ) {
        COPYQ_LOG( QString("Tab \"%1\": Failed to save items!").arg(tabName) );
        return false;
    }

    // 1. Safely flush all data to temporary file.
    tmpFile.flush();

    // 2. Remove old tab file.
    {
        QFile oldTabFile(tabFileName);
        if (oldTabFile.exists() && !oldTabFile.remove()) {
            printSaveItemFileError(tabName, tabFileName, oldTabFile);
            return false;
        }
    }

    // 3. Overwrite previous file.
    if ( !tmpFile.rename(tabFileName) ) {
        printSaveItemFileError(tabName, tabFileName, tmpFile);
        return false;
    }

    COPYQ_LOG( QString("Tab \"%1\": Items saved").arg(tabName) );

    return true;
}

void removeItems(const QString &tabName)
{
    const QString tabFileName = itemFileName(tabName);
    QFile::remove(tabFileName);
    QFile::remove(tabFileName + ".tmp");
}

void moveItems(const QString &oldId, const QString &newId)
{
    const QString oldFileName = itemFileName(oldId);
    const QString newFileName = itemFileName(newId);

    if ( oldFileName != newFileName && QFile::copy(oldFileName, newFileName) ) {
        QFile::remove(oldFileName);
    } else {
        COPYQ_LOG( QString("Failed to move items from \"%1\" (tab \"%2\") to \"%3\" (tab \"%4\")")
                   .arg(oldFileName).arg(oldId)
                   .arg(newFileName).arg(newId) );
    }
}
