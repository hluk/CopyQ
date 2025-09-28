// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemimage.h"
#include "ui_itemimagesettings.h"

#include "common/mimetypes.h"
#include "item/itemeditor.h"
#include "gui/pixelratio.h"

#ifdef HAS_TESTS
#   include "tests/itemimagetests.h"
#endif

#include <QBuffer>
#include <QHBoxLayout>
#include <QModelIndex>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QtPlugin>
#include <QVariant>
#include <QVariantMap>

namespace {

const QLatin1String configMaxImageWidth("max_image_width");
const QLatin1String configMaxImageHeight("max_image_height");
const QLatin1String configImageEditor("image_editor");
const QLatin1String configSvgEditor("svg_editor");

bool getImageData(
    const QVariantMap &dataMap, const QString &format, QByteArray *data, QString *mime, QPixmap *pix = nullptr)
{
    *data = dataMap.value(format).toByteArray();
    if (data->isEmpty())
        return false;

    if (!pix) {
        *mime = format;
        return true;
    }

    if (pix->loadFromData(*data, format.toLatin1())) {
        *mime = format;
        return true;
    }

    return false;
}

bool getPrefferedImageData(
    const QVariantMap &dataMap, QByteArray *data, QString *mime, QPixmap *pix = nullptr)
{
    return getImageData(dataMap, QStringLiteral("image/png"), data, mime, pix)
        || getImageData(dataMap, QStringLiteral("image/bmp"), data, mime, pix)
        || getImageData(dataMap, QStringLiteral("image/jpeg"), data, mime, pix)
        || getImageData(dataMap, QStringLiteral("image/gif"), data, mime, pix);
}

bool getFallbackImageData(
    const QVariantMap &dataMap, QByteArray *data, QString *mime, QPixmap *pix = nullptr)
{
    // Fallback to any other supported image format.
    const QLatin1String prefix("image/");
    for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it) {
        if (!it.key().startsWith(prefix))
            continue;

        if (getImageData(dataMap, it.key(), data, mime, pix))
            return true;
    }

    return false;
}

bool getSvgData(
    const QVariantMap &dataMap, QByteArray *data, QString *mime, QPixmap *pix = nullptr)
{
    return getImageData(dataMap, QStringLiteral("image/svg+xml"), data, mime, pix);
}

bool getAnimatedImageData(const QVariantMap &dataMap, QByteArray *data, QByteArray *format)
{
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

bool getPixmapFromData(const QVariantMap &dataMap, QPixmap *pix)
{
    QString mime;
    QByteArray data;
    return getPrefferedImageData(dataMap, &data, &mime, pix)
        || getSvgData(dataMap, &data, &mime, pix)
        || getFallbackImageData(dataMap, &data, &mime, pix);
}

} // namespace

ItemImage::ItemImage(
        const QPixmap &pix,
        const QByteArray &animationData, const QByteArray &animationFormat,
        QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
    , m_pixmap(pix)
    , m_animationData(animationData)
    , m_animationFormat(animationFormat)
    , m_animation(nullptr)
{
    setMargin(4);
    setPixmap(pix);
}

void ItemImage::updateSize(QSize, int)
{
    const auto m2 = 2 * margin();
    const auto ratio = pixelRatio(this);
    const int w = (m_pixmap.width() + 1) / ratio + m2;
    const int h = (m_pixmap.height() + 1) / ratio + m2;
    setFixedSize( QSize(w, h) );
}

void ItemImage::setCurrent(bool current)
{
    if (current) {
        if ( !m_animationData.isEmpty() ) {
            if (!m_animation) {
                QBuffer *stream = new QBuffer(&m_animationData, this);
                m_animation = new QMovie(stream, m_animationFormat, this);
                m_animation->setScaledSize( m_pixmap.size() );
            }

            if (m_animation) {
                setMovie(m_animation);
                startAnimation();
                m_animation->start();
            }
        }
    } else {
        stopAnimation();
        setPixmap(m_pixmap);
    }
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

void ItemImage::paintEvent(QPaintEvent *event)
{
    // WORKAROUND: Draw current animation frame with correct DPI.
    if (movie()) {
        QPainter painter(this);
        auto pix = m_animation->currentPixmap();
        pix.setDevicePixelRatio( pixelRatio(this) );
        const auto m = margin();
        painter.drawPixmap(m, m, pix);
    } else {
        QLabel::paintEvent(event);
    }
}

void ItemImage::startAnimation()
{
    if (movie())
        movie()->start();
}

void ItemImage::stopAnimation()
{
    if (movie())
        movie()->stop();
}

ItemImageLoader::ItemImageLoader()
{
}

ItemImageLoader::~ItemImageLoader() = default;

ItemWidget *ItemImageLoader::create(const QVariantMap &data, QWidget *parent, bool preview) const
{
    if ( data.value(mimeHidden).toBool() )
        return nullptr;

    QPixmap pix;
    if ( !getPixmapFromData(data, &pix) )
        return nullptr;

    pix.setDevicePixelRatio( pixelRatio(parent) );

    // scale pixmap
    const int w = preview ? 0 : m_maxImageWidth;
    const int h = preview ? 0 : m_maxImageHeight;
    if ( w > 0 && pix.width() > w && (h <= 0 || 1.0 * pix.width()/w > 1.0 * pix.height()/h) ) {
        pix = pix.scaledToWidth(w, Qt::SmoothTransformation);
    } else if (h > 0 && pix.height() > h) {
        pix = pix.scaledToHeight(h, Qt::SmoothTransformation);
    }

    QByteArray animationData;
    QByteArray animationFormat;
    getAnimatedImageData(data, &animationData, &animationFormat);

    return new ItemImage(pix, animationData, animationFormat, parent);
}

QStringList ItemImageLoader::formatsToSave() const
{
    return {
        QLatin1String("image/svg+xml"),
        QLatin1String("image/png"),
        QLatin1String("image/gif")
    };
}

QObject *ItemImageLoader::tests(const TestInterfacePtr &test) const
{
#ifdef HAS_TESTS
    QObject *tests = new ItemImageTests(test);
    return tests;
#else
    Q_UNUSED(test)
    return nullptr;
#endif
}

void ItemImageLoader::applySettings(QSettings &settings)
{
    settings.setValue(configMaxImageWidth, ui->spinBoxImageWidth->value());
    settings.setValue(configMaxImageHeight, ui->spinBoxImageHeight->value());
    settings.setValue(configImageEditor, ui->lineEditImageEditor->text());
    settings.setValue(configSvgEditor, ui->lineEditSvgEditor->text());
}

void ItemImageLoader::loadSettings(const QSettings &settings)
{
    m_maxImageWidth = settings.value(configMaxImageWidth, 320).toInt();
    m_maxImageHeight = settings.value(configMaxImageHeight, 240).toInt();
    m_imageEditor = settings.value(configImageEditor).toString();
    m_svgEditor = settings.value(configSvgEditor).toString();
}

QWidget *ItemImageLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemImageSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->spinBoxImageWidth->setValue(m_maxImageWidth);
    ui->spinBoxImageHeight->setValue(m_maxImageHeight);
    ui->lineEditImageEditor->setText(m_imageEditor);
    ui->lineEditSvgEditor->setText(m_svgEditor);
    return w;
}

QObject *ItemImageLoader::createExternalEditor(const QModelIndex &, const QVariantMap &data, QWidget *parent) const
{
    QString mime;
    QByteArray imageData;
    if ( !m_imageEditor.isEmpty() && getPrefferedImageData(data, &imageData, &mime) )
        return new ItemEditor(imageData, mime, m_imageEditor, parent);

    if ( !m_svgEditor.isEmpty() && getSvgData(data, &imageData, &mime) )
        return new ItemEditor(imageData, mime, m_svgEditor, parent);

    return nullptr;
}
