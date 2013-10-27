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

#include "common/contenttype.h"

#include <QContextMenuEvent>
#include <QModelIndex>
#include <QMouseEvent>
#include <QTextCursor>
#include <QTextDocument>
#include <QtPlugin>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 100*1024;

bool getRichText(const QModelIndex &index, QString *text)
{
    if ( index.data(contentType::hasHtml).toBool() ) {
        *text = index.data(contentType::html).toString();
        return true;
    }

    const QVariantMap dataMap = index.data(contentType::data).toMap();
    if ( !dataMap.contains("text/richtext") )
        return false;

    const QByteArray data = dataMap["text/richtext"].toByteArray();
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
{
    m_textDocument.setDefaultFont(font());

    setReadOnly(true);
    setUndoRedoEnabled(false);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);

    // Selecting text copies it to clipboard.
    connect( this, SIGNAL(selectionChanged()), SLOT(onSelectionChanged()) );

    if (isRichText)
        m_textDocument.setHtml( text.left(defaultMaxBytes) );
    else
        m_textDocument.setPlainText( text.left(defaultMaxBytes) );
    setDocument(&m_textDocument);
}

void ItemText::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    QList<QTextEdit::ExtraSelection> selections;

    if ( !re.isEmpty() ) {
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

void ItemText::updateSize()
{
    const int w = maximumWidth();
    m_textDocument.setTextWidth(w);
    resize( m_textDocument.idealWidth() + 16, m_textDocument.size().height() );
}

void ItemText::mousePressEvent(QMouseEvent *e)
{
    setTextCursor( cursorForPosition(e->pos()) );
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

void ItemText::mouseReleaseEvent(QMouseEvent *e)
{
    if ( property("copyOnMouseUp").toBool() ) {
        setProperty("copyOnMouseUp", false);
        if ( textCursor().hasSelection() )
            copy();
    } else {
        QTextEdit::mouseReleaseEvent(e);
    }
}

void ItemText::onSelectionChanged()
{
    setProperty("copyOnMouseUp", true);
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
    QString text;
    bool isRichText = m_settings.value("use_rich_text", true).toBool()
            && getRichText(index, &text);

    if ( isRichText || getText(index, &text) )
        return new ItemText(text, isRichText, parent);

    return NULL;
}

QStringList ItemTextLoader::formatsToSave() const
{
    return m_settings.value("use_rich_text", true).toBool()
            ? QStringList("text/plain") << QString("text/html") << QString("text/richtext")
            : QStringList("text/plain");
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
