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

#include "iconfactory.h"

#include "common/appconfig.h"
#include "gui/icons.h"
#include "gui/iconfont.h"

#include <QBitmap>
#include <QCoreApplication>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QVariant>
#include <QWidget>

#if QT_VERSION < 0x050000
# include <QIconEngineV2>
using QtIconEngine = QIconEngineV2;
#else
# include <QIconEngine>
using QtIconEngine = QIconEngine;
#endif

namespace {

const char imagesRecourcePath[] = ":/images/";

/// Up to this value of background lightness, icon color will be lighter.
const int lightThreshold = 100;

QPointer<QObject> activePaintDevice;

QPixmap colorizedPixmap(const QPixmap &pix, const QColor &color)
{
    QLinearGradient gradient(pix.width() / 2, 0, 0, pix.height());
    bool dark = color.lightness() < lightThreshold;
    gradient.setColorAt(0.0, color.darker(dark ? 200 : 120));
    gradient.setColorAt(0.5, color);
    gradient.setColorAt(1.0, color.lighter(dark ? 150 : 120));

    QPixmap pix2(pix);
    QPainter painter(&pix2);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pix.rect(), gradient);

    return pix2;
}

void replaceColor(QPixmap *pix, const QColor &from, const QColor &to)
{
    QPixmap pix2( pix->size() );
    pix2.fill(to);
    pix2.setMask( pix->createMaskFromColor(from, Qt::MaskOutColor) );

    QPainter p(pix);
    p.drawPixmap(0, 0, pix2);
}

QColor sessionNameToColor(const QString &name)
{
    if (name.isEmpty())
        return QColor(Qt::white);

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

QPixmap imageFromPrefix(const QString &iconSuffix, const QString &resources)
{
#ifdef COPYQ_ICON_PREFIX
    const QString fileName(COPYQ_ICON_PREFIX + iconSuffix);
    if ( QFile::exists(fileName) )
        return QPixmap(fileName);
#else
    Q_UNUSED(iconSuffix)
#endif
    return QPixmap(imagesRecourcePath + resources);
}

void drawFontIcon(QPixmap *pix, ushort id, int w, int h, const QColor &color)
{
    QPainter painter(pix);
    QFont font = iconFont();
    const int margins = h / 8;
    font.setPixelSize(h - margins);
    painter.setFont(font);
    painter.setPen(color);

    // Center the icon to whole pixels so it stays sharp.
    const auto flags = Qt::AlignTop | Qt::AlignLeft;
    const auto iconText = QString(QChar(id));
    auto boundingRect = painter.boundingRect(0, 0, w, h, flags, iconText);
    const auto pos = QPoint(w - boundingRect.width(), h - boundingRect.height()) / 2;
    boundingRect.moveTopLeft(pos);

    painter.drawText(boundingRect, flags, iconText);
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

bool loadIconFont()
{
    static bool iconFontLoaded = QFontDatabase::addApplicationFont(":/images/fontawesome-webfont.ttf") != -1;
    return iconFontLoaded;
}

bool useSystemIcons()
{
    return !loadIconFont()
            || AppConfig(AppConfig::ThemeCategory).isOptionOn("use_system_icons");
}

class IconEngine : public QtIconEngine
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

    QtIconEngine *clone() const override
    {
        return new IconEngine(*this);
    }

    QPixmap createPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, QPainter *painter = nullptr)
    {
        if ( m_iconId == 0 || useSystemIcons()) {
            // Tint tab icons.
            if ( m_iconName.startsWith(imagesRecourcePath + QString("tab_")) ) {
                QPixmap pixmap(m_iconName);
                pixmap = pixmap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                return colorizedPixmap( pixmap, color(painter, mode) );
            }

            QIcon icon = m_iconName.startsWith(':') ? QIcon(m_iconName) : QIcon::fromTheme(m_iconName);
            if ( !icon.isNull() )
                return icon.pixmap(size, mode, state);
        }

        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);

        if (m_iconId == 0)
            return pixmap;

        drawFontIcon( &pixmap, m_iconId, size.width(), size.height(), color(painter, mode) );

        return pixmap;
    }

    // QIconEngine doesn't seem to work in menus on OS X.
#ifdef Q_OS_MAC
    void updateIcon(QIcon *icon, const QSize &size, QIcon::Mode mode)
    {
        icon->addPixmap( createPixmap(size, mode, QIcon::Off), mode, QIcon::Off );
    }

    void updateIcon(QIcon *icon, int extent)
    {
        const QSize size(extent, extent);
        updateIcon(icon, size, QIcon::Normal);
        updateIcon(icon, size, QIcon::Disabled);
        updateIcon(icon, size, QIcon::Active);
    }

    static QIcon createIcon(ushort iconId, const QString &iconName)
    {
        IconEngine iconEngine(iconId, iconName);
        QIcon icon;
        iconEngine.updateIcon(&icon, 16);
        iconEngine.updateIcon(&icon, 32);
        iconEngine.updateIcon(&icon, 64);
        iconEngine.updateIcon(&icon, 128);
        return icon;
    }
#else
    static QIcon createIcon(ushort iconId, const QString &iconName)
    {
        return QIcon( new IconEngine(iconId, iconName) );
    }
#endif

private:
    IconEngine(ushort iconId, const QString &iconName)
        : m_iconId(iconId)
        , m_iconName(iconName)
    {
    }

    QColor color(QPainter *painter, QIcon::Mode mode)
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

    ushort m_iconId;
    QString m_iconName;
};

void updateIcon(QIcon *icon, const QPixmap &pix, int extent)
{
    if (pix.height() > extent)
        icon->addPixmap( pix.scaledToHeight(extent, Qt::SmoothTransformation) );
}

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

QIcon iconFromFile(const QString &fileName)
{
    if ( fileName.isEmpty() )
        return QIcon();

    const auto unicode = toIconId(fileName);
    if (unicode != 0)
        return loadIconFont() ? IconEngine::createIcon(unicode, "") : QIcon();

    return QIcon(fileName);
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
    const bool running = iconType == AppIconRunning;
    const QString suffix = running ? "-busy" : "-normal";
    const QString sessionName = qApp->property("CopyQ_session_name").toString();

    QIcon icon;

    if (sessionName.isEmpty())
        icon = QIcon::fromTheme("copyq" + suffix);
    else
        icon = QIcon::fromTheme("copyq-" + sessionName + "-" + suffix);

    if (icon.isNull()) {
        const QString resourceSuffix = running ? "-running" : "";
        QPixmap pix = imageFromPrefix(suffix + ".svg", "icon" + resourceSuffix);

        if (!sessionName.isEmpty()) {
            const QColor color1(0x7f, 0xca, 0x9b);
            const QColor color2 = sessionNameToColor(sessionName);
            replaceColor(&pix, color1, color2);
        }

        icon.addPixmap(pix);

        // This makes the icon smoother on some systems.
        updateIcon(&icon, pix, 48);
        updateIcon(&icon, pix, 32);
        updateIcon(&icon, pix, 24);
        updateIcon(&icon, pix, 16);
    }

    return icon;
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

    const auto unicode = fileNameOrId.at(0).unicode();
    return unicode >= IconFirst ? unicode : 0;
}
