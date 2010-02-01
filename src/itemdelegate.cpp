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
#include <QLabel>
#include <QPainter>
#include <QDebug>
#include <QTextCursor>
#include <QMetaProperty>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QUrl>
#include "clipboardmodel.h"

ItemDelegate::ItemDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
    m_doc = new QTextDocument(this);
}

QSize ItemDelegate::sizeHint (const QStyleOptionViewItem &options, const QModelIndex &index) const
{
    if ( index.data(Qt::DisplayRole).type() == QVariant::Image ) {
        QImage image = index.data(Qt::DisplayRole).value<QImage>();
        m_doc->addResource(QTextDocument::ImageResource,
                            QUrl("clipboard://img.png"), QVariant(image));
        m_doc->setHtml( m_format.arg(index.row()).arg(
                            "<img src=\"clipboard://img.png\" />") );
    }
    else {
        QString str = index.data(Qt::DisplayRole).toString();
        m_doc->setTextWidth(options.rect.width());
        m_doc->setHtml( m_format.arg(QString("999")).arg(str) );
    }
    return QSize(m_doc->idealWidth(), m_doc->size().height());
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    QWidget *editor = qobject_cast<QWidget*>(object);
    if ( event->type() == QEvent::KeyPress ) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
        switch ( keyevent->key() ) {
            //case Qt::Key_Tab:
                //emit commitData(editor);
                //emit closeEditor(editor, QAbstractItemDelegate::EditNextItem);
                //return true;
            //case Qt::Key_Backtab:
                //emit commitData(editor);
                //emit closeEditor(editor, QAbstractItemDelegate::EditPreviousItem);
                //return true;
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

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 options(option);
    initStyleOption(&options, index);

    // background color
    QColor color;
    if( option.state & QStyle::State_Selected )
        color = option.palette.color(QPalette::HighlightedText);
    else
        color = option.palette.color(QPalette::Text);

    if ( index.data(Qt::DisplayRole).type() == QVariant::Image ) {
        QImage image = index.data(Qt::DisplayRole).value<QImage>();
        m_doc->addResource(QTextDocument::ImageResource,
                           QUrl("mydata://image.png"), QVariant(image));
        m_doc->setHtml( m_format.arg(index.row()).arg("<img src=\"mydata://image.png\" />") );
    }
    else
        m_doc->setHtml( m_format.arg(index.row()).arg(options.text) );

    // get focus rect and selection background
    QString text = options.text;
    options.text = "";
    const QWidget *widget = options.widget;
    QStyle *style = widget->style();

    QRect clip(0, 0, options.rect.width(), options.rect.height());

    painter->save();
    style->drawControl(QStyle::CE_ItemViewItem, &options, painter, widget);
    painter->translate(options.rect.left(), options.rect.top());
//    painter->setClipRect(clip);
    m_doc->drawContents(painter,clip);
    painter->restore();
}
