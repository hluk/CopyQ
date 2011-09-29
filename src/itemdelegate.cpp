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
#include <QTextDocument>
#include "client_server.h"

ItemDelegate::ItemDelegate(QWidget *parent) : QStyledItemDelegate(parent),
    m_parent(parent)
{
    m_lbl.setObjectName("itemNumber");
}

ItemDelegate::~ItemDelegate()
{
    foreach(QTextDocument *doc, m_cache) {
        delete doc;
    }
}

void ItemDelegate::setStyleSheet(const QString &css)
{
    m_css = css;
    foreach(QTextDocument *doc, m_cache) {
        doc->setDefaultStyleSheet(css);
    }
    m_lbl.setStyleSheet(css);
    invalidateCache();
}

QSize ItemDelegate::sizeHint (const QStyleOptionViewItem &, const QModelIndex &index) const
{
    QSize size;

    int row = index.row();
    if ( row >= m_cache.size() ) {
        for( int i = m_cache.size(); i <= row; ++i )
            m_cache.insert(i, NULL);
    }

    getCache(index, &size);

    return size;
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

    // maximal editor size
    QRect w_rect = parent->rect();
    QRect o_rect = option.rect;
    QSize max_size( w_rect.width() - o_rect.left() - 10,
                   w_rect.height() - o_rect.top() - 10 );
    editor->setMaximumSize(max_size);
    editor->setMinimumSize(max_size);

    return editor;
}

void ItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QVariant v = index.data(Qt::EditRole);
    QByteArray n = editor->metaObject()->userProperty().name();
    if ( !n.isEmpty() ) {
        if (!v.isValid())
            v = QVariant(editor->property(n).userType(), (const void *)0);
        editor->setProperty(n, v);
        (qobject_cast<QPlainTextEdit*>(editor))->selectAll();
    }
}

void ItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QByteArray n = editor->metaObject()->userProperty().name();
    if (!n.isEmpty())
        model->setData(index, editor->property(n));
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

QTextDocument *ItemDelegate::getCache(const QModelIndex &index, QSize *size) const
{
    QTextDocument *doc;
    int n = index.row();

    doc = m_cache[n];
    if (!doc) {
        m_cache[n] = doc = new QTextDocument();
        doc->setDefaultStyleSheet(m_css);
        doc->setUndoRedoEnabled(false);

        // add images
        QVariantList lst = index.data(Qt::DisplayRole).toList();
        for(int i=1; i<lst.length(); ++i) {
            doc->addResource(QTextDocument::ImageResource,
                             QUrl("data://"+QString::number(i)), lst[i]);
        }

        // set html
        doc->setHtml( m_format.arg(lst[0].toString()) );
    }

    // maximum item size
    if (size) {
        size->setWidth( doc->idealWidth() );
        size->setHeight( doc->size().height() );
    }

    return doc;
}

void ItemDelegate::removeCache(const QModelIndex &index) const
{
    removeCache(index.row());
}

void ItemDelegate::removeCache(int row) const
{
    QTextDocument *doc = m_cache[row];
    if (doc) {
        delete doc;
        m_cache[row] = NULL;
    }
}

void ItemDelegate::invalidateCache() const
{
    log( QString("invalidating cache") );
    for( int i = 0; i < m_cache.length(); ++i ) {
        removeCache(i);
    }
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QTextDocument *doc;

    QStyleOptionViewItemV4 options(option);
    initStyleOption(&options, index);
    options.text = "";

    doc = getCache(index);

    // get focus rect and selection background
    const QWidget *widget = options.widget;
    QStyle *style = widget->style();

    QRectF clip( QPoint(0, 0), option.rect.size() );
    //QRectF text_rect;
    m_lbl.setText( QString::number(index.row()) );

    painter->save();
    style->drawControl(QStyle::CE_ItemViewItem, &options, painter, widget);
    painter->translate( option.rect.topLeft() );

    // draw number
    QSize size = m_lbl.minimumSizeHint();
    m_lbl.resize(size);
    m_lbl.render(painter);
    painter->translate( size.width(), 0 );

    doc->drawContents( painter, clip.adjusted(0, 0, -size.width(), 0) );
    painter->restore();
}
