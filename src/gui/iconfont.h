// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


class QFont;
class QString;

const QString &iconFontFamily();

bool loadIconFont();

int iconFontSizePixels();

QFont iconFont();

QFont iconFontFitSize(int w, int h);
