/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "common/common.h"
#include "common/config.h"
#include "common/log.h"
#include "item/itemfactory.h"
#include "item/clipboardmodel.h"

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
        log( QObject::tr("Cannot create directory for settings %1!")
             .arg(quoteString(settingsDir.path()) ),
             LogError );

        return false;
    }

    return true;
}

void printItemFileError(const QString &id, const QString &fileName, const QFile &file)
{
    log( QObject::tr("Cannot save tab %1 to %2 (%3)!")
         .arg( quoteString(id) )
         .arg( quoteString(fileName) )
         .arg( file.errorString() )
         , LogError );
}

bool needToSaveItemsAgain(const QAbstractItemModel &model, const ItemFactory &itemFactory,
                          const ItemLoaderInterface *currentLoader)
{
    if (!currentLoader)
        return false;

    bool saveWithCurrent = true;
    foreach ( ItemLoaderInterface *loader, itemFactory.loaders() ) {
        if ( itemFactory.isLoaderEnabled(loader) && loader->canSaveItems(model) )
            return loader != currentLoader;
        if (loader == currentLoader)
            saveWithCurrent = false;
    }

    return !saveWithCurrent;
}

} // namespace

ItemLoaderInterface *loadItems(ClipboardModel &model, ItemFactory *itemFactory)
{
    if ( !createItemDirectory() )
        return NULL;

    const QString tabName = model.property("tabName").toString();
    const QString fileName = itemFileName(tabName);

    // Load file with items.
    QFile file(fileName);
    if ( !file.exists() ) {
        // Try to open temporary file if regular file doesn't exist.
        QFile tmpFile(fileName + ".tmp");
        if ( tmpFile.exists() )
            tmpFile.rename(fileName);
    }

    ItemLoaderInterface *loader = NULL;

    model.setDisabled(true);

    if ( file.exists() ) {
        COPYQ_LOG( QString("Tab \"%1\": Loading items").arg(tabName) );
        if ( file.open(QIODevice::ReadOnly) )
            loader = itemFactory->loadItems(&model, &file);
        saveItemsWithOther(model, loader, itemFactory);
    } else {
        COPYQ_LOG( QString("Tab \"%1\": Creating new tab").arg(tabName) );
        if ( file.open(QIODevice::WriteOnly) ) {
            file.close();
            loader = itemFactory->initializeTab(&model);
            saveItems(model, loader);
        }
    }

    file.close();

    if (loader) {
        COPYQ_LOG( QString("Tab \"%1\": %2 items loaded").arg(tabName).arg(model.rowCount()) );
    } else {
        model.removeRows(0, model.rowCount());
        COPYQ_LOG( QString("Tab \"%1\": Disabled").arg(tabName) );
    }

    model.setDisabled(!loader);

    return loader;
}

bool saveItems(
        const ClipboardModel &model, ItemLoaderInterface *loader)
{
    const QString tabName = model.property("tabName").toString();
    const QString fileName = itemFileName(tabName);

    if ( !createItemDirectory() )
        return false;

    // Save to temp file.
    QFile file( fileName + ".tmp" );
    if ( !file.open(QIODevice::WriteOnly) ) {
        printItemFileError(tabName, fileName, file);
        return false;
    }

    COPYQ_LOG( QString("Tab \"%1\": Saving %2 items").arg(tabName).arg(model.rowCount()) );

    if ( loader->saveItems(model, &file) ) {
        // Overwrite previous file.
        QFile oldTabFile(fileName);
        if (oldTabFile.exists() && !oldTabFile.remove())
            printItemFileError(tabName, fileName, oldTabFile);
        else if ( file.rename(fileName) )
            COPYQ_LOG( QString("Tab \"%1\": Items saved").arg(tabName) );
        else
            printItemFileError(tabName, fileName, file);
    } else {
        COPYQ_LOG( QString("Tab \"%1\": Failed to save items!").arg(tabName) );
    }

    return true;
}

bool saveItemsWithOther(
        ClipboardModel &model, ItemLoaderInterface *loader, ItemFactory *itemFactory)
{
    if ( !needToSaveItemsAgain(model, *itemFactory, loader) )
        return false;

    model.setDisabled(true);

    COPYQ_LOG( QString("Tab \"%1\": Saving items using other plugin")
               .arg(model.property("tabName").toString()) );

    loader->uninitializeTab(&model);
    loader = itemFactory->initializeTab(&model);
    if ( loader && saveItems(model, loader) ) {
        model.setDisabled(false);
        return true;
    } else {
        COPYQ_LOG( QString("Tab \"%1\": Failed to re-save items")
               .arg(model.property("tabName").toString()) );
    }

    return false;
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
