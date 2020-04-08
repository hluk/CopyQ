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

#include "sanitize_text_document.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#ifdef Q_OS_MAC
#   include <QFontDatabase>
#endif

namespace {

constexpr int maxFontPointSize = 128;
constexpr int maxFontPixelSize = maxFontPointSize * 4 / 3;

/// Sanitize font sizes to prevent app freeze.
bool sanitizeFont(QFont *font)
{
    const int pixelSize = font->pixelSize();
    const int pointSize = font->pointSize();

    if (-maxFontPixelSize > pixelSize || pixelSize > maxFontPixelSize) {
        font->setPixelSize(maxFontPixelSize);
        return true;
    }

    if (-maxFontPointSize > pointSize || pointSize > maxFontPointSize) {
        font->setPointSize(maxFontPointSize);
        return true;
    }

    return false;
}

} // namespace

void sanitizeTextDocument(QTextDocument *document)
{
    QTextCursor tc(document);

#ifdef Q_OS_MAC
    QFontDatabase fontDatabase;
#endif

    for (auto block = document->begin(); block != document->end(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            QTextCharFormat charFormat = fragment.charFormat();
            bool sanitized = false;

#ifdef Q_OS_MAC
            // Prevent showing font download dialog on macOS.
            const QString fontFamily = charFormat.fontFamily();
            if ( !fontDatabase.hasFamily(fontFamily) ) {
                charFormat.setFontFamily(QString());
                sanitized = true;
            }
#endif

            QFont font = charFormat.font();
            if ( sanitizeFont(&font) ) {
                charFormat.setFont(font);
                sanitized = true;
            }

            if (sanitized) {
                tc.setPosition( fragment.position() );
                tc.setPosition( fragment.position() + fragment.length(), QTextCursor::KeepAnchor );
                tc.setCharFormat(charFormat);
            }
        }
    }
}
