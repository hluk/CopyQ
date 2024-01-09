// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <QSettings>
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

const QLatin1String optionUseRichText("use_rich_text");
const QLatin1String optionMaximumLines("max_lines");
const QLatin1String optionMaximumHeight("max_height");
const QLatin1String optionDefaultStyleSheet("default_style_sheet");

// Some applications insert \0 terminator at the end of text data.
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

ItemText::ItemText(
        const QString &text,
        const QString &richText,
        const QString &defaultStyleSheet,
        int maxLines,
        int lineLength,
        int maximumHeight,
        QWidget *parent)
    : QTextEdit(parent)
    , ItemWidget(this)
    , m_textDocument()
    , m_maximumHeight(maximumHeight)
{
    m_textDocument.setDefaultFont(font());

    // Disable slow word wrapping initially.
    setLineWrapMode(QTextEdit::NoWrap);
    QTextOption option = m_textDocument.defaultTextOption();
    option.setWrapMode(QTextOption::NoWrap);
    m_textDocument.setDefaultTextOption(option);
    m_textDocument.setDefaultStyleSheet(defaultStyleSheet);

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

    const bool noWrap = maximumSize.width() > idealWidth;
    QTextOption option = m_textDocument.defaultTextOption();
    const auto wrapMode = noWrap
            ? QTextOption::NoWrap : QTextOption::WrapAtWordBoundaryOrAnywhere;
    if (wrapMode != option.wrapMode()) {
        option.setWrapMode(wrapMode);
        m_textDocument.setDefaultTextOption(option);
    }
    setLineWrapMode(noWrap ? QTextEdit::NoWrap : QTextEdit::WidgetWidth);

    // setDocument() is slow, so postpone this after resized properly
    if (document() != &m_textDocument)
        setDocument(&m_textDocument);

    if (m_maximumHeight != -1) {
        const QSizeF size = m_textDocument.size();
        const int h = size.height() + 1;

        if (0 < m_maximumHeight && m_maximumHeight < h - 8)
            setFixedHeight(m_maximumHeight);
        else
            setFixedHeight(h);
    }
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
    const bool isRichText = m_useRichText && getRichText(data, &richText);

    QString text = getTextData(data);
    const bool isPlainText = !text.isEmpty();

    if (!isRichText && !isPlainText)
        return nullptr;

    richText = normalizeText(richText);
    text = normalizeText(text);

    ItemText *item = nullptr;
    Qt::TextInteractionFlags interactionFlags(Qt::LinksAccessibleByMouse);
    // Always limit text size for performance reasons.
    if (preview) {
        item = new ItemText(text, richText, m_defaultStyleSheet, maxLineCountInPreview, maxLineLengthInPreview, -1, parent);
        item->setFocusPolicy(Qt::StrongFocus);
        interactionFlags = interactionFlags
                | Qt::TextSelectableByKeyboard
                | Qt::LinksAccessibleByKeyboard;
    } else {
        int maxLines = m_maxLines;
        if (maxLines <= 0 || maxLines > maxLineCount)
            maxLines = maxLineCount;
        item = new ItemText(text, richText, m_defaultStyleSheet, maxLines, maxLineLength, m_maxHeight, parent);
        item->viewport()->installEventFilter(item);
        item->setContextMenuPolicy(Qt::NoContextMenu);
    }
    item->setTextInteractionFlags(item->textInteractionFlags() | interactionFlags);

    return item;
}

QStringList ItemTextLoader::formatsToSave() const
{
    return m_useRichText
            ? QStringList{mimeText, mimeTextUtf8, mimeHtml}
            : QStringList{mimeText, mimeTextUtf8};
}

void ItemTextLoader::applySettings(QSettings &settings)
{
    settings.setValue(optionUseRichText, ui->checkBoxUseRichText->isChecked());
    settings.setValue(optionMaximumLines, ui->spinBoxMaxLines->value());
    settings.setValue(optionMaximumHeight, ui->spinBoxMaxHeight->value());
    settings.setValue(optionDefaultStyleSheet, ui->textEditDefaultStyleSheet->toPlainText());
}

void ItemTextLoader::loadSettings(const QSettings &settings)
{
    m_useRichText = settings.value(optionUseRichText, true).toBool();
    m_maxLines = settings.value(optionMaximumLines, maxLineCount).toInt();
    m_maxHeight = settings.value(optionMaximumHeight).toInt();
    m_defaultStyleSheet = settings.value(optionDefaultStyleSheet).toString();
}

QWidget *ItemTextLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemTextSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->checkBoxUseRichText->setChecked(m_useRichText);
    ui->spinBoxMaxLines->setValue(m_maxLines);
    ui->spinBoxMaxHeight->setValue(m_maxHeight);
    ui->textEditDefaultStyleSheet->setPlainText(m_defaultStyleSheet);
    return w;
}
