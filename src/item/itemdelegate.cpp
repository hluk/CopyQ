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
#include "gui/iconfactory.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"

#include <QMenu>
#include <QListView>
#include <QLayout>
#include <QMainWindow>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QToolBar>

namespace {

const QSize defaultSize(0, 512);
const QSize defaultMaximumSize(2048, 2048 * 8);
const char propertyItemIndex[] = "CopyQ_item_index";
const char propertyEditNotes[] = "CopyQ_edit_notes";
const QString editorObjectName = "CopyQ_editor";

const QIcon iconSave(const QColor &color = QColor()) { return getIcon("document-save", IconSave, color); }
const QIcon iconCancel(const QColor &color = QColor()) { return getIcon("document-revert", IconRemove, color); }
const QIcon iconUndo(const QColor &color = QColor()) { return getIcon("edit-undo", IconUndo, color); }
const QIcon iconRedo(const QColor &color = QColor()) { return getIcon("edit-redo", IconRepeat, color); }

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

QWidget *getEditor(QObject *widget)
{
    return widget->findChild<QPlainTextEdit *>(editorObjectName);
}

QPlainTextEdit *getPlainTextEdit(QWidget *widget)
{
    return qobject_cast<QPlainTextEdit *>(widget->findChild<QWidget *>(editorObjectName));
}

} // namespace

ItemDelegate::ItemDelegate(QListView *parent)
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

    return false;
}

QWidget *ItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                    const QModelIndex &index) const
{
    // Use parent's parent widget so that context menu works everywhere in editor and
    // scrolling in viewport doesn't affect editor.
    QWidget *realParent = parent->parentWidget();
    Q_ASSERT(realParent != NULL);

    QWidget *widget = new QWidget(realParent);

    ItemWidget *w = m_cache[index.row()].data();
    QWidget *editor = (w == NULL || m_editNotes) ? new QPlainTextEdit(widget)
                                                 : w->createEditor(widget);
    if (editor == NULL) {
        delete widget;
        return NULL;
    }

    editor->setPalette(m_editorPalette);
    editor->setFont(m_editorFont);
    editor->setObjectName(editorObjectName);

    if (m_editNotes)
        widget->setProperty(propertyEditNotes, true);

    widget->setPalette(m_editorPalette);
    widget->setBackgroundRole(QPalette::Base);
    widget->setAutoFillBackground(true);

    QToolBar *toolBar = new QToolBar(widget);
    toolBar->setBackgroundRole(QPalette::Base);

    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(0);
    layout->setContentsMargins(QMargins(0, 0, 0, 0));
    layout->addWidget(toolBar);
    layout->addWidget(editor);

    widget->setFocusProxy(editor);

    const QColor color = getDefaultIconColor( m_editorPalette.color(QPalette::Base) );

    QAction *act;
    act = new QAction( iconSave(color), tr("Save"), editor );
    toolBar->addAction(act);
    act->setToolTip( tr("Save Item") );
    act->setShortcuts( QList<QKeySequence>()
                       << QKeySequence(tr("F2", "Shortcut to save item editor changes"))
                       << QKeySequence(tr("Ctrl+Return", "Shortcut to save item editor changes"))
                       << QKeySequence(tr("Ctrl+Enter", "Shortcut to save item editor changes"))
                       );
    connect( act, SIGNAL(triggered()), this, SLOT(editorSave()) );

    act = new QAction( iconCancel(color), tr("Cancel"), editor );
    toolBar->addAction(act);
    act->setToolTip( tr("Cancel Editing and Revert Changes") );
    act->setShortcut( QKeySequence(tr("Escape", "Shortcut to revert item editor changes")) );
    connect( act, SIGNAL(triggered()), this, SLOT(editorCancel()) );

    QPlainTextEdit *plainTextEdit = qobject_cast<QPlainTextEdit*>(editor);
    if (plainTextEdit != NULL) {
        plainTextEdit->setFrameShape(QFrame::NoFrame);

        act = new QAction( iconUndo(color), tr("Undo"), editor );
        toolBar->addAction(act);
        act->setShortcut( QKeySequence::Undo );
        act->setEnabled(false);
        connect( act, SIGNAL(triggered()), plainTextEdit, SLOT(undo()) );
        connect( plainTextEdit, SIGNAL(undoAvailable(bool)), act, SLOT(setEnabled(bool)) );

        act = new QAction( iconRedo(color), tr("Redo"), editor );
        toolBar->addAction(act);
        act->setShortcut( QKeySequence::Redo );
        act->setEnabled(false);
        connect( act, SIGNAL(triggered()), plainTextEdit, SLOT(redo()) );
        connect( plainTextEdit, SIGNAL(redoAvailable(bool)), act, SLOT(setEnabled(bool)) );
    }

    return widget;
}

void ItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &, const QModelIndex &) const
{
    editor->setGeometry(editor->parentWidget()->contentsRect());
}

void ItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    bool hasCustomEditor = false;

    if ( editor->property(propertyEditNotes).toBool() ) {
        getPlainTextEdit(editor)->setProperty( "plainText", index.data(contentType::notes) );
    } else {
        ItemWidget *w = m_cache[index.row()].data();
        hasCustomEditor = w != NULL;
        if (hasCustomEditor)
            w->setEditorData(getEditor(editor), index);
        else
            getPlainTextEdit(editor)->setProperty( "plainText", index.data(Qt::EditRole) );
    }

    if (!hasCustomEditor) {
        QPlainTextEdit *textEdit = (qobject_cast<QPlainTextEdit *>(editor));
        textEdit->selectAll();
    }
}

void ItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                const QModelIndex &index) const
{
    if ( editor->property(propertyEditNotes).toBool() ) {
        model->setData(index, getPlainTextEdit(editor)->toPlainText(), contentType::notes);
    } else {
        ItemWidget *w = m_cache[index.row()].data();
        if (w != NULL)
            w->setModelData(getEditor(editor), model, index);
        else
            model->setData(index, getPlainTextEdit(editor)->toPlainText());
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
    QWidget *editor = qobject_cast<QWidget *>(action->parent());
    Q_ASSERT(editor != NULL);
    editor = editor->parentWidget();
    Q_ASSERT(editor != NULL);

    emit commitData(editor);
    emit closeEditor(editor);
}

void ItemDelegate::editorCancel()
{
    QAction *action = qobject_cast<QAction*>( sender() );
    Q_ASSERT(action != NULL);
    QWidget *editor = qobject_cast<QWidget *>(action->parent());
    Q_ASSERT(editor != NULL);
    editor = editor->parentWidget();
    Q_ASSERT(editor != NULL);

    emit closeEditor(editor, QAbstractItemDelegate::RevertModelCache);
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
        w = ItemFactory::instance()->createItem(index, m_parent->viewport());
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

    bool isSelected = option.state & QStyle::State_Selected;

    /* render background (selected, alternate, ...) */
    QStyle *style = m_parent->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, m_parent);
    QPalette::ColorRole role = isSelected ? QPalette::HighlightedText : QPalette::Text;

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
    QWidget *ww = w->widget();
    if ( ww->property("CopyQ_selected") != isSelected ) {
        ww->setProperty("CopyQ_selected", isSelected);
        style->unpolish(ww);
        style->polish(ww);
        ww->update();
    }

    /* show small icon if item has notes */
    if ( index.data(contentType::hasNotes).toBool() ) {
        painter->setPen(m_numberPalette.color(role));
        IconFactory::instance()->drawIcon(IconEditSign, rect, painter);
    }
}
