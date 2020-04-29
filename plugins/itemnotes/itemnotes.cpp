/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "gui/iconfont.h"
#include "gui/iconwidget.h"
#include "gui/pixelratio.h"

#include <QBoxLayout>
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

#include <cmath>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 10*1024;

const int notesIndent = 16;

QWidget *createIconWidget(const QByteArray &icon, QWidget *parent)
{
    if (!icon.isEmpty()) {
        QPixmap p;
        if (p.loadFromData(icon)) {
            const auto ratio = pixelRatio(parent);
            p.setDevicePixelRatio(ratio);

            const int side = ratio * (iconFontSizePixels() + 2);
            p = p.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QLabel *label = new QLabel(parent);
            const auto m = side / 4;
            label->setFixedSize( p.size() / ratio + QSize(m, m) );
            label->setAlignment(Qt::AlignCenter);
            label->setPixmap(p);
            return label;
        }
    }

    return new IconWidget(IconPenSquare, parent);
}

} // namespace

ItemNotes::ItemNotes(ItemWidget *childItem, const QString &text, const QByteArray &icon,
                     NotesPosition notesPosition, bool showToolTip)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidgetWrapper(childItem, this)
    , m_notes(new QTextEdit(this))
    , m_icon(nullptr)
    , m_timerShowToolTip(nullptr)
    , m_toolTipText()
{
    childItem->widget()->setObjectName("item_child");
    childItem->widget()->setParent(this);

    if (!icon.isEmpty())
        m_icon = createIconWidget(icon, this);

    QBoxLayout *layout;

    m_notes->setObjectName("item_child");
    m_notes->setProperty("CopyQ_item_type", "notes");

    m_notes->setReadOnly(true);
    m_notes->setUndoRedoEnabled(false);

    m_notes->setFocusPolicy(Qt::NoFocus);
    m_notes->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_notes->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_notes->setFrameStyle(QFrame::NoFrame);
    m_notes->setContextMenuPolicy(Qt::NoContextMenu);

    m_notes->viewport()->installEventFilter(this);

    m_notes->setPlainText( text.left(defaultMaxBytes) );

    if (notesPosition == NotesBeside)
        layout = new QHBoxLayout(this);
    else
        layout = new QVBoxLayout(this);

    auto labelLayout = new QHBoxLayout;
    labelLayout->setMargin(0);
    labelLayout->setContentsMargins(notesIndent, 0, 0, 0);

    if (m_icon)
        labelLayout->addWidget(m_icon, 0, Qt::AlignLeft | Qt::AlignTop);

    labelLayout->addWidget(m_notes, 1, Qt::AlignLeft | Qt::AlignTop);

    if (notesPosition == NotesBelow) {
        layout->addWidget( childItem->widget() );
        layout->addLayout(labelLayout);
    } else {
        layout->addLayout(labelLayout);
        layout->addWidget( childItem->widget() );
    }

    if (showToolTip) {
        m_timerShowToolTip = new QTimer(this);
        m_timerShowToolTip->setInterval(250);
        m_timerShowToolTip->setSingleShot(true);
        connect( m_timerShowToolTip, &QTimer::timeout,
                 this, &ItemNotes::showToolTip );
        m_toolTipText = text;
    }

    layout->setMargin(0);
    layout->setSpacing(0);
}

void ItemNotes::setCurrent(bool current)
{
    ItemWidgetWrapper::setCurrent(current);

    m_isCurrent = current;

    ItemWidget::setCurrent(current);

    if (m_timerShowToolTip == nullptr)
        return;

    QToolTip::hideText();

    if (current)
        m_timerShowToolTip->start();
    else
        m_timerShowToolTip->stop();
}

void ItemNotes::highlight(const QRegularExpression &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    ItemWidgetWrapper::highlight(re, highlightFont, highlightPalette);

    if (m_notes != nullptr) {
        QList<QTextEdit::ExtraSelection> selections;

        if ( re.isValid() && !re.pattern().isEmpty() ) {
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

void ItemNotes::updateSize(QSize maximumSize, int idealWidth)
{
    setMaximumSize(maximumSize);

    if (m_notes) {
        const int w = maximumSize.width() - 2 * notesIndent - 8;
        QTextDocument *doc = m_notes->document();
        doc->setTextWidth(w);
        m_notes->setFixedSize(
                    static_cast<int>(doc->idealWidth()) + 16,
                    static_cast<int>(doc->size().height()) );
    }

    ItemWidgetWrapper::updateSize(maximumSize, idealWidth);

    adjustSize();
    setFixedSize(sizeHint());
}

void ItemNotes::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    // Decorate notes.
    if (m_notes != nullptr) {
        QPainter p(this);

        QColor c = p.pen().color();
        c.setAlpha(80);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        QWidget *w = m_icon ? m_icon : m_notes;
        const auto height = std::max(w->height(), m_notes->height()) - 8;
        p.drawRect(w->x() - notesIndent + 4, w->y() + 4,
                   notesIndent - 4, height);
    }
}

bool ItemNotes::eventFilter(QObject *, QEvent *event)
{
    if ( event->type() == QEvent::Show && m_timerShowToolTip && m_isCurrent )
        m_timerShowToolTip->start();

    return ItemWidget::filterMouseEvents(m_notes, event);
}

void ItemNotes::showToolTip()
{
    QToolTip::hideText();

    if ( !isVisible() )
        return;

    QPoint toolTipPosition = QPoint(parentWidget()->contentsRect().width() - 16, height() - 16);
    toolTipPosition = mapToGlobal(toolTipPosition);

    QToolTip::showText(toolTipPosition, m_toolTipText, this);
}

ItemNotesLoader::ItemNotesLoader()
{
}

ItemNotesLoader::~ItemNotesLoader() = default;

QStringList ItemNotesLoader::formatsToSave() const
{
    return QStringList() << mimeItemNotes << mimeIcon;
}

QVariantMap ItemNotesLoader::applySettings()
{
    m_settings["notes_at_bottom"] = ui->radioButtonBottom->isChecked();
    m_settings["notes_beside"] =  ui->radioButtonBeside->isChecked();
    m_settings["show_tooltip"] = ui->checkBoxShowToolTip->isChecked();
    return m_settings;
}

QWidget *ItemNotesLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemNotesSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    if ( m_settings["notes_at_bottom"].toBool() )
        ui->radioButtonBottom->setChecked(true);
    else if ( m_settings["notes_beside"].toBool() )
        ui->radioButtonBeside->setChecked(true);
    else
        ui->radioButtonTop->setChecked(true);

    ui->checkBoxShowToolTip->setChecked( m_settings["show_tooltip"].toBool() );

    return w;
}

ItemWidget *ItemNotesLoader::transform(ItemWidget *itemWidget, const QVariantMap &data)
{
    const auto text = getTextData(data, mimeItemNotes);
    const QByteArray icon = data.value(mimeIcon).toByteArray();
    if ( text.isEmpty() && icon.isEmpty() )
        return nullptr;

    const NotesPosition notesPosition =
            m_settings["notes_at_bottom"].toBool() ? NotesBelow
          : m_settings["notes_beside"].toBool() ? NotesBeside
          : NotesAbove;

    itemWidget->setTagged(true);
    return new ItemNotes(
        itemWidget, text, icon, notesPosition,
        m_settings["show_tooltip"].toBool() );
}

bool ItemNotesLoader::matches(const QModelIndex &index, const QRegularExpression &re) const
{
    const QString text = index.data(contentType::notes).toString();
    return text.contains(re);
}
