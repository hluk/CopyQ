/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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
#include <QMetaProperty>
#include <QPlainTextEdit>
#include <QUrl>
#include "client_server.h"
#include <assert.h>

static const QSize defaultSize(256, 256);

ItemDelegate::ItemDelegate(QWidget *parent) : QItemDelegate(parent),
    m_parent(parent), m_dryPaint(false)
{
    connect( this, SIGNAL(sizeUpdated(QModelIndex)),
             SIGNAL(sizeHintChanged(QModelIndex)), Qt::DirectConnection );
}

ItemDelegate::~ItemDelegate()
{
    for( int i = 0; i < m_cache.size(); ++i )
        removeCache(i);
}

void ItemDelegate::setStyleSheet(const QString &css)
{
    m_css = css;
    for( int i = 0; i < m_cache.size(); ++i )
        removeCache(i);
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const
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
    editor->setPalette( option.palette );
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

void ItemDelegate::rowsRemoved(const QModelIndex&,int start,int end)
{
    for( int i = start; i <= end; ++i ) {
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

QWidget *ItemDelegate::cache(const QModelIndex &index, QSize *size) const
{
    int n = index.row();

    QWidget *w = m_cache[n];
    if (!w) {
        QVariant displayData = index.data(Qt::DisplayRole);
        QString html = displayData.toString();
        QString text;
        QPixmap pix;

        bool hasHtml = !html.isEmpty();
        bool hasText = false;
        if (!hasHtml) {
            QVariant editData = index.data(Qt::EditRole);
            hasText = editData.canConvert(QVariant::String);
            if (hasText)
                text = editData.toString();
            else
                pix = displayData.value<QPixmap>();
        }

        if (hasText || hasHtml) {
            QTextEdit *textEdit = new QTextEdit();
            QTextDocument *doc = new QTextDocument(textEdit);
            doc->setDefaultStyleSheet(m_css);

            textEdit->setFrameShape(QFrame::NoFrame);
            textEdit->setWordWrapMode(QTextOption::NoWrap);
            textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            w = textEdit;
            w->setObjectName("item");
            w->setStyleSheet(m_css);

            // text or HTML
            if (hasText)
                doc->setPlainText(text);
            else
                doc->setHtml(html);
            textEdit->setDocument(doc);

            w->resize( doc->idealWidth(), doc->size().height() );
        } else {
            // Image
            QLabel *label = new QLabel();
            w = label;
            w->setObjectName("item");
            w->setStyleSheet(m_css);
            label->setMargin(4);
            label->setPixmap(pix);
            w->adjustSize();
        }
        m_cache[n] = w;
    }

    // maximum item size
    if (size)
        *size = w->size();

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
        log( QString("invalidating cache") );
        for( int i = 0; i < m_cache.length(); ++i ) {
            removeCache(i);
        }
    }
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    int row = index.row();
    QWidget *w = m_cache[row];
    if (w == NULL) {
        /* Size of currently painted item has changed. */
        w = cache(index);
        emit sizeUpdated(index);
        return;
    }

    if ( dryPaint() )
        return;

    const QRect &rect = option.rect;

    /* Render background (selected, alternate, ...). */
    QStyle *style = w->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter);
    QPalette::ColorRole role = option.state & QStyle::State_Selected ?
                QPalette::HighlightedText : QPalette::Text;
    style->drawItemText(painter, rect.translated(4, 4), 0,
                        option.palette, true, QString::number(row),
                        role);

    w->setMinimumSize(rect.size());

    /* Render text/image. */
    painter->save();
    painter->translate( rect.topLeft() );
    w->render(painter);
    painter->restore();
}
