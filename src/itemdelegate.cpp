/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

#include "itemdelegate.h"
#include <QApplication>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>
#include "client_server.h"
#include <assert.h>

static const QSize defaultSize(9999, 1024);

ItemDelegate::ItemDelegate(QWidget *parent)
    : QItemDelegate(parent)
    , m_parent(parent)
    , m_dryPaint(false)
    , m_showNumber(false)
    , m_re()
    , m_foundFont()
    , m_foundPalette()
    , m_editorFont()
    , m_editorPalette()
    , m_numberFont()
    , m_numberPalette()
    , m_cache()
{
    connect( this, SIGNAL(sizeUpdated(QModelIndex)),
             SIGNAL(sizeHintChanged(QModelIndex)), Qt::DirectConnection );
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &,
                             const QModelIndex &index) const
{
    int row = index.row();
    if( row >= m_cache.size() )
        return defaultSize;

    QWidget *w = m_cache[row];
    if (w)
        return w->size();
    else
        return defaultSize;
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    QWidget *editor = qobject_cast<QWidget*>(object);
    if ( event->type() == QEvent::KeyPress ) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
        switch ( keyevent->key() ) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
                if( keyevent->modifiers() == Qt::NoModifier )
                    return false;
            case Qt::Key_F2:
                emit commitData(editor);
                emit closeEditor(editor);
                return true;
            case Qt::Key_Escape:
                // don't commit data
                emit closeEditor(editor, QAbstractItemDelegate::RevertModelCache);
                return true;
            default:
                return false;
        }
    }

    return false;
}

QWidget *ItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    QPlainTextEdit *editor = new QPlainTextEdit(parent);
    editor->setPalette(m_editorPalette);
    editor->setFont(m_editorFont);
    editor->setObjectName("editor");

    // maximal editor size
    QRect w_rect = parent->rect();
    QRect o_rect = option.rect;
    QSize max_size( w_rect.width() - o_rect.left() - 10,
                   w_rect.height() - o_rect.top() - 10 );
    editor->setMaximumSize(max_size);
    editor->setMinimumSize(max_size);

    return editor;
}

void ItemDelegate::setEditorData(QWidget *editor,
                                 const QModelIndex &index) const
{
    QString text = index.data(Qt::EditRole).toString();
    QPlainTextEdit *textEdit = (qobject_cast<QPlainTextEdit*>(editor));
    textEdit->setPlainText(text);
    textEdit->selectAll();
}

void ItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                const QModelIndex &index) const
{
    QPlainTextEdit *textEdit = (qobject_cast<QPlainTextEdit*>(editor));
    model->setData(index, textEdit->toPlainText());
}

void ItemDelegate::dataChanged(const QModelIndex &a, const QModelIndex &b)
{
    // recalculating sizes of many items is expensive (when searching)
    // - assume that highlighted (matched) text has same size
    // - recalculate size only if item edited
    int row = a.row();
    if ( row == b.row() ) {
        removeCache(row);
        emit sizeHintChanged(a);
    }
}

void ItemDelegate::rowsRemoved(const QModelIndex &, int start, int end)
{
    for( int i = end; i >= start; --i ) {
        removeCache(i);
        m_cache.removeAt(i);
    }
}

void ItemDelegate::rowsMoved(const QModelIndex &, int sourceStart, int sourceEnd,
               const QModelIndex &, int destinationRow)
{
    int dest = sourceStart < destinationRow ? destinationRow-1 : destinationRow;
    for( int i = sourceStart; i <= sourceEnd; ++i ) {
        m_cache.move(i,dest);
        ++dest;
    }
}

void ItemDelegate::rowsInserted(const QModelIndex &, int start, int end)
{
    for( int i = start; i <= end; ++i )
        m_cache.insert( i, NULL );
}

QWidget *ItemDelegate::cache(const QModelIndex &index) const
{
    int n = index.row();

    QWidget *w = m_cache[n];
    if (w == NULL) {
        QVariant displayData = index.data(Qt::DisplayRole);
        QString text = displayData.toString();
        QPixmap pix;

        bool hasHtml = !text.isEmpty();
        if ( !hasHtml ) {
            QVariant editData = index.data(Qt::EditRole);
            text = editData.toString();
        }

        if ( !text.isEmpty() ) {
            QTextEdit *textEdit = new QTextEdit(m_parent);

            textEdit->setFrameShape(QFrame::NoFrame);
            textEdit->setWordWrapMode(QTextOption::NoWrap);
            textEdit->setUndoRedoEnabled(false);
            textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            w = textEdit;
            w->setProperty("textEdit", true);

            // text or HTML
            QTextDocument *doc = new QTextDocument(textEdit);
            if (hasHtml) {
                doc->setHtml(text);
            } else {
                doc->setPlainText(text);
                w->setPalette( m_parent->palette() );
            }

            textEdit->setDocument(doc);
            textEdit->setFont( m_parent->font() );
            textEdit->resize( defaultSize.width(),
                              doc->documentLayout()->documentSize().height() );
        } else {
            // Image
            QLabel *label = new QLabel(m_parent);
            pix = displayData.value<QPixmap>();
            w = label;
            label->setMargin(4);
            label->setPixmap(pix);
            w->adjustSize();
        }
        w->setStyleSheet("*{background:transparent}");
        w->hide();
        m_cache[n] = w;
    }

    return w;
}

void ItemDelegate::removeCache(const QModelIndex &index) const
{
    removeCache(index.row());
}

void ItemDelegate::removeCache(int row) const
{
    QWidget *w = m_cache[row];
    if (w) {
        w->deleteLater();
        m_cache[row] = NULL;
    }
}

void ItemDelegate::invalidateCache() const
{
    if ( m_cache.length() > 0 ) {
        for( int i = 0; i < m_cache.length(); ++i ) {
            removeCache(i);
        }
    }
}

void ItemDelegate::setSearch(const QRegExp &re)
{
    m_re = re;
}

void ItemDelegate::setSearchStyle(const QFont &font, const QPalette &palette)
{
    m_foundFont = font;
    m_foundPalette = palette;
}

void ItemDelegate::setEditorStyle(const QFont &font, const QPalette &palette)
{
    m_editorFont = font;
    m_editorPalette = palette;
}

void ItemDelegate::setNumberStyle(const QFont &font, const QPalette &palette)
{
    m_numberFont = font;
    m_numberPalette = palette;
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    int row = index.row();
    QWidget *w = m_cache[row];
    if (w == NULL) {
        /* Size of currently painted item has changed. */
        cache(index);
        emit sizeUpdated(index);
    }

    if ( w == NULL || dryPaint() )
        return;

    const QRect &rect = option.rect;

    /* render background (selected, alternate, ...) */
    QStyle *style = w->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter);
    QPalette::ColorRole role = option.state & QStyle::State_Selected ?
                QPalette::HighlightedText : QPalette::Text;

    /* render number */
    QRect numRect(0, 0, 0, 0);
    if (m_showNumber) {
        QString num = QString::number(row)+"  ";
        painter->save();
        painter->setFont(m_numberFont);
        style->drawItemText(painter, rect.translated(4, 4), 0,
                            m_numberPalette, true, num,
                            role);
        painter->restore();
        numRect = style->itemTextRect( QFontMetrics(m_numberFont), rect, 0,
                                       true, num );
    }

    /* highlight search string */
    QTextDocument *doc1 = NULL;
    QTextDocument *doc2 = NULL;
    QTextEdit *textEdit = NULL;
    if ( !m_re.isEmpty() ) {
        if ( w->property("textEdit").toBool() ) {
            textEdit = static_cast<QTextEdit *>(w);
            doc1 = textEdit->document();
            doc2 = doc1->clone(textEdit);
            textEdit->setDocument(doc2);

            int a, b;
            QTextCursor cur = doc2->find(m_re);
            a = cur.position();
            while ( !cur.isNull() ) {
                QTextCharFormat fmt = cur.charFormat();
                if ( cur.hasSelection() ) {
                    fmt.setBackground( m_foundPalette.base() );
                    fmt.setForeground( m_foundPalette.text() );
                    fmt.setFont(m_foundFont);
                    cur.setCharFormat(fmt);
                } else {
                    cur.movePosition(QTextCursor::NextCharacter);
                }
                cur = doc2->find(m_re, cur);
                b = cur.position();
                if (a == b) {
                    cur.movePosition(QTextCursor::NextCharacter);
                    cur = doc2->find(m_re, cur);
                    b = cur.position();
                    if (a == b) break;
                }
                a = b;
            }
        }
    }

    /* render widget */
    QPalette p1 = w->palette();
    QPalette p2 = m_parent->palette();
    if ( p1.color(QPalette::Text) != p2.color(role) ) {
        p1.setColor( QPalette::Text, p2.color(role) );
        w->setPalette(p1);
    }
    QRegion region(0, 0, rect.width()-numRect.width()-4, rect.height());
    painter->save();
    painter->translate( rect.topLeft() + QPoint(numRect.width(), 0) );
    w->render(painter, QPoint(), region);
    painter->restore();

    /* restore highlight */
    if (doc2 != NULL) {
        textEdit->setDocument(doc1);
        delete doc2;
    }
}
