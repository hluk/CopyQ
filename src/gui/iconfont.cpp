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

#include "iconfont.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QStyle>

namespace {

const int iconFontMaxHeight = 128;

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

const QString &iconFontFamily()
{
    static const QString fontFamily =
            QFontDatabase::applicationFontFamilies(solidIconFontId()).value(0);
    return fontFamily;
}

int iconFontMaxWidth() {
    QFont font = iconFont();
    font.setPixelSize(iconFontMaxHeight);
    const auto maxWidth = QFontMetrics(font).maxWidth();
    return maxWidth;
}

} // namespace

bool loadIconFont()
{
    return solidIconFontId() != -1 && brandsIconFontId() != -1;
}

QFont iconFont()
{
    QFont font(iconFontFamily());
    font.setPixelSize( iconFontSizePixels() );
    return font;
}

int iconFontSizePixels()
{
    return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
}

QFont iconFontFitSize(int w, int h)
{
    static const auto iconFontMaxWidth = ::iconFontMaxWidth();
    QFont font = iconFont();
    if (w < h)
        font.setPixelSize(w * iconFontMaxWidth / iconFontMaxHeight);
    else
        font.setPixelSize(h * iconFontMaxHeight / iconFontMaxWidth);
    return font;
}
