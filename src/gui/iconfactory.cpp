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

#include "iconfactory.h"

#include "gui/fix_icon_id.h"
#include "gui/icons.h"
#include "gui/iconfont.h"
#include "gui/pixelratio.h"

#include <QBitmap>
#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QIcon>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPixmap>
#include <QPixmapCache>
#include <QPointer>
#include <QSvgRenderer>
#include <QVariant>
#include <QWidget>

#ifndef COPYQ_ICON_NAME
# define COPYQ_ICON_NAME "copyq"
#endif

namespace {

const char imagesRecourcePath[] = ":/images/";

/// Up to this value of background lightness, icon color will be lighter.
const int lightThreshold = 100;

bool sessionIconTagEnabledFlag = true;

QPointer<QObject> activePaintDevice;

QIcon fromTheme(const QString &name)
{
    if ( qEnvironmentVariableIsEmpty("COPYQ_DEFAULT_ICON") )
        return QIcon::fromTheme(name);

    return QIcon();
}

bool hasNormalIconHelper()
{
    // QIcon::hasThemeIcon() returns true even if icon "copyq-normal" is not available
    // but "copyq" is.
    const QString iconName = COPYQ_ICON_NAME "-normal";
    const QIcon normalIcon = fromTheme(COPYQ_ICON_NAME "-normal");
    if ( normalIcon.isNull() )
        return false;

    const QIcon defaultIcon = fromTheme(COPYQ_ICON_NAME);
    return defaultIcon.pixmap(16).toImage() != normalIcon.pixmap(16).toImage();
}

bool hasNormalIcon()
{
    static const bool result = hasNormalIconHelper();
    return result;
}

QString sessionName()
{
    return qApp->property("CopyQ_session_name").toString();
}

QString textFromEnv(const char *envVaribleName)
{
    const auto name = qgetenv(envVaribleName);
    return QString::fromUtf8(name);
}

QColor colorFromEnv(const char *envVaribleName)
{
    return QColor( textFromEnv(envVaribleName) );
}

QColor sessionNameToColor(const QString &name)
{
    if (name.isEmpty())
        return QColor();

    int r = 0;
    int g = 0;
    int b = 0;

    for (const auto &c : name) {
        const ushort x = c.unicode() % 3;
        if (x == 0)
            r += 255;
        else if (x == 1)
            g += 255;
        else
            b += 255;
    }

    int max = qMax(r, qMax(g, b));
    r = r * 255 / max;
    g = g * 255 / max;
    b = b * 255 / max;

    return QColor(r, g, b);
}

QColor sessionIconColorHelper()
{
    const auto color = colorFromEnv("COPYQ_SESSION_COLOR");
    return color.isValid() ? color : sessionNameToColor( sessionName() );
}

QColor &sessionIconColorVariable()
{
    static QColor color = sessionIconColorHelper();
    return color;
}

QString &sessionIconTagVariable()
{
    static QString tag = textFromEnv("COPYQ_SESSION_TAG");
    return tag;
}

QColor &sessionIconTagColorVariable()
{
    static QColor color = colorFromEnv("COPYQ_SESSION_TAG_COLOR");
    return color;
}

QPixmap pixmapFromBitmapFile(const QString &path, QSize size)
{
    return QPixmap(path)
            .scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap pixmapFromFile(const QString &path, QSize size)
{
    const auto cacheKey = QString("path:%1|%2x%3")
            .arg(path)
            .arg(size.width())
            .arg(size.height());

    {
        QPixmap pixmap;
        if ( QPixmapCache::find(cacheKey, &pixmap) )
            return pixmap;
    }

    if ( !QFile::exists(path) )
        return QPixmap();

    if ( !path.endsWith(".svg", Qt::CaseInsensitive) )
        return pixmapFromBitmapFile(path, size);

    QSvgRenderer renderer(path);
    if ( !renderer.isValid() )
        return pixmapFromBitmapFile(path, size);

    QPixmap pix(size);
    pix.setDevicePixelRatio(1);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    renderer.render(&painter, pix.rect());

    QPixmapCache::insert(cacheKey, pix);

    return pix;
}

QString iconPath(const QString &iconSuffix)
{
#ifdef COPYQ_ICON_PREFIX
    const QString fileName(COPYQ_ICON_PREFIX + iconSuffix + ".svg");
    if ( QFile::exists(fileName) )
        return fileName;
#else
    Q_UNUSED(iconSuffix)
#endif
    return imagesRecourcePath + QLatin1String("icon") + iconSuffix;
}

QPixmap appPixmap(const QString &iconSuffix, QSize size)
{
    if ( iconSuffix.isEmpty() && hasNormalIcon() )
        return appPixmap("-normal", size);

    const auto icon = fromTheme(COPYQ_ICON_NAME + iconSuffix);

    QPixmap pix;

    if ( icon.isNull() ) {
        const auto path = iconPath(iconSuffix);
        pix = pixmapFromFile(path, size);
    } else {
        pix = icon.pixmap(size)
                .scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    pix.setDevicePixelRatio(1);

    return pix;
}

void replaceColor(QPixmap *pix, const QColor &targetColor)
{
    auto pix2 = appPixmap("_mask", pix->size());

    {
        QPainter p1(&pix2);
        p1.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        p1.fillRect(pix->rect(), targetColor);
    }

    QPainter p(pix);
    p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    p.drawPixmap(0, 0, pix2);
}

void disableIcon(QPixmap *pix)
{
    QPixmap pix2(pix->size());
    pix2.fill(Qt::transparent);
    {
        QPainter p(&pix2);
        p.setOpacity(0.7);
        p.drawPixmap(0, 0, *pix);
    }
    *pix = pix2;
}

QPixmap drawFontIcon(ushort id, int w, int h, const QColor &color)
{
    const auto cacheKey = QString("id:%1|%2x%3|%4")
            .arg(id)
            .arg(w)
            .arg(h)
            .arg(color.rgba());

    {
        QPixmap pixmap;
        if ( QPixmapCache::find(cacheKey, &pixmap) )
            return pixmap;
    }

    QPixmap pixmap(w, h);
    pixmap.fill(Qt::transparent);

    id = fixIconId(id);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    const QFont font = iconFontFitSize(w, h);

    painter.setFont(font);

    // Center the icon to whole pixels so it stays sharp.
    const auto flags = Qt::AlignTop | Qt::AlignLeft;
    const auto iconText = QString(QChar(id));
    auto boundingRect = painter.boundingRect(0, 0, w, h, flags, iconText);
    const auto x = w - boundingRect.width();
    // If icon is wider, assume that a tag will be rendered and align image to the right.
    const auto pos = boundingRect.bottomLeft()
            + ((w > h) ? QPoint(x * 3 / 4, 0) : QPoint(x / 2, 0));

    // Draw shadow.
    painter.setPen(Qt::black);
    painter.setOpacity(0.2);
    painter.drawText(pos + QPoint(1, 1), iconText);
    painter.setOpacity(1);

    painter.setPen(color);
    painter.drawText(pos, iconText);

    QPixmapCache::insert(cacheKey, pixmap);

    return pixmap;
}

QColor getDefaultIconColor(const QColor &color)
{
    QColor c = color;
    bool menuBackgrounIsLight = c.lightness() > lightThreshold;
    c.setHsl(c.hue(),
             qMax(0, qMin(255, c.saturation() + (menuBackgrounIsLight ? 30 : 10))),
             qMax(0, qMin(255, c.lightness() + (menuBackgrounIsLight ? -140 : 100))));

    return c;
}

void tagIcon(QPixmap *pix, const QString &tag, QColor color)
{
    if ( tag.isEmpty() )
        return;

    const auto ratio = pixelRatio(pix);
    pix->setDevicePixelRatio(1);

    QPainter painter(pix);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);

    const int h = pix->height();
    const int strokeWidth = static_cast<int>(ratio + h / 16);

    QFont font;
    const auto pixelSize = pix->width() / 2;
    if ( tag.size() == 1 ) {
        font = iconFontFitSize(pixelSize, pixelSize);
    } else {
        font.setPixelSize(pixelSize);
        font.setBold(true);
    }
    painter.setFont(font);

    const auto rect = painter.fontMetrics().tightBoundingRect(tag);
    const auto baseLineY = rect.bottom();
    const auto pos = QPoint(strokeWidth, h - strokeWidth - baseLineY);

    QPainterPath path;
    path.addText(pos, font, tag);
    const auto strokeColor = color.lightness() < 100 ? Qt::white : Qt::black;
    painter.strokePath(path, QPen(strokeColor, strokeWidth));

    painter.setPen(color);
    painter.drawText(pos, tag);
}

QColor colorForMode(QPainter *painter, QIcon::Mode mode)
{
    auto parent = painter
            ? dynamic_cast<QWidget*>(painter->device())
            : qobject_cast<QWidget*>(activePaintDevice.data());

    const bool selected = (mode == QIcon::Active || mode == QIcon::Selected);
    QColor color = parent ? getDefaultIconColor(*parent, selected) : Qt::darkGray;

    if (mode == QIcon::Disabled)
        color.setAlphaF(0.5);

    return color;
}

class BaseIconEngine : public QIconEngine
{
public:
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        painter->drawPixmap( rect, createPixmap(rect.size(), mode, state, painter) );
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return createPixmap(size, mode, state);
    }

    QPixmap createPixmap(QSize size, QIcon::Mode mode, QIcon::State state, QPainter *painter = nullptr)
    {
        // WORKAROUND: Big icons can cause application to crash,
        //             so limit icon size to 1024x1024.
        if ( size.width() > 1024 || size.height() > 1024 )
            size.scale(1024, 1024, Qt::KeepAspectRatio);

        if (painter)
            size *= pixelRatio(painter->paintEngine()->paintDevice());

        auto pixmap = doCreatePixmap(size, mode, state, painter);

        if ( pixmap.isNull() ) {
            pixmap = QPixmap(size);
            pixmap.fill(Qt::transparent);
        }

        return taggedIcon(&pixmap);
    }

    QList<QSize> availableSizes(QIcon::Mode, QIcon::State) const override
    {
        static const auto sizes = QList<QSize>()
                << QSize(32, 32)
                << QSize(48, 48)
                << QSize(64, 64)
                << QSize(96, 96)
                << QSize(128, 128);
        return sizes;
    }

protected:
    BaseIconEngine(const QString &tag, QColor tagColor)
        : m_tag(tag)
        , m_tagColor(tagColor)
    {
    }

private:
    virtual QPixmap doCreatePixmap(QSize size, QIcon::Mode mode, QIcon::State state, QPainter *painter) = 0;

    QPixmap taggedIcon(QPixmap *pix)
    {
        tagIcon(pix, m_tag, m_tagColor);
        return *pix;
    }

    QString m_tag;
    QColor m_tagColor;
};

class FontIconEngine : public BaseIconEngine
{
public:
    FontIconEngine(ushort iconId, const QString &tag, QColor tagColor)
        : BaseIconEngine(tag, tagColor)
        , m_iconId(iconId)
    {
    }

    QIconEngine *clone() const override
    {
        return new FontIconEngine(*this);
    }

    QPixmap doCreatePixmap(QSize size, QIcon::Mode mode, QIcon::State, QPainter *painter) override
    {
        if (m_iconId == 0) {
            QPixmap pixmap(size);
            pixmap.fill(Qt::transparent);
            return pixmap;
        }

        return drawFontIcon( m_iconId, size.width(), size.height(), colorForMode(painter, mode) );
    }

private:
    ushort m_iconId;
};

class ImageIconEngine final : public FontIconEngine
{
public:
    ImageIconEngine(const QString &iconName, ushort fallbackIconId, const QString &tag, QColor tagColor)
        : FontIconEngine(fallbackIconId, tag, tagColor)
        , m_iconName(iconName)
    {
    }

    QIconEngine *clone() const override
    {
        return new ImageIconEngine(*this);
    }

    QPixmap doCreatePixmap(QSize size, QIcon::Mode mode, QIcon::State state, QPainter *painter) override
    {
        if ( m_iconName.isEmpty() )
            return FontIconEngine::doCreatePixmap(size, mode, state, painter);

        // Tint tab icons.
        if ( m_iconName.startsWith(imagesRecourcePath + QString("tab_")) ) {
            const QPixmap pixmap = pixmapFromFile(m_iconName, size);

            QPixmap pixmap2(pixmap.size());
            pixmap2.fill(Qt::transparent);
            QPainter painter2(&pixmap2);
            painter2.setRenderHint(QPainter::SmoothPixmapTransform);
            painter2.setRenderHint(QPainter::TextAntialiasing);
            painter2.setRenderHint(QPainter::Antialiasing);

            // Draw shadow.
            const auto rect = pixmap.rect();
            painter2.setOpacity(0.2);
            painter2.drawPixmap(rect.translated(1, 1), pixmap);
            painter2.setOpacity(1);
            painter2.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter2.fillRect( pixmap2.rect(), Qt::black );
            painter2.setCompositionMode(QPainter::CompositionMode_SourceOver);

            painter2.drawPixmap(rect, pixmap);
            painter2.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter2.fillRect( pixmap2.rect(), colorForMode(painter, mode) );

            return pixmap2;
        }

        QIcon icon = pixmapFromFile(m_iconName, size);
        if ( icon.isNull() )
            icon = fromTheme(m_iconName);
        if ( !icon.isNull() ) {
            auto pixmap = icon.pixmap(size, mode, state);
            if (pixmap.size() != size) {
                pixmap = pixmap.scaled(size.width(), size.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QPixmap pixmap2(size);
                pixmap2.fill(Qt::transparent);
                QPainter painter2(&pixmap2);
                const int x = size.width() - pixmap.width();
                const QRect rect( QPoint(x, 0), pixmap.size() );
                painter2.drawPixmap(rect, pixmap);
                pixmap = pixmap2;
            }
            return pixmap;
        }

        return FontIconEngine::doCreatePixmap(size, mode, state, painter);
    }

private:
    QString m_iconName;
};

class AppIconEngine final : public BaseIconEngine
{
public:
    AppIconEngine()
        : BaseIconEngine(sessionIconTagVariable(), sessionIconTagColor())
    {
    }

    QIconEngine *clone() const override
    {
        return new AppIconEngine(*this);
    }

    QPixmap doCreatePixmap(QSize size, QIcon::Mode, QIcon::State, QPainter *) override
    {
        // If copyq-normal icon exist in theme, omit changing color.
        const bool useColoredIcon = !hasNormalIcon();
        const auto sessionColor = useColoredIcon ? sessionIconColor() : QColor();

        const auto cacheKey = QString("app:%1|%2x%3|%4")
                .arg(sessionColor.name())
                .arg(size.width())
                .arg(size.height())
                .arg(sessionIconTagEnabledFlag);

        {
            QPixmap pixmap;
            if ( QPixmapCache::find(cacheKey, &pixmap) )
                return pixmap;
        }

        auto pix = appPixmap(QString(), size);

        if ( sessionColor.isValid() )
            replaceColor(&pix, sessionColor);

        if (!sessionIconTagEnabledFlag)
            disableIcon(&pix);

        QPixmapCache::insert(cacheKey, pix);

        return pix;
    }
};

class IconEngine final
{
public:
    static bool useSystemIcons;

    static QIcon createIcon(ushort iconId, const QString &iconName, const QString &tag = QString(), const QColor &tagColor = QColor())
    {
        if ( canUseFontIcon(iconId, iconName) )
            return QIcon( new FontIconEngine(iconId, tag, tagColor) );
        return QIcon( new ImageIconEngine(iconName, iconId, tag, tagColor) );
    }

    static QIcon createIcon()
    {
        return QIcon( new AppIconEngine() );
    }

private:
    static bool canUseFontIcon(ushort iconId, const QString &iconName)
    {
        if ( iconId == 0 || !loadIconFont() )
            return false;

        if ( useSystemIcons && !iconName.isEmpty() )
            return false;

        return true;
    }
};

bool IconEngine::useSystemIcons = false;

} // namespace

QIcon getIcon(const QString &themeName, unsigned short id)
{
    return loadIconFont() || !themeName.isEmpty()
            ? IconEngine::createIcon(loadIconFont() ? id : 0, themeName)
            : QIcon();
}

QIcon getIcon(const QVariant &iconOrIconId)
{
    if (iconOrIconId.canConvert(QVariant::UInt))
        return getIcon( QString(), iconOrIconId.value<ushort>() );

    if (iconOrIconId.canConvert(QVariant::Icon))
        return iconOrIconId.value<QIcon>();

    return QIcon();
}

QIcon getIconFromResources(const QString &iconName)
{
    return IconEngine::createIcon(0, imagesRecourcePath + iconName);
}

QIcon iconFromFile(const QString &fileName, const QString &tag, const QColor &color)
{
    if ( fileName.isEmpty() && tag.isEmpty() )
        return QIcon();

    const auto unicode = toIconId(fileName);
    if (unicode != 0)
        return loadIconFont() ? IconEngine::createIcon(unicode, "", tag, color) : QIcon();

    return IconEngine::createIcon(0, fileName, tag, color);
}

QPixmap createPixmap(unsigned short id, const QColor &color, int size)
{
    if (loadIconFont())
        return drawFontIcon(id, size, size, color);

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

QIcon appIcon()
{
    return IconEngine::createIcon();
}

void setActivePaintDevice(QObject *device)
{
    activePaintDevice = device;
}

QColor getDefaultIconColor(const QWidget &widget, bool selected)
{
    const QWidget *parent = &widget;
    while ( parent->parentWidget()
            && !parent->isTopLevel()
            && !parent->testAttribute(Qt::WA_OpaquePaintEvent) )
    {
        parent = parent->parentWidget();
    }

    QPalette::ColorRole role = selected ? QPalette::Highlight : parent->backgroundRole();
    return getDefaultIconColor( parent->palette().color(QPalette::Active, role) );
}

unsigned short toIconId(const QString &fileNameOrId)
{
    if ( fileNameOrId.size() != 1 )
        return 0;

    return fileNameOrId.at(0).unicode();
}

void setSessionIconColor(QColor color)
{
    sessionIconColorVariable() = color.isValid() ? color : sessionIconColorHelper();
}

void setSessionIconTag(const QString &tag)
{
    sessionIconTagVariable() = tag;
}

void setSessionIconTagColor(QColor color)
{
    sessionIconTagColorVariable() = color;
}

void setSessionIconEnabled(bool enabled)
{
    sessionIconTagEnabledFlag = enabled;
}

QColor sessionIconColor()
{
    return ::sessionIconColorVariable();
}

QString sessionIconTag()
{
    return ::sessionIconTagVariable();
}

QColor sessionIconTagColor()
{
    return ::sessionIconTagColorVariable();
}

void setUseSystemIcons(bool useSystemIcons)
{
    IconEngine::useSystemIcons = useSystemIcons;
}
