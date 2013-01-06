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
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QtWebKit/QWebView>
#include <QtWebKit/QWebPage>
#include <QtWebKit/QWebFrame>
#include "client_server.h"
#include "configurationmanager.h"

const QSize defaultSize(0, 512);
const int maxChars = 100*1024;

ItemDelegate::ItemDelegate(QWidget *parent)
    : QItemDelegate(parent)
    , m_parent(parent)
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
}

ItemDelegate::~ItemDelegate()
{
    invalidateCache();
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &,
                             const QModelIndex &index) const
{
    const QWidget *w = m_cache.value(index.row(), NULL);
    return (w != NULL) ? w->size() : defaultSize;
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

QWidget *ItemDelegate::cache(const QModelIndex &index)
{
    int n = index.row();

    QWidget *w = m_cache[n];
    if (w != NULL)
        return w;

    QVariant displayData = index.data(Qt::DisplayRole);
    bool hasHtml = displayData.type() == QVariant::String;
    QString text = hasHtml ? displayData.toString() : index.data(Qt::EditRole).toString();

    if ( !text.isEmpty() ) {
        if (hasHtml) {
            QWebView *view = new QWebView(m_parent);
            w = view;

            // FIXME: Set only maximal size for document.
            const QRect rect = m_parent->contentsRect();
            view->resize(rect.width() - 16, rect.height() - 16);

            view->page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
            view->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical,   Qt::ScrollBarAlwaysOff);
            view->setFont( m_parent->font() );
            view->setContent(text.toLocal8Bit(), QString("text/html"));
        } else {
            QLabel *label = new QLabel(m_parent);
            w = label;

            label->setTextFormat(Qt::PlainText);
            label->setMargin(4);
            label->setFont( m_parent->font() );
            label->setAutoFillBackground(false);
            label->setPalette( m_parent->palette() );
            label->setText(text);
            label->adjustSize();
        }
    } else {
        // Image
        QLabel *label = new QLabel(m_parent);
        w = label;
        label->setMargin(4);
        label->setPixmap(displayData.value<QPixmap>());
        w->adjustSize();
    }
    w->hide();

    // transparent background
    QPalette palette(w->palette());
    palette.setColor(QPalette::Background, Qt::transparent);
    w->setPalette(palette);

    m_cache[n] = w;
    emit sizeHintChanged(index);

    return w;
}

bool ItemDelegate::hasCache(const QModelIndex &index) const
{
    return m_cache[index.row()] != NULL;
}

void ItemDelegate::removeCache(const QModelIndex &index)
{
    removeCache(index.row());
}

void ItemDelegate::removeCache(int row)
{
    QWidget *w = m_cache[row];
    if (w) {
        w->deleteLater();
        m_cache[row] = NULL;
    }
}

void ItemDelegate::invalidateCache()
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
    if (w == NULL)
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
    QWebView *view = NULL;
    QLabel *label = NULL;
    QString labelText;
    if ( !m_re.isEmpty() ) {
        view = qobject_cast<QWebView *>(w);
        // FIXME: Hightlight matching text!
        if (view != NULL) {
            view->findText(m_re.pattern(), QWebPage::HighlightAllOccurrences);
        } else {
            label = qobject_cast<QLabel *>(w);
            if (label != NULL) {
                labelText = label->text();

                QTextDocument doc(labelText);
                doc.setDefaultFont( m_parent->font() );

                QTextCursor cur = doc.find(m_re);
                int a = cur.position();
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
                    cur = doc.find(m_re, cur);
                    int b = cur.position();
                    if (a == b) {
                        cur.movePosition(QTextCursor::NextCharacter);
                        cur = doc.find(m_re, cur);
                        b = cur.position();
                        if (a == b) break;
                    }
                    a = b;
                }

                label->setTextFormat(Qt::RichText);
                label->setText(doc.toHtml());
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
    QRegion region(0, 0, rect.width() - numRect.width() - 4, rect.height());
    painter->save();
    painter->translate( rect.topLeft() + QPoint(numRect.width(), 0) );
    w->render(painter, QPoint(), region);
    painter->restore();

    /* restore highlight */
    if (view != NULL) {
        view->findText(QString(), QWebPage::HighlightAllOccurrences);
    } else if ( label != NULL ) {
        label->setTextFormat(Qt::PlainText);
        label->setText(labelText);
    }
}
