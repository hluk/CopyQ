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

#include "itemdelegate.h"
#include "itemimage.h"
#include "itemtext.h"
#ifdef HAS_WEBKIT
#   include "itemweb.h"
#endif
#include "itemwidget.h"
#include "client_server.h"
#include "configurationmanager.h"

#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>

const QSize defaultSize(0, 512);

ItemDelegate::ItemDelegate(QWidget *parent)
    : QItemDelegate(parent)
    , m_parent(parent)
    , m_showNumber(false)
    , m_re()
    , m_maxSize(maximumItemSize)
    , m_foundFont()
    , m_foundPalette()
    , m_editorFont()
    , m_editorPalette()
    , m_numberFont()
    , m_numberPalette()
    , m_cache()
#ifdef HAS_WEBKIT
    , m_useWeb(true)
#endif
{
}

ItemDelegate::~ItemDelegate()
{
    invalidateCache();
}

QSize ItemDelegate::sizeHint(const QModelIndex &index) const
{
    const ItemWidget *w = m_cache.value(index.row(), NULL);
    return (w != NULL) ? w->size() : defaultSize;
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &,
                             const QModelIndex &index) const
{
    return sizeHint(index);
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(object);
    if (object == NULL)
        return false;

    QEvent::Type type = event->type();
    if ( type == QEvent::KeyPress ) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
        switch ( keyevent->key() ) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
                // Commit data on Ctrl+Return or Enter?
                if (ConfigurationManager::instance()->value("edit_ctrl_return").toBool()) {
                    if (keyevent->modifiers() != Qt::ControlModifier)
                        return false;
                } else {
                    if (keyevent->modifiers() == Qt::ControlModifier) {
                        editor->insertPlainText("\n");
                        return true;
                    } else if (keyevent->modifiers() != Qt::NoModifier) {
                        return false;
                    }
                }
                emit commitData(editor);
                emit closeEditor(editor);
                return true;
            case Qt::Key_S:
                // Commit data on Ctrl+S.
                if (keyevent->modifiers() != Qt::ControlModifier)
                    return false;
                emit commitData(editor);
                emit closeEditor(editor);
                return true;
            case Qt::Key_F2:
                // Commit data on F2.
                emit commitData(editor);
                emit closeEditor(editor);
                return true;
            case Qt::Key_Escape:
                // Close editor without committing data.
                emit closeEditor(editor, QAbstractItemDelegate::RevertModelCache);
                return true;
            default:
                return false;
        }
    } else if ( type == QEvent::ContextMenu ) {
        QAction *act;
        QMenu *menu = editor->createStandardContextMenu();
        connect( menu, SIGNAL(aboutToHide()), menu, SLOT(deleteLater()) );
        menu->setParent(editor);

        act = menu->addAction( tr("&Save Item") );
        act->setShortcut( QKeySequence(tr("F2, Ctrl+Enter")) );
        connect( act, SIGNAL(triggered()), this, SLOT(editorSave()) );

        act = menu->addAction( tr("Cancel Editing") );
        act->setShortcut( QKeySequence(tr("Escape")) );
        connect( act, SIGNAL(triggered()), this, SLOT(editorCancel()) );

        QContextMenuEvent *menuEvent = static_cast<QContextMenuEvent *>(event);
        menu->popup( menuEvent->globalPos() );
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
    QRect w_rect = parent->contentsRect();
    QRect o_rect = option.rect;
    QSize max_size( w_rect.width() - o_rect.left() - 4,
                    w_rect.height() - o_rect.top() - 4 );
    editor->setMaximumSize(max_size);
    editor->setMinimumSize(max_size);

    connect( editor, SIGNAL(destroyed()),
             this, SLOT(editingStops()) );
    connect( editor, SIGNAL(textChanged()),
             this, SLOT(editingStarts()) );

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

void ItemDelegate::editorSave()
{
    QAction *action = qobject_cast<QAction*>( sender() );
    Q_ASSERT(action != NULL);
    QObject *parent = action->parent();
    Q_ASSERT(parent != NULL);
    QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>( parent->parent() );
    Q_ASSERT(editor != NULL);

    emit commitData(editor);
    emit closeEditor(editor);
}

void ItemDelegate::editorCancel()
{
    QAction *action = qobject_cast<QAction*>( sender() );
    Q_ASSERT(action != NULL);
    QObject *parent = action->parent();
    Q_ASSERT(parent != NULL);
    QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>( parent->parent() );
    Q_ASSERT(editor != NULL);

    emit closeEditor(editor, QAbstractItemDelegate::RevertModelCache);
}

void ItemDelegate::onItemChanged(ItemWidget *item)
{
    for( int i = 0; i < m_cache.length(); ++i ) {
        ItemWidget *w = m_cache[i];
        if ( w != NULL && w == item ) {
            QSize oldSize = w->size();
            w->updateItem();
            emit rowChanged(i, oldSize);
            return;
        }
    }
}

void ItemDelegate::editingStarts()
{
    emit editingActive(true);
}

void ItemDelegate::editingStops()
{
    emit editingActive(false);
}

void ItemDelegate::rowsInserted(const QModelIndex &, int start, int end)
{
    for( int i = start; i <= end; ++i )
        m_cache.insert( i, NULL );
}

ItemWidget *ItemDelegate::cache(const QModelIndex &index)
{
    int n = index.row();

    ItemWidget *w = m_cache[n];
    if (w != NULL)
        return w;

    const QVariant displayData = index.data(Qt::DisplayRole);

    // Create item from image, html or text data.
    if ( displayData.canConvert<QPixmap>() ) {
        w = new ItemImage(displayData.value<QPixmap>(), m_parent);
    } else {
        bool hasHtml = displayData.type() == QVariant::String;
        const QString text = hasHtml ? displayData.toString() : index.data(Qt::EditRole).toString();

        if (hasHtml) {
#ifdef HAS_WEBKIT
            if (m_useWeb) {
                w = new ItemWeb(text, m_parent);
                connect( w->widget(), SIGNAL(itemChanged(ItemWidget*)),
                         this, SLOT(onItemChanged(ItemWidget*)) );
            }
            else
#endif
            {
                w = new ItemText(text, Qt::RichText, m_parent);
            }
        } else {
            w = new ItemText(text, Qt::PlainText, m_parent);
        }
    }

    w->setMaximumSize(m_maxSize);
    m_cache[n] = w;
    emit sizeHintChanged(index);

    return w;
}

bool ItemDelegate::hasCache(const QModelIndex &index) const
{
    return m_cache[index.row()] != NULL;
}

void ItemDelegate::setItemMaximumSize(const QSize &size)
{
    int w = size.width() - 8;
    if (m_showNumber) {
        w -= QFontMetrics(m_numberFont).boundingRect( QString("0123") ).width();
    }

    if (m_maxSize.width() == w)
        return;

    m_maxSize.setWidth(w);

    for( int i = 0; i < m_cache.length(); ++i ) {
        ItemWidget *w = m_cache[i];
        if (w != NULL) {
            QSize oldSize = w->size();
            w->setMaximumSize(m_maxSize);
            emit rowChanged(i, oldSize);
        }
    }
}

void ItemDelegate::updateRowPosition(int row, const QPoint &position)
{
    ItemWidget *w = m_cache[row];
    if (w == NULL)
        return;

    int x = position.x();
    int y = position.y();

    if (m_showNumber)
        x += QFontMetrics(m_numberFont).boundingRect( QString("0123") ).width();

    w->widget()->move(x, y);

    y += w->widget()->height();
    for (int i = row + 1; i < m_cache.size(); ++i ) {
        w = m_cache[i];
        if (w == NULL)
            continue;

        if (w->widget()->y() < y)
            w->widget()->hide();
    }
}

void ItemDelegate::hideRow(int row)
{
    ItemWidget *w = m_cache[row];
    if (w != NULL)
        w->widget()->hide();
}

void ItemDelegate::removeCache(const QModelIndex &index)
{
    removeCache(index.row());
}

void ItemDelegate::removeCache(int row)
{
    ItemWidget *w = m_cache[row];
    if (w != NULL) {
        m_cache[row] = NULL;
        delete w;
    }
}

void ItemDelegate::invalidateCache()
{
    for( int i = 0; i < m_cache.length(); ++i )
        removeCache(i);
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
    ItemWidget *w = m_cache[row];
    if (w == NULL)
        return;

    const QRect &rect = option.rect;

    /* render background (selected, alternate, ...) */
    QStyle *style = m_parent->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter);
    QPalette::ColorRole role = option.state & QStyle::State_Selected ?
                QPalette::HighlightedText : QPalette::Text;

    /* render number */
    QRect numRect(0, 0, 0, 0);
    if (m_showNumber) {
        QString num = QString::number(row) + "  ";
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
    w->setHighlight(m_re, m_foundFont, m_foundPalette);

    /* text color for selected/unselected item */
    QPalette palette = w->widget()->palette();
    palette.setColor( QPalette::Text, option.palette.color(role) );
    w->widget()->setPalette(palette);

    /* render widget */
    w->widget()->show();
}
