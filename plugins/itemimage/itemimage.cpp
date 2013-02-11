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
#include <QPixmap>
#include <QtPlugin>
#include <QVariant>

namespace {

bool getPixmapFromData(const QModelIndex &index, QPixmap *pix)
{
    const QStringList formats = ItemLoaderInterface::getFormats(index);

    // TODO: Size from plugin settings?
    QSize size = index.data(Qt::SizeHintRole).toSize();
    const int w = size.width();
    const int h = size.height();

    int i = formats.indexOf("image/svg+xml");
    if (i == -1) {
        i = formats.indexOf("image/bmp");
        if (i == -1) {
            i = formats.indexOf("image/png");
            if (i == -1) {
                i = formats.indexOf("image/jpeg");
                if (i == -1) {
                    i = formats.indexOf("image/gif");
                    if (i == -1)
                        return false;
                }
            }
        }
    }

    const QString &mimeType = formats[i];
    pix->loadFromData( ItemLoaderInterface::getData(i, index), mimeType.toLatin1() );

    if ( w > 0 && pix->width() > w && (h <= 0 || pix->width()/w > pix->height()/h) ) {
        *pix = pix->scaledToWidth(w);
    } else if (h > 0 && pix->height() > h) {
        *pix = pix->scaledToHeight(h);
    }

    return true;
}

} // namespace

ItemImage::ItemImage(QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
{
    setMargin(4);
}

void ItemImage::setData(const QModelIndex &index)
{
    const QVariant displayData = index.data(Qt::DisplayRole);
    QPixmap pix;
    getPixmapFromData(index, &pix);
    setLabelPixmap(pix);
}

void ItemImage::setLabelPixmap(const QPixmap &pix)
{
    setPixmap(pix);
    adjustSize();
    updateSize();
    updateItem();
}

void ItemImage::updateSize()
{
    adjustSize();
}

ItemWidget *ItemImageLoader::create(const QModelIndex &index, QWidget *parent) const
{
    // TODO: Just check if image provided and load it in different thread.
    QPixmap pix;
    if ( !getPixmapFromData(index, &pix) )
        return NULL;

    ItemImage *item = new ItemImage(parent);
    item->setPixmap(pix);
    return item;
}

Q_EXPORT_PLUGIN2(itemimage, ItemImageLoader)
