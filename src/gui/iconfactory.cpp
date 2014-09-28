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

#include "iconfactory.h"

#include "gui/icons.h"
#include "gui/iconfont.h"

#include <QBitmap>
#include <QCoreApplication>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QIconEngineV2>
#include <QPainter>
#include <QPixmap>
#include <QVariant>
#include <QWidget>

namespace {

const int iconSize = 16;
const char imagesRecourcePath[] = ":/images/";

/// Up to this value of background lightness, icon color will be lighter.
const int lightThreshold = 100;

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

    foreach (const QChar &c, name) {
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
    font.setPixelSize(h - 2);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText( QRect(1, 1, w - 1, h - 1), QString(QChar(id)) );
}

class IconEngine : public QIconEngineV2
{
public:
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state)
    {
        painter->drawPixmap( rect, pixmap(rect.size(), mode, state) );
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
    {
        if ( m_iconId == 0 || m_factory->useSystemIcons() ) {
            // Tint tab icons.
            if ( m_iconName.startsWith(imagesRecourcePath + QString("tab_")) ) {
                QPixmap pixmap(m_iconName);
                pixmap = pixmap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                return colorizedPixmap( pixmap, color(mode) );
            }

            QIcon icon = m_iconName.startsWith(':') ? QIcon(m_iconName) : QIcon::fromTheme(m_iconName);
            if ( !icon.isNull() )
                return icon.pixmap(size, mode, state);
        }

        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);

        if (m_iconId == 0 || !m_factory->isIconFontLoaded())
            return pixmap;

        drawFontIcon( &pixmap, m_iconId, size.width(), size.height(), color(mode) );

        return pixmap;
    }

    static QIcon createIcon(
            ushort iconId, const QString &iconName, IconFactory *factory,
            const QColor &color = QColor(), const QColor &colorActive = QColor())
    {
        return QIcon( new IconEngine(iconId, iconName, factory, color, colorActive) );
    }

private:
    IconEngine(
            ushort iconId, const QString &iconName, IconFactory *factory,
            const QColor &color, const QColor &colorActive)
        : m_iconId(iconId)
        , m_iconName(iconName)
        , m_color(color)
        , m_colorActive(colorActive)
        , m_factory(factory)
    {
    }

    QColor color(QIcon::Mode mode)
    {
        const bool selected = (mode == QIcon::Active || mode == QIcon::Selected);
        QColor color = selected
                ? (m_colorActive.isValid() ? m_colorActive : m_factory->iconColorActive())
                : (m_color.isValid() ? m_color : m_factory->iconColor());

        if (mode == QIcon::Disabled)
            color.setAlphaF(0.5);

        return color;
    }

    ushort m_iconId;
    QString m_iconName;
    QColor m_color;
    QColor m_colorActive;
    IconFactory *m_factory;
};

} // namespace

IconFactory::IconFactory()
    : m_iconColor(Qt::black)
    , m_iconColorActive(Qt::white)
    , m_useSystemIcons(true)
    , m_iconFontLoaded(false)
{
    m_iconFontLoaded = QFontDatabase::addApplicationFont(":/images/fontawesome-webfont.ttf") != -1;
}

QIcon IconFactory::getIcon(
        const QString &themeName, ushort id, const QColor &color, const QColor &activeColor)
{
    return IconEngine::createIcon(id, themeName, this, color, activeColor);
}

const QIcon IconFactory::getIcon(const QString &iconName)
{
    return IconEngine::createIcon(0, iconName, this);
}

QIcon IconFactory::getIcon(const QString &iconName, const QColor &color, const QColor &activeColor)
{
    return IconEngine::createIcon(0, imagesRecourcePath + iconName, this, color, activeColor);
}

QIcon IconFactory::iconFromFile(
        const QString &fileName, const QColor &color, const QColor &activeColor)
{
    if ( fileName.isEmpty() )
        return QIcon();

    ushort unicode = fileName.at(0).unicode();
    if (fileName.size() == 1 && unicode >= IconFirst && unicode <= IconLast)
        return IconEngine::createIcon(unicode, "", this, color, activeColor);

    QImage image(fileName);
    if (image.isNull())
        return QIcon();

    QPixmap pix = QPixmap::fromImage( image.scaledToHeight(iconSize) );
    return QIcon(pix);
}

void IconFactory::drawIcon(ushort id, const QRect &itemRect, QPainter *painter)
{
    if (isIconFontLoaded())
        return;

    QFont font = iconFont();
    int size = qMin(itemRect.height() - 5, 18);
    font.setPixelSize(size);

    painter->save();
    painter->setFont(font);
    painter->drawText( itemRect.right() - size, itemRect.top() + size, QString(QChar(id)) );
    painter->restore();
}

QPixmap IconFactory::createPixmap(ushort id, const QColor &color, int size)
{
    const int sz = (size > 0) ? size : iconSize;
    QPixmap pixmap(sz, sz);
    pixmap.fill(Qt::transparent);

    if (isIconFontLoaded())
        drawFontIcon(&pixmap, id, size, size, color);

    return pixmap;
}

void IconFactory::setDefaultColors(const QColor &color, const QColor &activeColor)
{
    m_iconColor = color;
    m_iconColorActive = activeColor;
}

QIcon IconFactory::appIcon(AppIconFlags flags)
{
    QPixmap pix = flags.testFlag(AppIconRunning)
            ? imageFromPrefix("-busy.svg", "icon-running")
            : imageFromPrefix("-normal.svg", "icon");

    const QString sessionName = qApp->property("CopyQ_session_name").toString();
    if (!sessionName.isEmpty()) {
        const QColor color1 = QColor(0x7f, 0xca, 0x9b);
        const QColor color2 = sessionNameToColor(sessionName);
        replaceColor(&pix, color1, color2);
    }

    if (flags.testFlag(AppIconDisabled)) {
        QPainter p(&pix);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        p.fillRect(pix.rect(), QColor(100, 100, 100, 100));
    }

    return QIcon(pix);
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

QColor getDefaultIconColor(const QWidget &widget, QPalette::ColorRole colorRole)
{
    return getDefaultIconColor( widget.palette().color(QPalette::Active, colorRole) );
}
