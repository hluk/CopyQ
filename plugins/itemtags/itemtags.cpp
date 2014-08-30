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

#include "itemtags.h"
#include "ui_itemtagssettings.h"

#include "common/command.h"
#include "common/common.h"
#include "common/contenttype.h"
#include "gui/iconfont.h"
#include "gui/iconselectbutton.h"

#include <QBoxLayout>
#include <QColorDialog>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QLabel>
#include <QModelIndex>
#include <QMouseEvent>
#include <QResource>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QtPlugin>

#if QT_VERSION < 0x050000
#   include <QtWebKit/QWebFrame>
#   include <QtWebKit/QWebPage>
#   include <QtWebKit/QWebView>
#else
#   include <QtWebKitWidgets/QWebFrame>
#   include <QtWebKitWidgets/QWebPage>
#   include <QtWebKitWidgets/QWebView>
#endif

namespace {

const char mimeTags[] = "application/x-copyq-tags";

const int notesIndent = 16;

const char defaultStyleSheetData[] =
        "* {"
            ";padding: 0"
            ";margin: 0"
        "}"
        ".tag {"
            ";background: rgba(240, 240, 240, 0.5)"
            ";color: rgba(50, 50, 50, 0.6)"
            ";border: 1pt solid rgba(50, 50, 50, 0.25)"
            ";border-radius: 2pt"
            ";padding: 1pt 2pt 1pt 2pt"
            ";margin: 2pt"
            ";float: right"
        "}"
        ;

const char configTags[] = "tags";

const char propertyColor[] = "CopyQ_color";

namespace tagsTableColumns {
enum {
    name,
    color,
    icon
};
}

QString serializeColor(const QColor &color)
{
    if (color.alpha() == 255)
        return color.name();

    return QString("rgba(%1,%2,%3,%4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.alpha() * 1.0 / 255);
}

void setColorIcon(QPushButton *button, const QColor &color)
{
    QPixmap pix(button->iconSize());
    pix.fill(color);
    button->setIcon(pix);
    button->setProperty( propertyColor, serializeColor(color) );
}

void setHeaderSectionResizeMode(QTableWidget *table, int logicalIndex, QHeaderView::ResizeMode mode)
{
#if QT_VERSION < 0x050000
    table->horizontalHeader()->setResizeMode(logicalIndex, mode);
#else
    table->horizontalHeader()->setSectionResizeMode(logicalIndex, mode);
#endif
}

void setFixedColumnSize(QTableWidget *table, int logicalIndex)
{
    setHeaderSectionResizeMode(table, logicalIndex, QHeaderView::Fixed);
    table->horizontalHeader()->resizeSection(logicalIndex, table->rowHeight(0));
}

QString cellWidgetProperty(QTableWidget *table, int row, int column, const char *property)
{
    return table->cellWidget(row, column)->property(property).toString();
}

QString tags(const QModelIndex &index)
{
    const QByteArray tagsData =
            index.data(contentType::data).toMap().value(mimeTags).toByteArray();
    return QString::fromUtf8(tagsData);
}

QString defaultStyleSheetUrl()
{
    return
        "data:text/css;charset=utf-8;base64,"
        + QString::fromUtf8(QByteArray(defaultStyleSheetData).toBase64());
}

} // namespace

ItemTags::ItemTags(ItemWidget *childItem, const QString &tagsContent)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_tags( new QWebView(this) )
    , m_childItem(childItem)
{
    QWebFrame *frame = m_tags->page()->mainFrame();
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    const QFont &defaultFont = m_childItem->widget()->font();
    m_tags->settings()->setFontFamily(QWebSettings::StandardFont, defaultFont.family());
    // DPI resolution can be different than the one used by this widget.
    QWidget* window = QApplication::desktop()->screen();
    const int dpi = window->logicalDpiX();
    const int pt = defaultFont.pointSize() * 0.8;
    m_tags->settings()->setFontSize(QWebSettings::DefaultFontSize, pt * dpi / 72);
    m_tags->settings()->setUserStyleSheetUrl(QUrl(defaultStyleSheetUrl()));

    QPalette pal(palette());
    pal.setBrush(QPalette::Base, Qt::transparent);
    m_tags->page()->setPalette(pal);
    m_tags->setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_tags->setContextMenuPolicy(Qt::NoContextMenu);

    // Open link with external application.
    m_tags->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect( m_tags->page(), SIGNAL(linkClicked(QUrl)), SLOT(onLinkClicked(QUrl)) );

    // Set some remote URL as base URL so we can include remote scripts.
    m_tags->setHtml(tagsContent, QUrl("http://example.com/"));

    m_tags->setProperty("CopyQ_no_style", true);
    m_tags->setObjectName("item_child");

    m_childItem->widget()->setObjectName("item_child");
    m_childItem->widget()->setParent(this);

    QBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);

    layout->addWidget(m_tags);
    layout->addWidget( m_childItem->widget() );
}

void ItemTags::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_childItem->setHighlight(re, highlightFont, highlightPalette);
}

QWidget *ItemTags::createEditor(QWidget *parent) const
{
    return m_childItem.isNull() ? NULL : m_childItem->createEditor(parent);
}

void ItemTags::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->setEditorData(editor, index);
}

void ItemTags::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->setModelData(editor, model, index);
}

bool ItemTags::hasChanges(QWidget *editor) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->hasChanges(editor);
}

QObject *ItemTags::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    return m_childItem ? m_childItem->createExternalEditor(index, parent)
                       : ItemWidget::createExternalEditor(index, parent);
}

void ItemTags::updateSize(const QSize &maximumSize)
{
    QWebPage *page = m_tags->page();
    QWebFrame *frame = page->mainFrame();
    disconnect( frame, SIGNAL(contentsSizeChanged(QSize)),
                this, SLOT(onItemChanged()) );

    setMaximumSize(maximumSize);
    m_maximumSize = maximumSize;

    const int w = maximumSize.width();
    page->setPreferredContentsSize( QSize(w, 10) );
    const int h = frame->contentsSize().height();

    const QSize size(w, h);
    page->setViewportSize(size);
    m_tags->setFixedSize(size);

    if ( !m_childItem.isNull() )
        m_childItem->updateSize(maximumSize);

    adjustSize();

    connect( frame, SIGNAL(contentsSizeChanged(QSize)),
             this, SLOT(onItemChanged()) );
}

void ItemTags::onItemChanged()
{
    updateSize(m_maximumSize);
}

void ItemTags::onLinkClicked(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

ItemTagsLoader::ItemTagsLoader()
{
}

ItemTagsLoader::~ItemTagsLoader()
{
}

QStringList ItemTagsLoader::formatsToSave() const
{
    return QStringList(mimeTags);
}

QVariantMap ItemTagsLoader::applySettings()
{
    m_tags.clear();

    QTableWidget *t = ui->tableWidget;
    QStringList tags;

    for (int row = 0; row < t->rowCount(); ++row) {
        Tag tag;
        const QString tagName = t->item(row, tagsTableColumns::name)->text();
        if ( !tagName.isEmpty() ) {
            tag.color = cellWidgetProperty(t, row, tagsTableColumns::color, propertyColor);
            tag.icon = cellWidgetProperty(t, row, tagsTableColumns::icon, "currentIcon");
            tags.append(tagName + ";;" + tag.color + ";;" + tag.icon);
            m_tags[tagName.toLower()] = tag;
        }
    }

    m_settings.insert(configTags, tags);

    return m_settings;
}

void ItemTagsLoader::loadSettings(const QVariantMap &settings)
{
    m_settings = settings;

    m_tags.clear();
    foreach (const QString &tag, m_settings.value(configTags).toStringList()) {
        const QStringList values = tag.split(";;");
        const QString tagName = values.value(tagsTableColumns::name);
        if (!tagName.isEmpty()) {
            Tag tag;
            tag.color = values.value(tagsTableColumns::color);
            tag.icon = values.value(tagsTableColumns::icon);
            m_tags[tagName.toLower()] = tag;
        }
    }
}

QWidget *ItemTagsLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemTagsSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    // Init tag table.
    foreach (const QString &tagName, m_tags.keys())
        addTagToSettingsTable(tagName, m_tags[tagName]);
    for (int i = 0; i < 10; ++i)
        addTagToSettingsTable();

    QTableWidget *t = ui->tableWidget;
    setHeaderSectionResizeMode(t, tagsTableColumns::name, QHeaderView::Stretch);
    setFixedColumnSize(t, tagsTableColumns::color);
    setFixedColumnSize(t, tagsTableColumns::icon);

    return w;
}

ItemWidget *ItemTagsLoader::transform(ItemWidget *itemWidget, const QModelIndex &index)
{
    const QString tagsContent = tags(index);
    if ( tagsContent.isEmpty() )
        return NULL;

    return new ItemTags(itemWidget, generateTagsHtml(tagsContent));
}

bool ItemTagsLoader::matches(const QModelIndex &index, const QRegExp &re) const
{
    return re.indexIn(tags(index)) != -1;
}

QString ItemTagsLoader::script() const
{
    return
        "plugins." + id() + " = {"

        "\n" "  mime: '" + QString(mimeTags) + "',"

        "\n" "tagUntag: function(tagName, add) {"
        "\n" "  if (!tagName)"
        "\n" "    tagName = dialog('Tag')"
        "\n" "  if (!tagName)"
        "\n" "    return;"
        "\n" "  "
        "\n" "  tab(selectedtab())"
        "\n" "  selected = selecteditems()"
        "\n" "  "
        "\n" "  for (var i in selected) {"
        "\n" "    row = selected[i]"
        "\n" "    tags = str(read(this.mime, row))"
        "\n" "      .split(/\\s*,\\s*/)"
        "\n" "      .filter(function(x) {return x != tagName;})"
        "\n" "    if (add)"
        "\n" "      tags = tags.concat(tagName)"
        "\n" "    tags = tags.sort().join(',')"
        "\n" "    change(row, this.mime, tags)"
        "\n" "  }"
        "\n" "},"

        "\n" "tag: function(tagName) {"
        "\n" "  this.tagUntag(tagName, true)"
        "\n" "},"

        "\n" "removeTag: function(tagName) {"
        "\n" "  this.tagUntag(tagName, false)"
        "\n" "},"

        "\n" "clearTags: function() {"
        "\n" "  tab(selectedtab())"
        "\n" "  selected = selecteditems()"
        "\n" "  for (var i in selected)"
        "\n" "    change(selected[i], this.mime, '')"
        "\n" "},"

        "\n" "}";
}

void ItemTagsLoader::addCommands(QList<Command> *commands) const
{
    const QString tagName = tr("Important", "Tag name for example command");

    Command c;
    c.icon = QString(QChar(IconTag));
    c.inMenu = true;

    c.name = tr("Tag as %1").arg(quoteString(tagName));
    c.cmd = "copyq: plugins.itemtags.tag('" + tagName + "')";
    commands->append(c);

    c.name = tr("Remove tag %1").arg(quoteString(tagName));
    c.cmd = "copyq: plugins.itemtags.removeTag('" + tagName + "')";
    commands->append(c);

    c.name = tr("Clear all tags");
    c.cmd = "copyq: plugins.itemtags.clearTags()";
    commands->append(c);
}

void ItemTagsLoader::onColorButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    Q_ASSERT(button);

    QColor color(button->property(propertyColor).toString());
    QColorDialog dialog(button->window());
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel);
    dialog.setCurrentColor(color);

    if ( dialog.exec() == QDialog::Accepted )
        setColorIcon( button, dialog.selectedColor() );
}

QString ItemTagsLoader::generateTagsHtml(const QString &tagsContent)
{
    if (tagsContent.startsWith('<'))
        return tagsContent;

    QString html;
    foreach (const QString &tagName, tagsContent.split(',', QString::SkipEmptyParts)) {
        QString tag = tagName.trimmed();
        QString style;

        Tags::const_iterator it = m_tags.find(tag.toLower());
        if (it != m_tags.end()) {
            const QColor bg(it->color);
            const int fgLightness =
                    bg.lightness() == 0 ? 200 : bg.lightness() * (bg.lightness() > 100 ? 0.3 : 5.0);
            const QColor fg = QColor::fromHsl(
                        bg.hue(), bg.saturation(), qBound(0, fgLightness, 255), bg.alpha());

            style = "background:" + bg.name() + ";color:" + fg.name();
            if (!it->icon.isEmpty()) {
                QFont font = iconFont();
                style += QString(";font-family:\"%1\";font-size:%2px")
                        .arg(font.family(), QString::number(font.pixelSize()));
                tag = it->icon;
            }
        }

        html.append("<span class='tag' style='" + style + "'>" + escapeHtml(tag) + "</span>");
    }

    return html;
}

void ItemTagsLoader::addTagToSettingsTable(const QString &tagName, const ItemTagsLoader::Tag &tag)
{
    QTableWidget *t = ui->tableWidget;

    const int row = t->rowCount();

    t->insertRow(row);
    t->setItem( row, tagsTableColumns::name, new QTableWidgetItem(tagName) );
    t->setItem( row, tagsTableColumns::color, new QTableWidgetItem() );
    t->setItem( row, tagsTableColumns::icon, new QTableWidgetItem() );

    QPushButton *colorButton = new QPushButton(t);
    setColorIcon(colorButton, tag.color);
    connect(colorButton, SIGNAL(clicked()), SLOT(onColorButtonClicked()));
    t->setCellWidget(row, tagsTableColumns::color, colorButton);

    IconSelectButton *iconButton = new IconSelectButton(t);
    iconButton->setCurrentIcon(tag.icon);
    t->setCellWidget(row, tagsTableColumns::icon, iconButton);
}

Q_EXPORT_PLUGIN2(itemtags, ItemTagsLoader)
