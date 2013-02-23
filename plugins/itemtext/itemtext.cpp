/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "contenttype.h"

#include <QContextMenuEvent>
#include <QModelIndex>
#include <QMouseEvent>
#include <QTextCursor>
#include <QTextDocument>
#include <QtPlugin>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 100*1024;

void init(QTextDocument &doc, const QFont &font)
{
    doc.setDefaultFont(font);
    doc.setUndoRedoEnabled(false);
}

bool getRichText(const QModelIndex &index, const QStringList &formats, QString *text)
{
    if ( index.data(contentType::hasHtml).toBool() ) {
        *text = index.data(contentType::html).toString();
        return true;
    }

    int i = formats.indexOf("text/richtext");
    if (i == -1)
        return false;

    const QByteArray data = index.data(contentType::firstFormat + i).toByteArray();
    *text = QString::fromUtf8(data);

    // Remove trailing null character.
    if ( text->endsWith(QChar(0)) )
        text->resize(text->size() - 1);

    return true;
}

bool getText(const QModelIndex &index, QString *text)
{
    if ( index.data(contentType::hasText).toBool() ) {
        *text = index.data(contentType::text).toString();
        return true;
    }
    return false;
}

} // namespace

ItemText::ItemText(const QString &text, bool isRichText, QWidget *parent)
    : QTextEdit(parent)
    , ItemWidget(this)
    , m_textDocument()
    , m_searchTextDocument()
    , m_textFormat(isRichText ? Qt::RichText : Qt::PlainText)
{
    init(m_textDocument, font());
    init(m_searchTextDocument, font());

    setUndoRedoEnabled(false);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);

    // Selecting text copies it to clipboard.
    connect( this, SIGNAL(selectionChanged()), SLOT(copy()) );

    if (isRichText)
        m_textDocument.setHtml( text.left(defaultMaxBytes) );
    else
        m_textDocument.setPlainText( text.left(defaultMaxBytes) );
    setDocument(&m_textDocument);
    updateSize();
    updateItem();
}

void ItemText::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_searchTextDocument.clear();
    if ( re.isEmpty() ) {
        setDocument(&m_textDocument);
    } else {
        bool plain = m_textFormat == Qt::PlainText;
        const QString &text = plain ? m_textDocument.toPlainText() : m_textDocument.toHtml();
        if (plain)
            m_searchTextDocument.setPlainText(text);
        else
            m_searchTextDocument.setHtml(text);

        QTextCursor cur = m_searchTextDocument.find(re);
        int a = cur.position();
        while ( !cur.isNull() ) {
            QTextCharFormat fmt = cur.charFormat();
            if ( cur.hasSelection() ) {
                fmt.setBackground( highlightPalette.base() );
                fmt.setForeground( highlightPalette.text() );
                fmt.setFont(highlightFont);
                cur.setCharFormat(fmt);
            } else {
                cur.movePosition(QTextCursor::NextCharacter);
            }
            cur = m_searchTextDocument.find(re, cur);
            int b = cur.position();
            if (a == b) {
                cur.movePosition(QTextCursor::NextCharacter);
                cur = m_searchTextDocument.find(re, cur);
                b = cur.position();
                if (a == b) break;
            }
            a = b;
        }
        setDocument(&m_searchTextDocument);
    }

    update();
}

void ItemText::updateSize()
{
    const int w = maximumWidth();
    m_searchTextDocument.setTextWidth(w);
    m_textDocument.setTextWidth(w);
    resize( m_textDocument.idealWidth() + 16, m_textDocument.size().height() );
}

void ItemText::mousePressEvent(QMouseEvent *e)
{
    QTextEdit::mousePressEvent(e);
    e->ignore();
}

void ItemText::mouseDoubleClickEvent(QMouseEvent *e)
{
    if ( e->modifiers().testFlag(Qt::ShiftModifier) )
        QTextEdit::mouseDoubleClickEvent(e);
    else
        e->ignore();
}

void ItemText::contextMenuEvent(QContextMenuEvent *e)
{
    e->ignore();
}

ItemTextLoader::ItemTextLoader()
    : ui(NULL)
{
}

ItemTextLoader::~ItemTextLoader()
{
    delete ui;
}

ItemWidget *ItemTextLoader::create(const QModelIndex &index, QWidget *parent) const
{
    const QStringList formats = index.data(contentType::formats).toStringList();

    QString text;
    bool isRichText = m_settings.value("use_rich_text", true).toBool()
            && getRichText(index, formats, &text);

    if ( isRichText || getText(index, &text) )
        return new ItemText(text, isRichText, parent);

    return NULL;
}

QStringList ItemTextLoader::formatsToSave() const
{
    return QStringList("text/plain") << QString("text/html") << QString("text/richtext");
}

QVariantMap ItemTextLoader::applySettings()
{
    Q_ASSERT(ui != NULL);
    m_settings["use_rich_text"] = ui->checkBoxUseRichText->isChecked();
    return m_settings;
}

QWidget *ItemTextLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemTextSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->checkBoxUseRichText->setChecked( m_settings.value("use_rich_text", true).toBool() );
    return w;
}

Q_EXPORT_PLUGIN2(itemtext, ItemTextLoader)
