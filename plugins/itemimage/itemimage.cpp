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

#include "itemimage.h"
#include "ui_itemimagesettings.h"

#include "common/contenttype.h"
#include "item/itemeditor.h"

#include <QBuffer>
#include <QHBoxLayout>
#include <QModelIndex>
#include <QMovie>
#include <QPixmap>
#include <QtPlugin>
#include <QVariant>

namespace {

QString findImageFormat(const QList<QString> &formats)
{
    // Check formats in this order.
    static const QStringList imageFormats = QStringList()
            << QString("image/png")
            << QString("image/bmp")
            << QString("image/jpeg")
            << QString("image/gif")
            << QString("image/svg+xml");

    for (const auto &format : imageFormats) {
        if ( formats.contains(format) )
            return format;
    }

    return QString();
}

bool getImageData(const QModelIndex &index, QByteArray *data, QString *mime)
{
    QVariantMap dataMap = index.data(contentType::data).toMap();

    *mime = findImageFormat(dataMap.keys());
    if ( mime->isEmpty() )
        return false;

    *data = dataMap[*mime].toByteArray();

    return true;
}

bool getAnimatedImageData(const QModelIndex &index, QByteArray *data, QByteArray *format)
{
    QVariantMap dataMap = index.data(contentType::data).toMap();

    for (const auto &movieFormat : QMovie::supportedFormats()) {
        const QByteArray mime = "image/" + movieFormat;
        if (dataMap.contains(mime)) {
            *format = movieFormat;
            *data = dataMap[mime].toByteArray();
            return true;
        }
    }

    return false;
}

bool getPixmapFromData(const QModelIndex &index, QPixmap *pix)
{
    QString mime;
    QByteArray data;
    if ( !getImageData(index, &data, &mime) )
        return false;

    pix->loadFromData( data, mime.toLatin1() );

    return true;
}

} // namespace

ItemImage::ItemImage(
        const QPixmap &pix,
        const QByteArray &animationData, const QByteArray &animationFormat,
        const QString &imageEditor, const QString &svgEditor,
        QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
    , m_editor(imageEditor)
    , m_svgEditor(svgEditor)
    , m_pixmap(pix)
    , m_animationData(animationData)
    , m_animationFormat(animationFormat)
    , m_animation(nullptr)
{
    setMargin(4);
    setPixmap(pix);
}

QObject *ItemImage::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    QString mime;
    QByteArray data;
    if ( !getImageData(index, &data, &mime) )
        return nullptr;

    const QString &cmd = mime.contains("svg") ? m_svgEditor : m_editor;

    return cmd.isEmpty() ? nullptr : new ItemEditor(data, mime, cmd, parent);
}

void ItemImage::showEvent(QShowEvent *event)
{
    startAnimation();
    QLabel::showEvent(event);
}

void ItemImage::hideEvent(QHideEvent *event)
{
    QLabel::hideEvent(event);
    stopAnimation();
}

void ItemImage::startAnimation()
{
    if ( !m_animationData.isEmpty() ) {
        if (!m_animation) {
            auto stream = new QBuffer(&m_animationData, this);
            m_animation = new QMovie(stream, m_animationFormat, this);
            m_animation->setScaledSize( m_pixmap.size() );
        }

        if (m_animation) {
            setMovie(m_animation);
            movie()->start();
            m_animation->start();
        }
    }
}

void ItemImage::stopAnimation()
{
    if (m_animation) {
        m_animation->stop();
        setPixmap(m_pixmap);
    }
}

ItemImageLoader::ItemImageLoader()
{
}

ItemImageLoader::~ItemImageLoader() = default;

ItemWidget *ItemImageLoader::create(const QModelIndex &index, QWidget *parent, bool preview) const
{
    if ( index.data(contentType::isHidden).toBool() )
        return nullptr;

    // TODO: Just check if image provided and load it in different thread.
    QPixmap pix;
    if ( !getPixmapFromData(index, &pix) )
        return nullptr;

    // scale pixmap
    const int w = preview ? 0 : m_settings.value("max_image_width", 320).toInt();
    const int h = preview ? 0 : m_settings.value("max_image_height", 240).toInt();
    if ( w > 0 && pix.width() > w && (h <= 0 || pix.width()/w > pix.height()/h) ) {
        pix = pix.scaledToWidth(w);
    } else if (h > 0 && pix.height() > h) {
        pix = pix.scaledToHeight(h);
    }

    QByteArray animationData;
    QByteArray animationFormat;
    getAnimatedImageData(index, &animationData, &animationFormat);

    return new ItemImage(pix,
                         animationData, animationFormat,
                         m_settings.value("image_editor").toString(),
                         m_settings.value("svg_editor").toString(), parent);
}

QStringList ItemImageLoader::formatsToSave() const
{
    return QStringList()
            << QString("image/svg+xml")
            << QString("image/png")
            << QString("image/gif");
}

QVariantMap ItemImageLoader::applySettings()
{
    m_settings["max_image_width"] = ui->spinBoxImageWidth->value();
    m_settings["max_image_height"] = ui->spinBoxImageHeight->value();
    m_settings["image_editor"] = ui->lineEditImageEditor->text();
    m_settings["svg_editor"] = ui->lineEditSvgEditor->text();
    return m_settings;
}

QWidget *ItemImageLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemImageSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->spinBoxImageWidth->setValue( m_settings.value("max_image_width", 320).toInt() );
    ui->spinBoxImageHeight->setValue( m_settings.value("max_image_height", 240).toInt() );
    ui->lineEditImageEditor->setText( m_settings.value("image_editor", "").toString() );
    ui->lineEditSvgEditor->setText( m_settings.value("svg_editor", "").toString() );
    return w;
}

Q_EXPORT_PLUGIN2(itemimage, ItemImageLoader)
