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

#include "itemnotes.h"
#include "ui_itemnotessettings.h"

#include "common/contenttype.h"

#include <QContextMenuEvent>
#include <QLabel>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPainter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolTip>
#include <QtPlugin>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 10*1024;

const QString defaultFormat = QString("application/x-copyq-item-notes");

const int notesIndent = 16;

} // namespace

ItemNotes::ItemNotes(ItemWidget *childItem, const QString &text,
                     bool notesAtBottom, bool showIconOnly, bool showToolTip)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_notes( showIconOnly ? NULL : new QTextEdit(this) )
    , m_icon( showIconOnly ? new QLabel(this) : NULL )
    , m_childItem(childItem)
    , m_notesAtBottom(notesAtBottom)
    , m_timerShowToolTip(NULL)
    , m_toolTipText()
{
    m_childItem->widget()->setObjectName("item_child");
    m_childItem->widget()->setParent(this);

    if (showIconOnly) {
        m_icon->setObjectName("item_child");
        m_icon->setTextFormat(Qt::RichText);
        m_icon->setText("<span style=\"font-family:FontAwesome\">&#xf14b;</span>");
        m_icon->adjustSize();
        m_childItem->widget()->move( m_icon->width() + 6, 0 );
        m_icon->move(4, 6);
    } else {
        m_notes->setObjectName("item_child");

        m_notes->document()->setDefaultFont(font());

        m_notes->setReadOnly(true);
        m_notes->setUndoRedoEnabled(false);

        m_notes->setFocusPolicy(Qt::NoFocus);
        m_notes->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_notes->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_notes->setFrameStyle(QFrame::NoFrame);

        // Selecting text copies it to clipboard.
        connect( m_notes, SIGNAL(selectionChanged()), SLOT(onSelectionChanged()) );

        m_notes->setPlainText( text.left(defaultMaxBytes) );

        m_notes->move(notesIndent, 0);

        m_notes->viewport()->installEventFilter(this);
    }

    if (showToolTip) {
        m_timerShowToolTip = new QTimer(this);
        m_timerShowToolTip->setInterval(250);
        m_timerShowToolTip->setSingleShot(true);
        connect( m_timerShowToolTip, SIGNAL(timeout()),
                 this, SLOT(showToolTip()) );
        m_toolTipText = text;
    }
}

void ItemNotes::setCurrent(bool current)
{
    if (m_timerShowToolTip == NULL)
        return;

    QToolTip::hideText();

    if (current)
        m_timerShowToolTip->start();
    else
        m_timerShowToolTip->stop();
}

void ItemNotes::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_childItem->setHighlight(re, highlightFont, highlightPalette);

    if (m_notes != NULL) {
        QList<QTextEdit::ExtraSelection> selections;

        if ( !re.isEmpty() ) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground( highlightPalette.base() );
            selection.format.setForeground( highlightPalette.text() );
            selection.format.setFont(highlightFont);

            QTextCursor cur = m_notes->document()->find(re);
            int a = cur.position();
            while ( !cur.isNull() ) {
                if ( cur.hasSelection() ) {
                    selection.cursor = cur;
                    selections.append(selection);
                } else {
                    cur.movePosition(QTextCursor::NextCharacter);
                }
                cur = m_notes->document()->find(re, cur);
                int b = cur.position();
                if (a == b) {
                    cur.movePosition(QTextCursor::NextCharacter);
                    cur = m_notes->document()->find(re, cur);
                    b = cur.position();
                    if (a == b) break;
                }
                a = b;
            }
        }

        m_notes->setExtraSelections(selections);
    }

    update();
}

void ItemNotes::updateSize()
{
    // FIXME: Set correct layout with icon only.
    const int w = maximumWidth() - ( m_icon != NULL ? m_icon->width() : 0 );

    m_childItem->widget()->setMaximumWidth(w);
    m_childItem->updateSize();

    const int h = m_childItem->widget()->height();

    if (m_notes != NULL) {
        m_notes->setMaximumWidth(w - notesIndent);
        m_notes->document()->setTextWidth(w - notesIndent);
        m_notes->resize( m_notes->document()->idealWidth() + 16, m_notes->document()->size().height() );

        if (m_notesAtBottom)
            m_notes->move(notesIndent, h);
        else
            m_childItem->widget()->move( 0, m_notes->height() );

        resize( w, h + m_notes->height() );
    } else {
        resize(w, h);
    }
}

void ItemNotes::mousePressEvent(QMouseEvent *e)
{
    m_notes->setTextCursor( m_notes->cursorForPosition(e->pos()) );
    QWidget::mousePressEvent(e);
    e->ignore();
}

void ItemNotes::mouseDoubleClickEvent(QMouseEvent *e)
{
    if ( e->modifiers().testFlag(Qt::ShiftModifier) )
        QWidget::mouseDoubleClickEvent(e);
    else
        e->ignore();
}

void ItemNotes::contextMenuEvent(QContextMenuEvent *e)
{
    e->ignore();
}

void ItemNotes::mouseReleaseEvent(QMouseEvent *e)
{
    if ( property("copyOnMouseUp").toBool() ) {
        setProperty("copyOnMouseUp", false);
        if ( m_notes->textCursor().hasSelection() )
            m_notes->copy();
    } else {
        QWidget::mouseReleaseEvent(e);
    }
}

void ItemNotes::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    // Decorate notes.
    if (m_notes != NULL) {
        QPainter p(this);

        QColor c = p.pen().color();
        c.setAlpha(80);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRect(m_notes->x() - notesIndent + 4, m_notes->y() + 4,
                   notesIndent - 4, m_notes->height() - 8);
    }
}

bool ItemNotes::eventFilter(QObject *obj, QEvent *event)
{
    if ( m_notes != NULL && obj == m_notes->viewport() ) {
        if ( event->type() == QEvent::MouseButtonPress ) {
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);
            mousePressEvent(ev);
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ItemNotes::onSelectionChanged()
{
    setProperty("copyOnMouseUp", true);
}

void ItemNotes::showToolTip()
{
    QToolTip::hideText();

    QPoint toolTipPosition = QPoint(parentWidget()->contentsRect().width() - 16, height() - 16);
    toolTipPosition = mapToGlobal(toolTipPosition);

    QToolTip::showText(toolTipPosition, m_toolTipText, this);
}

ItemNotesLoader::ItemNotesLoader()
    : ui(NULL)
{
}

ItemNotesLoader::~ItemNotesLoader()
{
    delete ui;
}

QStringList ItemNotesLoader::formatsToSave() const
{
    return QStringList(defaultFormat);
}

QVariantMap ItemNotesLoader::applySettings()
{
    m_settings["notes_at_bottom"] = ui->radioButtonBottom->isChecked();
    m_settings["icon_only"] = ui->radioButtonIconOnly->isChecked();
    m_settings["show_tooltip"] = ui->checkBoxShowToolTip->isChecked();
    return m_settings;
}

QWidget *ItemNotesLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemNotesSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    if ( m_settings["icon_only"].toBool() )
        ui->radioButtonIconOnly->setChecked(true);
    else if ( m_settings["notes_at_bottom"].toBool() )
        ui->radioButtonBottom->setChecked(true);
    else
        ui->radioButtonTop->setChecked(true);

    ui->checkBoxShowToolTip->setChecked( m_settings["show_tooltip"].toBool() );

    return w;
}

ItemWidget *ItemNotesLoader::transform(ItemWidget *itemWidget, const QModelIndex &index)
{
    const QString text = index.data(contentType::notes).toString();
    if ( text.isEmpty() )
        return NULL;

    return new ItemNotes( itemWidget, text,
                          m_settings["notes_at_bottom"].toBool(),
                          m_settings["icon_only"].toBool(),
                          m_settings["show_tooltip"].toBool() );
}

Q_EXPORT_PLUGIN2(itemtext, ItemNotesLoader)
