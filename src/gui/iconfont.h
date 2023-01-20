// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ICONFONT_H
#define ICONFONT_H

class QFont;
class QString;

const QString &iconFontFamily();

bool loadIconFont();

int iconFontSizePixels();

QFont iconFont();

QFont iconFontFitSize(int w, int h);

#endif // ICONFONT_H
