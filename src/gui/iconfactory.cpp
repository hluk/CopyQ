/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include "gui/icons.h"
#include "gui/iconfont.h"

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

QPointer<QObject> activePaintDevice;

void replaceColor(QPixmap *pix, const QColor &from, const QColor &to)
{
    if (from == to)
        return;

    QPixmap pix2( pix->size() );
    pix2.fill(to);
    pix2.setMask( pix->createMaskFromColor(from, Qt::MaskOutColor) );

    QPainter p(pix);
    p.drawPixmap(0, 0, pix2);
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

QColor appIconColorHelper()
{
    const auto color = colorFromEnv("COPYQ_APP_COLOR");
    return color.isValid() ? QColor(color) : QColor(0x7f, 0xca, 0x9b);
}

QColor appIconColor()
{
    static const QColor color = appIconColorHelper();
    return color;
}

QColor sessionNameToColor(const QString &name)
{
    if (name.isEmpty())
        return appIconColor();

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
    return color.isValid() ? QColor(color) : sessionNameToColor( sessionName() );
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
    return pix;
}

QString imagePathFromPrefix(const QString &iconSuffix, const QString &resources)
{
#ifdef COPYQ_ICON_PREFIX
    const QString fileName(COPYQ_ICON_PREFIX + iconSuffix);
    if ( QFile::exists(fileName) )
        return fileName;
#else
    Q_UNUSED(iconSuffix)
#endif
    return imagesRecourcePath + resources;
}

void drawFontIcon(QPixmap *pix, ushort id, int w, int h, const QColor &color)
{
    id = fixIconId(id);

    QPainter painter(pix);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    const QFont font = iconFontFitSize(w, h);

    painter.setFont(font);

    // Center the icon to whole pixels so it stays sharp.
    const auto flags = Qt::AlignTop | Qt::AlignLeft;
    const auto iconText = QString(QChar(id));
    auto boundingRect = painter.boundingRect(0, 0, w, h, flags, iconText);
    const auto x = w - boundingRect.width();
    const auto y = h - boundingRect.height();
    // If icon is wider, assume that a tag will be rendered and align image to the right.
    const auto pos = boundingRect.bottomLeft()
            + ((w > h) ? QPoint(x, y / 2) : QPoint(x, y) / 2);

    // Draw shadow.
    painter.setPen(Qt::black);
    painter.setOpacity(0.2);
    painter.drawText(pos + QPoint(1, 1), iconText);
    painter.setOpacity(1);

    painter.setPen(color);
    painter.drawText(pos, iconText);
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

    const auto ratio = pix->devicePixelRatio();
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
        if (painter)
            size *= painter->paintEngine()->paintDevice()->devicePixelRatio();

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
                << QSize(16, 16)
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

class ImageIconEngine : public BaseIconEngine
{
public:
    ImageIconEngine(const QString &iconName, const QString &tag, QColor tagColor)
        : BaseIconEngine(tag, tagColor)
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
            return QPixmap();

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
            icon = QIcon::fromTheme(m_iconName);
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

        return QPixmap();
    }

private:
    QString m_iconName;
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
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);

        if (m_iconId == 0)
            return pixmap;

        drawFontIcon( &pixmap, m_iconId, size.width(), size.height(), colorForMode(painter, mode) );

        return pixmap;
    }

private:
    ushort m_iconId;
};

class AppIconEngine : public BaseIconEngine
{
public:
    AppIconEngine(AppIconType iconType)
        : BaseIconEngine(sessionIconTagVariable(), sessionIconTagColor())
        , m_iconType(iconType)
    {
    }

    QIconEngine *clone() const override
    {
        return new AppIconEngine(*this);
    }

    QPixmap doCreatePixmap(QSize size, QIcon::Mode, QIcon::State, QPainter *) override
    {
        const bool running = m_iconType == AppIconRunning;
        const QString suffix = running ? "-busy" : "-normal";
        const QString sessionName = ::sessionName();

        QIcon icon;

        if (sessionName.isEmpty())
            icon = QIcon::fromTheme(COPYQ_ICON_NAME + suffix);
        else
            icon = QIcon::fromTheme(COPYQ_ICON_NAME "_" + sessionName + "-" + suffix);

        QPixmap pix;

        const auto sessionColor = sessionIconColor();
        const auto appColor = appIconColor();
        const bool colorize = (sessionColor != appColor);

        // Colorize icon in higher resolution.
        const auto colorizeSize = 256;
        const auto sizeFactor = (colorize && size.height() < colorizeSize)
                ? (colorizeSize / size.height())
                : 1;
        size *= sizeFactor;

        if ( icon.isNull() ) {
            const auto path = imagePathFromPrefix(suffix + ".svg", running ? "icon-running" : "icon");
            pix = pixmapFromFile(path, size);
        } else {
            pix = icon.pixmap(size)
                    .scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        pix.setDevicePixelRatio(1);

        if (colorize)
            replaceColor(&pix, appColor, sessionColor);

        if (sizeFactor != 1) {
            size /= sizeFactor;
            pix = pix.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        return pix;
    }

    // WORKAROUND: Fixes ugly icon in Latte-Dock.
    QList<QSize> availableSizes(QIcon::Mode, QIcon::State) const override
    {
        static const auto sizes = QList<QSize>() << QSize(128, 128);
        return sizes;
    }

private:
    AppIconType m_iconType;
};

class IconEngine
{
public:
    static bool useSystemIcons;

    static QIcon createIcon(ushort iconId, const QString &iconName, const QString &tag = QString(), const QColor &tagColor = QColor())
    {
        if ( canUseFontIcon(iconId, iconName) )
            return QIcon( new FontIconEngine(iconId, tag, tagColor) );
        return QIcon( new ImageIconEngine(iconName, tag, tagColor) );
    }

    static QIcon createIcon(AppIconType iconType)
    {
        return QIcon( new AppIconEngine(iconType) );
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
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    if (loadIconFont())
        drawFontIcon(&pixmap, id, size, size, color);

    return pixmap;
}

QIcon appIcon(AppIconType iconType)
{
    return IconEngine::createIcon(iconType);
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
    sessionIconColorVariable() = color;
}

void setSessionIconTag(const QString &tag)
{
    sessionIconTagVariable() = tag;
}

void setSessionIconTagColor(QColor color)
{
    sessionIconTagColorVariable() = color;
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
