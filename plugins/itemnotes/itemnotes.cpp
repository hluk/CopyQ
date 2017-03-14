/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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
#include "gui/iconfont.h"
#include "gui/iconwidget.h"

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
#include <QDebug>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 10*1024;

const char mimeNotes[] = "application/x-copyq-item-notes";
const char mimeIcon[] = "application/x-copyq-item-icon";

const int notesIndent = 16;

QWidget *createIconWidget(const QByteArray &icon, QWidget *parent)
{
    if (!icon.isEmpty()) {
        QPixmap p;
        if (p.loadFromData(icon)) {
            const int side = iconFontSizePixels() + 2;
            p = p.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QLabel *label = new QLabel(parent);
            const int m = side / 4;
            label->setContentsMargins(m, m, m, m);
            label->setPixmap(p);
            return label;
        }
    }

    return new IconWidget(IconEditSign, parent);
}

} // namespace

ItemNotes::ItemNotes(ItemWidget *childItem, const QString &text, const QByteArray &icon,
                     bool notesAtBottom, bool showIconOnly, bool showToolTip)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_notes(nullptr)
    , m_icon(nullptr)
    , m_childItem(childItem)
    , m_notesAtBottom(notesAtBottom)
    , m_timerShowToolTip(nullptr)
    , m_toolTipText()
{
    m_childItem->widget()->setObjectName("item_child");
    m_childItem->widget()->setParent(this);

    if (showIconOnly || !icon.isEmpty())
        m_icon = createIconWidget(icon, this);

    if (!showIconOnly)
        m_notes = new QTextEdit(this);

    QBoxLayout *layout;

    if (showIconOnly) {
        layout = new QHBoxLayout(this);
        layout->addWidget(m_icon, 0, Qt::AlignRight | Qt::AlignTop);
        layout->addWidget(m_childItem->widget());
    } else {
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

        layout = new QVBoxLayout(this);

        auto labelLayout = new QHBoxLayout;
        labelLayout->setMargin(0);
        labelLayout->setContentsMargins(notesIndent, 0, 0, 0);

        if (m_icon)
            labelLayout->addWidget(m_icon, 0, Qt::AlignLeft);

        labelLayout->addWidget(m_notes, 1, Qt::AlignLeft);

        if (notesAtBottom) {
            layout->addWidget( m_childItem->widget() );
            layout->addLayout(labelLayout);
        } else {
            layout->addLayout(labelLayout);
            layout->addWidget( m_childItem->widget() );
        }
    }

    if (showToolTip) {
        m_timerShowToolTip = new QTimer(this);
        m_timerShowToolTip->setInterval(250);
        m_timerShowToolTip->setSingleShot(true);
        connect( m_timerShowToolTip, SIGNAL(timeout()),
                 this, SLOT(showToolTip()) );
        m_toolTipText = text;
    }

    layout->setMargin(0);
    layout->setSpacing(0);
}

void ItemNotes::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_childItem->setHighlight(re, highlightFont, highlightPalette);

    if (m_notes != nullptr) {
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

QWidget *ItemNotes::createEditor(QWidget *parent) const
{
    return (m_childItem == nullptr) ? nullptr : m_childItem->createEditor(parent);
}

void ItemNotes::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    Q_ASSERT(m_childItem != nullptr);
    return m_childItem->setEditorData(editor, index);
}

void ItemNotes::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    Q_ASSERT(m_childItem != nullptr);
    return m_childItem->setModelData(editor, model, index);
}

bool ItemNotes::hasChanges(QWidget *editor) const
{
    Q_ASSERT(m_childItem != nullptr);
    return m_childItem->hasChanges(editor);
}

QObject *ItemNotes::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    return m_childItem ? m_childItem->createExternalEditor(index, parent)
                       : ItemWidget::createExternalEditor(index, parent);
}

void ItemNotes::updateSize(const QSize &maximumSize, int idealWidth)
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

    if (m_childItem != nullptr)
        m_childItem->updateSize(maximumSize, idealWidth);

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
        p.drawRect(w->x() - notesIndent + 4, w->y() + 4,
                   notesIndent - 4, m_notes->height() - 8);
    }
}

bool ItemNotes::eventFilter(QObject *, QEvent *event)
{
    return ItemWidget::filterMouseEvents(m_notes, event);
}

void ItemNotes::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (m_timerShowToolTip != nullptr) {
        QToolTip::hideText();
        m_timerShowToolTip->start();
    }
}

void ItemNotes::hideEvent(QHideEvent *event)
{
    if (m_timerShowToolTip != nullptr) {
        QToolTip::hideText();
        m_timerShowToolTip->stop();
    }

    QWidget::hideEvent(event);
}

void ItemNotes::showToolTip()
{
    QToolTip::hideText();

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
    return QStringList() << mimeNotes << mimeIcon;
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
    ui.reset(new Ui::ItemNotesSettings);
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
        return nullptr;

    const QByteArray icon = index.data(contentType::data).toMap().value(mimeIcon).toByteArray();

    itemWidget->setTagged(true);
    return new ItemNotes( itemWidget, text, icon,
                          m_settings["notes_at_bottom"].toBool(),
                          m_settings["icon_only"].toBool(),
            m_settings["show_tooltip"].toBool() );
}

bool ItemNotesLoader::matches(const QModelIndex &index, const QRegExp &re) const
{
    const QString text = index.data(contentType::notes).toString();
    return re.indexIn(text) != -1;
}

Q_EXPORT_PLUGIN2(itemnotes, ItemNotesLoader)
