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

#include "itemtext.h"
#include "ui_itemtextsettings.h"

#include "common/mimetypes.h"
#include "common/sanitize_text_document.h"
#include "common/textdata.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtPlugin>

namespace {

// Limit number of characters for performance reasons.
const int maxCharacters = 100 * 1024;

// Limit line length for performance reasons.
const int maxLineLength = 1024;
const int maxLineLengthInPreview = 16 * maxLineLength;

// Limit line count for performance reasons.
const int maxLineCount = 4 * 1024;
const int maxLineCountInPreview = 16 * maxLineCount;

const char optionUseRichText[] = "use_rich_text";
const char optionMaximumLines[] = "max_lines";
const char optionMaximumHeight[] = "max_height";

const char mimeRichText[] = "text/richtext";

// Some applications insert \0 teminator at the end of text data.
// It needs to be removed because QTextBrowser can render the character.
void removeTrailingNull(QString *text)
{
    if ( text->endsWith(QChar(0)) )
        text->chop(1);
}

bool getRichText(const QVariantMap &dataMap, QString *text)
{
    if ( dataMap.contains(mimeHtml) ) {
        *text = getTextData(dataMap, mimeHtml);
        return true;
    }

    if ( dataMap.contains(mimeRichText) ) {
        *text = getTextData(dataMap, mimeRichText);
        return true;
    }

    return false;
}

bool getText(const QVariantMap &dataMap, QString *text)
{
    if ( dataMap.contains(mimeText) ) {
        *text = getTextData(dataMap, mimeText);
        return true;
    }

    if ( dataMap.contains(mimeUriList) ) {
        *text = getTextData(dataMap, mimeUriList);
        return true;
    }

    return false;
}

QString normalizeText(QString text)
{
    removeTrailingNull(&text);
    return text.left(maxCharacters);
}

void insertEllipsis(QTextCursor *tc)
{
    tc->insertHtml( " &nbsp;"
                    "<span style='background:rgba(0,0,0,30);border-radius:4px'>"
                    "&nbsp;&hellip;&nbsp;"
                    "</span>" );
}

} // namespace

ItemText::ItemText(const QString &text, const QString &richText, int maxLines, int lineLength, int maximumHeight, QWidget *parent)
    : QTextEdit(parent)
    , ItemWidget(this)
    , m_textDocument()
    , m_maximumHeight(maximumHeight)
{
    m_textDocument.setDefaultFont(font());

    // Disable slow word wrapping initially.
    QTextOption option = m_textDocument.defaultTextOption();
    option.setWrapMode(QTextOption::NoWrap);
    m_textDocument.setDefaultTextOption(option);

    setReadOnly(true);
    setUndoRedoEnabled(false);

    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);

    if ( !richText.isEmpty() ) {
        m_textDocument.setHtml(richText);
        // Use plain text instead if rendering HTML fails or result is empty.
        m_isRichText = !m_textDocument.isEmpty();
    }

    if (!m_isRichText)
        m_textDocument.setPlainText(text);

    if (maxLines > 0) {
        QTextBlock block = m_textDocument.findBlockByLineNumber(maxLines);
        if (block.isValid()) {
            QTextCursor tc(&m_textDocument);
            tc.setPosition(block.position() - 1);
            tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

            m_elidedFragment = tc.selection();
            tc.removeSelectedText();

            m_ellipsisPosition = tc.position();
            insertEllipsis(&tc);
        }
    }

    if (lineLength > 0) {
        for ( auto block = m_textDocument.begin(); block.isValid(); block = block.next() ) {
            if ( block.length() > lineLength ) {
                QTextCursor tc(&m_textDocument);
                tc.setPosition(block.position() + lineLength);
                tc.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
                insertEllipsis(&tc);
            }
        }
    }

    if (m_isRichText)
        sanitizeTextDocument(&m_textDocument);

    connect( this, &QTextEdit::selectionChanged,
             this, &ItemText::onSelectionChanged );
}

void ItemText::highlight(const QRegularExpression &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    QList<QTextEdit::ExtraSelection> selections;

    if ( re.isValid() && !re.pattern().isEmpty() ) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground( highlightPalette.base() );
        selection.format.setForeground( highlightPalette.text() );
        selection.format.setFont(highlightFont);

        QTextCursor cur = m_textDocument.find(re);
        int a = cur.position();
        while ( !cur.isNull() ) {
            if ( cur.hasSelection() ) {
                selection.cursor = cur;
                selections.append(selection);
            } else {
                cur.movePosition(QTextCursor::NextCharacter);
            }
            cur = m_textDocument.find(re, cur);
            int b = cur.position();
            if (a == b) {
                cur.movePosition(QTextCursor::NextCharacter);
                cur = m_textDocument.find(re, cur);
                b = cur.position();
                if (a == b) break;
            }
            a = b;
        }
    }

    setExtraSelections(selections);

    update();
}

void ItemText::updateSize(QSize maximumSize, int idealWidth)
{
    if ( m_textDocument.isEmpty() ) {
        setFixedSize(0, 0);
        return;
    }

    const int scrollBarWidth = verticalScrollBar()->width();
    setMaximumHeight( maximumSize.height() );
    setFixedWidth(idealWidth);
    m_textDocument.setTextWidth(idealWidth - scrollBarWidth);

    QTextOption option = m_textDocument.defaultTextOption();
    const QTextOption::WrapMode wrapMode = maximumSize.width() > idealWidth
            ? QTextOption::NoWrap : QTextOption::WrapAtWordBoundaryOrAnywhere;
    if (wrapMode != option.wrapMode()) {
        option.setWrapMode(wrapMode);
        m_textDocument.setDefaultTextOption(option);
    }

    // setDocument() is slow, so postpone this after resized properly
    if (document() != &m_textDocument)
        setDocument(&m_textDocument);

    const QSizeF size = m_textDocument.size();
    const int h = size.height() + 1;

    if (0 < m_maximumHeight && m_maximumHeight < h - 8)
        setFixedHeight(m_maximumHeight);
    else
        setFixedHeight(h);
}

bool ItemText::eventFilter(QObject *, QEvent *event)
{
    return ItemWidget::filterMouseEvents(this, event);
}

QMimeData *ItemText::createMimeDataFromSelection() const
{
    const auto data = QTextEdit::createMimeDataFromSelection();
    if (!data)
        return nullptr;

    // Copy only plain text if rich text is not available.
    if (!m_isRichText) {
        const auto text = data->text();
        data->clear();
        data->setText(text);
    }

    const auto owner = qApp->property("CopyQ_session_name").toString();
    data->setData( mimeOwner, owner.toUtf8() );

    return data;
}

void ItemText::onSelectionChanged()
{
    // Expand the ellipsis if selected.
    if ( m_ellipsisPosition == -1 || textCursor().selectionEnd() <= m_ellipsisPosition )
        return;

    QTextCursor tc(&m_textDocument);
    tc.setPosition(m_ellipsisPosition);
    m_ellipsisPosition = -1;
    tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    tc.insertFragment(m_elidedFragment);
    m_elidedFragment = QTextDocumentFragment();
}

ItemTextLoader::ItemTextLoader()
{
}

ItemTextLoader::~ItemTextLoader() = default;

ItemWidget *ItemTextLoader::create(const QVariantMap &data, QWidget *parent, bool preview) const
{
    if ( data.value(mimeHidden).toBool() )
        return nullptr;

    QString richText;
    const bool isRichText = m_settings.value(optionUseRichText, true).toBool()
            && getRichText(data, &richText);

    QString text;
    const bool isPlainText = getText(data, &text);

    if (!isRichText && !isPlainText)
        return nullptr;

    richText = normalizeText(richText);
    text = normalizeText(text);

    ItemText *item = nullptr;
    Qt::TextInteractionFlags interactionFlags(Qt::LinksAccessibleByMouse);
    // Always limit text size for performance reasons.
    if (preview) {
        item = new ItemText(text, richText, maxLineCountInPreview, maxLineLengthInPreview, 0, parent);
        item->setFocusPolicy(Qt::StrongFocus);
        interactionFlags = interactionFlags
                | Qt::TextSelectableByKeyboard
                | Qt::LinksAccessibleByKeyboard;
    } else {
        int maxLines = m_settings.value(optionMaximumLines, maxLineCount).toInt();
        if (maxLines <= 0 || maxLines > maxLineCount)
            maxLines = maxLineCount;
        const int maxHeight = m_settings.value(optionMaximumHeight, 0).toInt();
        item = new ItemText(text, richText, maxLines, maxLineLength, maxHeight, parent);
        item->viewport()->installEventFilter(item);
        item->setContextMenuPolicy(Qt::NoContextMenu);
    }
    item->setTextInteractionFlags(item->textInteractionFlags() | interactionFlags);

    return item;
}

QStringList ItemTextLoader::formatsToSave() const
{
    return m_settings.value(optionUseRichText, true).toBool()
            ? QStringList(mimeText) << mimeHtml << mimeRichText
            : QStringList(mimeText);
}

QVariantMap ItemTextLoader::applySettings()
{
    m_settings[optionUseRichText] = ui->checkBoxUseRichText->isChecked();
    m_settings[optionMaximumLines] = ui->spinBoxMaxLines->value();
    m_settings[optionMaximumHeight] = ui->spinBoxMaxHeight->value();
    return m_settings;
}

QWidget *ItemTextLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemTextSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->checkBoxUseRichText->setChecked( m_settings.value(optionUseRichText, true).toBool() );
    ui->spinBoxMaxLines->setValue( m_settings.value(optionMaximumLines, 0).toInt() );
    ui->spinBoxMaxHeight->setValue( m_settings.value(optionMaximumHeight, 0).toInt() );
    return w;
}
