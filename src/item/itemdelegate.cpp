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

#include "common/client_server.h"
#include "common/contenttype.h"
#include "itemfactory.h"
#include "itemwidget.h"

#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>

namespace {

const QSize defaultSize(0, 512);
const QSize defaultMaximumSize(2048, 2048 * 8);
const char propertyItemIndex[] = "CopyQ_item_index";
const char propertyEditNotes[] = "CopyQ_edit_notes";

inline void reset(QSharedPointer<ItemWidget> *ptr, ItemWidget *value = NULL)
{
#if QT_VERSION < 0x050000
    if (value != NULL)
        *ptr = QSharedPointer<ItemWidget>(value);
    else
        ptr->clear();
#else
    ptr->reset(value);
#endif
}

} // namespace

ItemDelegate::ItemDelegate(QWidget *parent)
    : QItemDelegate(parent)
    , m_parent(parent)
    , m_showNumber(false)
    , m_saveOnReturnKey(true)
    , m_re()
    , m_maxSize(defaultMaximumSize)
    , m_editNotes(false)
    , m_foundFont()
    , m_foundPalette()
    , m_editorFont()
    , m_editorPalette()
    , m_numberFont()
    , m_numberWidth(0)
    , m_numberPalette()
    , m_cache()
{
}

ItemDelegate::~ItemDelegate()
{
    invalidateCache();
}

QSize ItemDelegate::sizeHint(const QModelIndex &index) const
{
    int row = index.row();
    if ( row < m_cache.size() ) {
        const ItemWidget *w = m_cache[row].data();
        if (w != NULL)
            return w->widget()->size();
    }
    return defaultSize;
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &,
                             const QModelIndex &index) const
{
    return sizeHint(index);
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (object->objectName() == "editor") {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(object);
        if (editor == NULL)
            return false;

        QEvent::Type type = event->type();
        if ( type == QEvent::KeyPress ) {
            QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
            switch ( keyevent->key() ) {
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    // Commit data on Ctrl+Return or Enter?
                    if (m_saveOnReturnKey) {
                        if (keyevent->modifiers() == Qt::ControlModifier) {
                            editor->insertPlainText("\n");
                            return true;
                        } else if (keyevent->modifiers() != Qt::NoModifier) {
                            return false;
                        }
                    } else {
                        if (keyevent->modifiers() != Qt::ControlModifier)
                            return false;
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

            return true;
        } else if ( type == QEvent::FocusOut ) {
            // Don't allow editor to loose child focus.
            editor->setFocus();
            return false;
        }
    } else {
        // resize event for items
        if (event->type() == QEvent::Resize) {
            QResizeEvent *resize = static_cast<QResizeEvent *>(event);
            ItemWidget *item = dynamic_cast<ItemWidget *>(object);
            if (item != NULL) {
                item->widget()->resize(resize->size());
                int i = item->widget()->property(propertyItemIndex).toInt();
                emit rowSizeChanged(i);
                return true;
            }
        }
    }

    return false;
}

QWidget *ItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                    const QModelIndex &index) const
{
    ItemWidget *w = m_cache[index.row()].data();
    QWidget *editor = (w == NULL || m_editNotes) ? new QPlainTextEdit(parent->window())
                                                 : w->createEditor(parent->window());
    if (editor == NULL)
        return NULL;

    editor->setPalette(m_editorPalette);
    editor->setFont(m_editorFont);
    editor->setObjectName("editor");

    connect( editor, SIGNAL(destroyed()),
             this, SLOT(editingStops()) );
    connect( editor, SIGNAL(textChanged()),
             this, SLOT(editingStarts()) );

    if (m_editNotes)
        editor->setProperty(propertyEditNotes, true);

    return editor;
}

void ItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &, const QModelIndex &) const
{
    editor->setGeometry(editor->parentWidget()->contentsRect());
}

void ItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if ( editor->property(propertyEditNotes).toBool() ) {
        editor->setProperty( "plainText", index.data(contentType::notes) );
    } else {
        ItemWidget *w = m_cache[index.row()].data();
        if (w != NULL)
            w->setEditorData(editor, index);
        else
            editor->setProperty( "plainText", index.data(Qt::EditRole) );
    }
}

void ItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                const QModelIndex &index) const
{
    if ( editor->property(propertyEditNotes).toBool() ) {
        QPlainTextEdit *textEdit = (qobject_cast<QPlainTextEdit*>(editor));
        model->setData(index, textEdit->toPlainText(), contentType::notes);
    } else {
        ItemWidget *w = m_cache[index.row()].data();
        if (w != NULL) {
            w->setModelData(editor, model, index);
        } else {
            QPlainTextEdit *textEdit = (qobject_cast<QPlainTextEdit*>(editor));
            model->setData(index, textEdit->toPlainText());
        }
    }
}

void ItemDelegate::dataChanged(const QModelIndex &a, const QModelIndex &b)
{
    // recalculating sizes of many items is expensive (when searching)
    // - assume that highlighted (matched) text has same size
    // - recalculate size only if item edited
    int row = a.row();
    if ( row == b.row() ) {
        reset(&m_cache[row]);
        emit rowSizeChanged(a.row());
    }
}

void ItemDelegate::rowsRemoved(const QModelIndex &, int start, int end)
{
    for( int i = end; i >= start; --i ) {
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
        m_cache.insert(i, QSharedPointer<ItemWidget>());
}

ItemWidget *ItemDelegate::cache(const QModelIndex &index)
{
    int n = index.row();

    ItemWidget *w = m_cache[n].data();
    if (w == NULL) {
        w = ItemFactory::instance()->createItem(index, m_parent);
        setIndexWidget(index, w);
    } else {
        w->widget()->setProperty(propertyItemIndex, index.row());
    }

    return w;
}

bool ItemDelegate::hasCache(const QModelIndex &index) const
{
    return !m_cache[index.row()].isNull();
}

void ItemDelegate::setItemMaximumSize(const QSize &size)
{
    int width = size.width() - 8;
    if (m_showNumber)
        width -= m_numberWidth;

    if (m_maxSize.width() == width)
        return;

    m_maxSize.setWidth(width);

    for( int i = 0; i < m_cache.length(); ++i ) {
        ItemWidget *w = m_cache[i].data();
        if (w != NULL) {
            w->widget()->setMaximumSize(m_maxSize);
            w->widget()->setMinimumWidth(width);
            w->updateSize();
        }
    }
}

void ItemDelegate::updateRowPosition(int row, const QPoint &position)
{
    ItemWidget *w = m_cache[row].data();
    if (w == NULL)
        return;

    int x = position.x();
    int y = position.y();

    if (m_showNumber)
        x += m_numberWidth;

    w->widget()->move(x, y);

    y += w->widget()->height();
    for (int i = row + 1; i < m_cache.size(); ++i ) {
        w = m_cache[i].data();
        if (w == NULL)
            continue;

        if (w->widget()->y() < y)
            w->widget()->hide();
    }
}

void ItemDelegate::setRowVisible(int row, bool visible)
{
    ItemWidget *w = m_cache[row].data();
    if (w != NULL)
        w->widget()->setVisible(visible);
}

void ItemDelegate::nextItemLoader(const QModelIndex &index)
{
    ItemWidget *w = m_cache[index.row()].data();
    if (w != NULL) {
        ItemWidget *w2 = ItemFactory::instance()->nextItemLoader(index, w);
        if (w2 != NULL)
            setIndexWidget(index, w2);
    }
}

void ItemDelegate::previousItemLoader(const QModelIndex &index)
{
    ItemWidget *w = m_cache[index.row()].data();
    if (w != NULL) {
        ItemWidget *w2 = ItemFactory::instance()->previousItemLoader(index, w);
        if (w2 != NULL)
            setIndexWidget(index, w2);
    }
}

void ItemDelegate::setEditNotes(bool editNotes)
{
    m_editNotes = editNotes;
}

void ItemDelegate::setIndexWidget(const QModelIndex &index, ItemWidget *w)
{
    reset(&m_cache[index.row()], w);
    if (w == NULL)
        return;

    w->widget()->setMaximumSize(m_maxSize);
    w->widget()->setMinimumWidth(m_maxSize.width());
    w->updateSize();
    w->widget()->installEventFilter(this);
    w->widget()->setProperty(propertyItemIndex, index.row());

    emit rowSizeChanged(index.row());
}

void ItemDelegate::invalidateCache()
{
    for( int i = 0; i < m_cache.length(); ++i )
        reset(&m_cache[i]);
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
    m_numberWidth = QFontMetrics(m_numberFont).boundingRect( QString("0123") ).width();
    m_numberPalette = palette;
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    int row = index.row();

    ItemWidget *w = m_cache[row].data();
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
}
