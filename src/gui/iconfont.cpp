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

int solidIconFontId()
{
    static const auto
            fontId = QFontDatabase::addApplicationFont(":/images/fontawesome-solid.ttf");
    return fontId;
}

int brandsIconFontId()
{
    static const auto fontId =
            QFontDatabase::addApplicationFont(":/images/fontawesome-brands.ttf");
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
    const auto iconFontFamilies = QStringList()
            << QFontDatabase::applicationFontFamilies(solidIconFontId()).value(0)
            << QFontDatabase::applicationFontFamilies(brandsIconFontId()).value(0);

    Q_ASSERT(iconFontFamilies[0].endsWith("(CopyQ)"));
    Q_ASSERT(iconFontFamilies[1].endsWith("(CopyQ)"));

    const QString iconFontFamily = "CopyQ Icon Font";
    QFont::insertSubstitutions(iconFontFamily, iconFontFamilies);
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
    return solidIconFontId() != -1 && brandsIconFontId() != -1;
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
