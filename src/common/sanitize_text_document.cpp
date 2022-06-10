// SPDX-License-Identifier: GPL-3.0-or-later

#include "sanitize_text_document.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#ifdef Q_OS_MAC
#   include <QFontDatabase>
#   include <algorithm>
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
            const QStringList fontFamilies = charFormat.fontFamilies().toStringList();
            if ( !fontFamilies.isEmpty() &&
                 !std::any_of(fontFamilies.constBegin(), fontFamilies.constEnd(),
                     [&](const QString &fontFamily){ return fontDatabase.hasFamily(fontFamily); }) )
            {
                charFormat.setFontFamilies({});
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
