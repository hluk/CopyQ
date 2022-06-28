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

#include "itemstore.h"

#include "common/config.h"
#include "common/log.h"
#include "common/textdata.h"
#include "item/itemfactory.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace {

/// @return File name for data file with items.
QString itemFileName(const QString &id)
{
    QString part( id.toUtf8().toBase64() );
    part.replace( QChar('/'), QString('-') );
    return getConfigurationFilePath("_tab_") + part + QLatin1String(".dat");
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

} // namespace

ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel &model, ItemFactory *itemFactory, int maxItems)
{
    if ( !createItemDirectory() )
        return nullptr;

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

    if ( !createItemDirectory() )
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
