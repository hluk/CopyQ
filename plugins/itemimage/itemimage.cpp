/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "itemimage.h"

#include <QModelIndex>
#include <QtPlugin>
#include <QVariant>

ItemImage::ItemImage(QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
{
    setMargin(4);
}

void ItemImage::setData(const QModelIndex &index)
{
    const QVariant displayData = index.data(Qt::DisplayRole);
    setPixmap(displayData.value<QPixmap>());
    adjustSize();

    updateSize();
    updateItem();
}

void ItemImage::updateSize()
{
    adjustSize();
}

ItemWidget *ItemImageLoader::create(const QModelIndex &index, QWidget *parent)
{
    const QVariant displayData = index.data(Qt::DisplayRole);
    if ( !displayData.canConvert<QPixmap>() )
        return NULL;

    ItemWidget *item = new ItemImage(parent);
    item->setData(index);
    return item;
}

Q_EXPORT_PLUGIN2(itemimage, ItemImageLoader)
