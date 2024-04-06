// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemnotes.h"
#include "ui_itemnotessettings.h"

#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "gui/iconfont.h"
#include "gui/iconwidget.h"
#include "gui/pixelratio.h"
#include "item/itemfilter.h"

#include <QBoxLayout>
#include <QLabel>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolTip>
#include <QVariantMap>
#include <QtPlugin>

#include <cmath>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 10*1024;

const int notesIndent = 16;

const QLatin1String configNotesAtBottom("notes_at_bottom");
const QLatin1String configNotesBeside("notes_beside");
const QLatin1String configShowTooltip("show_tooltip");

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

    return new IconWidget(IconPenToSquare, parent);
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

    layout->setContentsMargins({});
    layout->setSpacing(0);
}

void ItemNotes::setCurrent(bool current)
{
    ItemWidgetWrapper::setCurrent(current);

    m_isCurrent = current;

    if (m_timerShowToolTip == nullptr)
        return;

    QToolTip::hideText();

    if (current)
        m_timerShowToolTip->start();
    else
        m_timerShowToolTip->stop();
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

void ItemNotesLoader::applySettings(QSettings &settings)
{
    settings.setValue(configNotesAtBottom, ui->radioButtonBottom->isChecked());
    settings.setValue(configNotesBeside,  ui->radioButtonBeside->isChecked());
    settings.setValue(configShowTooltip, ui->checkBoxShowToolTip->isChecked());
}

void ItemNotesLoader::loadSettings(const QSettings &settings)
{
    m_notesAtBottom = settings.value(configNotesAtBottom, false).toBool();
    m_notesBeside = settings.value(configNotesBeside, false).toBool();
    m_showTooltip = settings.value(configShowTooltip, false).toBool();
}

QWidget *ItemNotesLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemNotesSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    if (m_notesAtBottom)
        ui->radioButtonBottom->setChecked(true);
    else if (m_notesBeside)
        ui->radioButtonBeside->setChecked(true);
    else
        ui->radioButtonTop->setChecked(true);

    ui->checkBoxShowToolTip->setChecked(m_showTooltip);

    return w;
}

ItemWidget *ItemNotesLoader::transform(ItemWidget *itemWidget, const QVariantMap &data)
{
    const auto text = getTextData(data, mimeItemNotes);
    const QByteArray icon = data.value(mimeIcon).toByteArray();
    if ( text.isEmpty() && icon.isEmpty() )
        return nullptr;

    const NotesPosition notesPosition =
            m_notesAtBottom ? NotesBelow
          : m_notesBeside ? NotesBeside
          : NotesAbove;

    itemWidget->setTagged(true);
    return new ItemNotes(
        itemWidget, text, icon, notesPosition, m_showTooltip );
}

bool ItemNotesLoader::matches(const QModelIndex &index, const ItemFilter &filter) const
{
    const QString text = index.data(contentType::notes).toString();
    return filter.matches(text) || filter.matches(accentsRemoved(text));
}
