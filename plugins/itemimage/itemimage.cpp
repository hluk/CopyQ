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
#include "ui_itemimagesettings.h"

#include "contenttype.h"

#include <QHBoxLayout>
#include <QModelIndex>
#include <QPixmap>
#include <QtPlugin>
#include <QVariant>

namespace {

const QStringList imageFormats =
        QStringList("image/svg+xml") << QString("image/bmp") << QString("image/png")
                                     << QString("image/jpeg") << QString("image/gif");

bool getPixmapFromData(const QModelIndex &index, QPixmap *pix)
{
    const QStringList formats = index.data(contentType::formats).toStringList();

    int i = -1;
    foreach (const QString &format, imageFormats) {
        i = formats.indexOf(format);
        if (i != -1)
            break;
    }

    if (i == -1)
        return false;

    const QString &mimeType = formats[i];
    const QByteArray data = index.data(contentType::firstFormat + i).toByteArray();
    pix->loadFromData( data, mimeType.toLatin1() );

    return true;
}

} // namespace

ItemImage::ItemImage(const QPixmap &pix, QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
{
    setMargin(4);
    setPixmap(pix);
    adjustSize();
    updateSize();
    updateItem();
}

void ItemImage::updateSize()
{
    adjustSize();
}

ItemImageLoader::ItemImageLoader()
    : ui(NULL)
{
}

ItemImageLoader::~ItemImageLoader()
{
    delete ui;
}

ItemWidget *ItemImageLoader::create(const QModelIndex &index, QWidget *parent) const
{
    // TODO: Just check if image provided and load it in different thread.
    QPixmap pix;
    if ( !getPixmapFromData(index, &pix) )
        return NULL;

    // scale pixmap
    const int w = m_settings.value("max_image_width", 320).toInt();
    const int h = m_settings.value("max_image_height", 240).toInt();
    if ( w > 0 && pix.width() > w && (h <= 0 || pix.width()/w > pix.height()/h) ) {
        pix = pix.scaledToWidth(w);
    } else if (h > 0 && pix.height() > h) {
        pix = pix.scaledToHeight(h);
    }

    return new ItemImage(pix, parent);
}

QStringList ItemImageLoader::formatsToSave() const
{
    return QStringList("image/svg+xml") << QString("image/bmp") << QString("image/png")
                                        << QString("image/jpeg") << QString("image/gif");
}

QVariantMap ItemImageLoader::applySettings()
{
    Q_ASSERT(ui != NULL);
    m_settings["max_image_width"] = ui->spinBoxImageWidth->value();
    m_settings["max_image_height"] = ui->spinBoxImageHeight->value();
    return m_settings;
}

QWidget *ItemImageLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemImageSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->spinBoxImageWidth->setValue( m_settings.value("max_image_width", 320).toInt() );
    ui->spinBoxImageHeight->setValue( m_settings.value("max_image_height", 240).toInt() );
    return w;
}

Q_EXPORT_PLUGIN2(itemimage, ItemImageLoader)
