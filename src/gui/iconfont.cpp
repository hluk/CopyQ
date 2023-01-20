// SPDX-License-Identifier: GPL-3.0-or-later

#include "iconfont.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QStyle>

#include <algorithm>
#include <vector>

namespace {

const int iconFontMaxHeight = 128;
const int iconFontMaxWidth = 160;

int iconFontId()
{
    static const auto fontId =
        QFontDatabase::addApplicationFont(":/images/fontawesome.ttf");
    return fontId;
}

std::vector<int> smoothSizes()
{
    const auto smoothSizes = QFontDatabase().smoothSizes(iconFontFamily(), QString());
    return std::vector<int>(std::begin(smoothSizes), std::end(smoothSizes));
}

int iconFontSmoothPixelSize(int pixelSize)
{
    static const auto smoothSizes = ::smoothSizes();

    const auto it = std::upper_bound(
        std::begin(smoothSizes), std::end(smoothSizes), pixelSize);

    if ( it == std::begin(smoothSizes) )
        return pixelSize;

    return *(it - 1);
}

QString createIconFontFamily()
{
    const QString iconFontFamily =
        QFontDatabase::applicationFontFamilies(iconFontId()).value(0);
    Q_ASSERT(iconFontFamily.endsWith("(CopyQ)"));
    return iconFontFamily;
}

} // namespace

const QString &iconFontFamily()
{
    static const QString fontFamily = createIconFontFamily();
    return fontFamily;
}

bool loadIconFont()
{
    return iconFontId() != -1;
}

QFont iconFont()
{
    static QFont font(iconFontFamily());
    font.setPixelSize( iconFontSizePixels() );
    return font;
}

int iconFontSizePixels()
{
    return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
}

QFont iconFontFitSize(int w, int h)
{
    QFont font = iconFont();
    const auto pixelSize = w < h
        ? w * iconFontMaxWidth / iconFontMaxHeight
        : h * iconFontMaxHeight / iconFontMaxWidth;
    const auto smoothPixelSize =  iconFontSmoothPixelSize(pixelSize);
    font.setPixelSize(smoothPixelSize);
    return font;
}
