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
#include <QLabel>
#include <QModelIndex>
#include <QPainter>
#include <QPushButton>
#include <QtPlugin>
#include <QUrl>

namespace {

const char mimeTags[] = "application/x-copyq-tags";

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

QColor deserializeColor(const QString &colorName)
{
    if ( colorName.startsWith("rgba(") ) {
        QStringList list = colorName.mid(5, colorName.indexOf(')') - 5).split(',');
        int r = list.value(0).toInt();
        int g = list.value(1).toInt();
        int b = list.value(2).toInt();
        int a = list.value(3).toDouble() * 255;

        return QColor(r, g, b, a);
    }

    return QColor(colorName);
}

void setColorIcon(QPushButton *button, const QColor &color)
{
    QPixmap pix(button->iconSize());
    pix.fill(color);
    button->setIcon(pix);
    button->setProperty(propertyColor, color);
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

QString addTagText()
{
    return ItemTagsLoader::tr("Add a Tag");
}

QString removeTagText()
{
    return ItemTagsLoader::tr("Remove a Tag");
}

Command dummyTagCommand()
{
    Command c;
    c.icon = QString(QChar(IconTag));
    c.inMenu = true;
    return c;
}

void addTagCommands(const QString &tagName, QList<Command> *commands)
{
    Command c;

    c = dummyTagCommand();
    c.name = ItemTagsLoader::tr("Tag as %1").arg(quoteString(tagName));
    c.matchCmd = "copyq: plugins.itemtags.hasTag('" + tagName + "') && fail()";
    c.cmd = "copyq: plugins.itemtags.tag('" + tagName + "')";
    commands->append(c);

    c = dummyTagCommand();
    c.name = ItemTagsLoader::tr("Remove tag %1").arg(quoteString(tagName));
    c.matchCmd = "copyq: plugins.itemtags.hasTag('" + tagName + "') || fail()";
    c.cmd = "copyq: plugins.itemtags.untag('" + tagName + "')";
    commands->append(c);
}

QString escapeTagField(const QString &field)
{
    return QString(field).replace("\\", "\\\\").replace(";;", ";\\;");
}

QString unescapeTagField(const QString &field)
{
    return QString(field).replace(";\\;", ";;").replace("\\\\", "\\");
}

QPixmap renderTags(const ItemTags::Tags &tags, const QFont &font)
{
    const QFont iconFont = ::iconFont();
    const QFontMetrics fm(font);
    const QFontMetrics iconFm(iconFont);
    const int wordSpacing = qMax(iconFont.wordSpacing(), font.wordSpacing());
    const int fontHeight = qMax(iconFm.height(), fm.height());
    const int padding = 2 * (wordSpacing + 1);
    const int tagMargin = 2 * padding;
    const int h = fontHeight + 2 * padding;

    QPixmap pix(2048, h);
    pix.fill(Qt::transparent);

    QPainter p(&pix);

    p.translate(tagMargin, 0);
    int w = tagMargin;
    int maxHeight = 0;

    foreach (const ItemTags::Tag &tag, tags) {
        QColor bg(deserializeColor(tag.color));
        const int l = bg.lightness();
        const int fgLightness = (l == 0) ? 200 : l * (l > 100 ? 0.3 : 5.0);
        QColor fg = QColor::fromHsl(
                    bg.hue(), bg.saturation(), qBound(0, fgLightness, 255), bg.alpha());

        if (tag.icon.size() > 1) {
            QPixmap icon(tag.icon);
            const int scaledHeight = qMin(h - 2 * padding, icon.height());
            icon.scaledToHeight(scaledHeight, Qt::SmoothTransformation);
            const QRect rect = icon.rect().translated(padding, padding);
            p.drawPixmap(rect, icon);
            p.translate(icon.width(), 0);
            w += icon.width();
            maxHeight = qMax(maxHeight, icon.height() + 2 * padding);
        } else {
            QString text;

            if (tag.icon.size() == 1) {
                p.setFont(iconFont);
                text = tag.icon;
                qSwap(fg, bg);
            } else {
                p.setFont(font);
                text = tag.name;
            }

            QRect rect = p.fontMetrics().boundingRect(text);
            const int pad = 2 * (p.font().wordSpacing() + 1);
            rect.adjust(0, 0, 2 * pad, 2 * pad);
            rect.moveTo(0, (h - rect.height()) / 2);
            maxHeight = qMax(maxHeight, rect.height());

            p.fillRect(rect, bg);

            p.setPen(fg);
            p.drawText(rect, Qt::AlignCenter, text);

            p.translate(rect.right(), 0);
            w += rect.width();
        }

        p.translate(tagMargin, 0);
        w += tagMargin;
    }

    return pix.copy(0, (pix.height() - maxHeight) / 2, w, maxHeight);
}

QString toScriptString(const QString &text)
{
    return "decodeURIComponent('" + QUrl::toPercentEncoding(text) + "')";
}

} // namespace

ItemTags::ItemTags(ItemWidget *childItem, const Tags &tags)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_tagsLabel(new QLabel(this))
    , m_childItem(childItem)
{
    QFont tagFont = font();
    if (tagFont.pixelSize() != -1)
        tagFont.setPixelSize(0.75 * tagFont.pixelSize());
    else
        tagFont.setPointSizeF(0.75 * tagFont.pointSizeF());

    const QPixmap pix = renderTags(tags, tagFont);
    m_tagsLabel->setPixmap(pix);
    m_tagsLabel->setFixedSize(pix.size());
    m_tagsLabel->setContentsMargins(0, 0, 0, 0);

    m_childItem->widget()->setObjectName("item_child");
    m_childItem->widget()->setParent(this);

    QBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);

    layout->addWidget(m_tagsLabel, 0, Qt::AlignRight);
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
    setMaximumSize(maximumSize);

    if ( !m_childItem.isNull() )
        m_childItem->updateSize(maximumSize);

    adjustSize();
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
        tag.name = t->item(row, tagsTableColumns::name)->text();
        if ( !tag.name.isEmpty() ) {
            const QColor color =
                    cellWidgetProperty(t, row, tagsTableColumns::color, propertyColor);
            tag.color = serializeColor(color);
            tag.icon = cellWidgetProperty(t, row, tagsTableColumns::icon, "currentIcon");
            tags.append(tag.name + ";;" + tag.color + ";;" + tag.icon);
            m_tags.append(tag);
        }
    }

    m_settings.insert(configTags, tags);

    return m_settings;
}

void ItemTagsLoader::loadSettings(const QVariantMap &settings)
{
    m_settings = settings;

    m_tags.clear();
    foreach (const QString &tagField, m_settings.value(configTags).toStringList()) {
        Tag tag;
        const QStringList values = tagField.split(";;");
        tag.name = values.value(0);
        if (!tag.name.isEmpty()) {
            tag.color = values.value(1);
            tag.icon = values.value(2);
            m_tags.append(tag);
        }
    }
}

QWidget *ItemTagsLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemTagsSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    // Init tag table.
    foreach (const Tag &tag, m_tags)
        addTagToSettingsTable(tag);
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
    const Tags tags = toTags(tagsContent);
    if ( tags.isEmpty() )
        return NULL;

    return new ItemTags(itemWidget, tags);
}

bool ItemTagsLoader::matches(const QModelIndex &index, const QRegExp &re) const
{
    return re.indexIn(tags(index)) != -1;
}

QString ItemTagsLoader::script() const
{
    QString userTags;
    foreach (const Tag &tag, m_tags)
        userTags.append(toScriptString(tag.name) + ",");

    const QString addTagString = toScriptString(addTagText());
    const QString removeTagString = toScriptString(removeTagText());

    return
        "plugins." + id() + " = {"

        "\n" "mime: '" + QString(mimeTags) + "',"
        "\n" "userTags: [" + userTags + "],"

        "\n" "tags: function(row) {"
        "\n" "  return str(read(this.mime, row))"
        "\n" "    .split(/\\s*,\\s*/)"
        "\n" "    .filter(function(x) {return x != ''})"
        "\n" "},"

        "\n" "_rowsOrSelected: function(args) {"
        "\n" "  if (args.length > 1)"
        "\n" "    return Array.prototype.slice.call(args, 1)"
        "\n" "  return selecteditems()"
        "\n" "},"

        "\n" "_tagUntag: function(add, args) {"
        "\n" "  var tagName = args[0]"
        "\n" "  if (!tagName) {"
        "\n" "    title = add ? " + addTagString + " : " + removeTagString +
        "\n" "    tagName = dialog('.title', title, 'Tag', this.userTags)"
        "\n" "    if (!tagName)"
        "\n" "      return;"
        "\n" "  }"
        "\n" "  "
        "\n" "  var rows = this._rowsOrSelected(args)"
        "\n" "  for (var i in rows) {"
        "\n" "    var row = rows[i]"
        "\n" "    tags = this.tags(row)"
        "\n" "      .filter(function(x) {return x != tagName})"
        "\n" "    if (add)"
        "\n" "      tags = tags.concat(tagName)"
        "\n" "    tags = tags.sort().join(',')"
        "\n" "    change(row, this.mime, tags)"
        "\n" "  }"
        "\n" "},"

        "\n" "tag: function(tagName) {"
        "\n" "  this._tagUntag(true, arguments)"
        "\n" "},"

        "\n" "untag: function(tagName) {"
        "\n" "  this._tagUntag(false, arguments)"
        "\n" "},"

        "\n" "clearTags: function() {"
        "\n" "  var rows = Array.prototype.slice.call(arguments)"
        "\n" "  if (rows.length == 0)"
        "\n" "    rows = selecteditems()"
        "\n" "  for (var i in rows)"
        "\n" "    change(rows[i], this.mime, '')"
        "\n" "},"

        "\n" "hasTag: function(tagName) {"
        "\n" "  var rows = this._rowsOrSelected(arguments)"
        "\n" "  if (tagName) {"
        "\n" "    for (var i in rows) {"
        "\n" "      if (this.tags(rows[i]).indexOf(tagName) != -1)"
        "\n" "        return true"
        "\n" "    }"
        "\n" "  } else {"
        "\n" "    for (var i in rows) {"
        "\n" "      if (this.tags(rows[i]).length != 0)"
        "\n" "        return true"
        "\n" "    }"
        "\n" "  }"
        "\n" "  return false"
        "\n" "},"

        "\n" "}";
}

QList<Command> ItemTagsLoader::commands() const
{
    QList<Command> commands;

    if (m_tags.isEmpty()) {
        addTagCommands(tr("Important", "Tag name for example command"), &commands);
    } else {
        foreach (const Tag &tag, m_tags)
            addTagCommands(tag.name, &commands);
    }

    Command c;

    c = dummyTagCommand();
    c.name = addTagText();
    c.matchCmd = "copyq: plugins.itemtags.tag()";
    c.cmd.clear();
    commands.append(c);

    c = dummyTagCommand();
    c.name = removeTagText();
    c.matchCmd = "copyq: plugins.itemtags.untag()";
    c.cmd.clear();
    commands.append(c);

    c = dummyTagCommand();
    c.name = tr("Clear all tags");
    c.matchCmd = "copyq: plugins.itemtags.hasTag() || fail()";
    c.cmd = "copyq: plugins.itemtags.clearTags()";
    commands.append(c);

    return commands;
}

void ItemTagsLoader::onColorButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    Q_ASSERT(button);

    const QColor color = button->property(propertyColor).value<QColor>();
    QColorDialog dialog(button->window());
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel);
    dialog.setCurrentColor(color);

    if ( dialog.exec() == QDialog::Accepted )
        setColorIcon( button, dialog.selectedColor() );
}

QString ItemTagsLoader::serializeTag(const ItemTagsLoader::Tag &tag)
{
    return escapeTagField(tag.name)
            + ";;" + escapeTagField(tag.color)
            + ";;" + escapeTagField(tag.icon);
}

ItemTagsLoader::Tag ItemTagsLoader::deserializeTag(const QString &tagText)
{
    QStringList tagFields = tagText.split(";;");

    Tag tag;
    tag.name = unescapeTagField(tagFields.value(0));
    tag.color = unescapeTagField(tagFields.value(1));
    tag.icon = unescapeTagField(tagFields.value(2));

    return tag;
}

ItemTagsLoader::Tags ItemTagsLoader::toTags(const QString &tagsContent)
{
    Tags tags;

    foreach (const QString &tagText, tagsContent.split(',', QString::SkipEmptyParts)) {
        QString tagName = tagText.trimmed();
        bool userTagFound = false;

        foreach (const Tag &userTag, m_tags) {
            if (userTag.name == tagName) {
                tags.append(userTag);
                userTagFound = true;
                break;
            }
        }

        if (!userTagFound) {
            Tag tag;
            tag.name = tagName;
            tag.color = "white";
            tags.append(tag);
        }
    }

    return tags;
}

void ItemTagsLoader::addTagToSettingsTable(const ItemTagsLoader::Tag &tag)
{
    QTableWidget *t = ui->tableWidget;

    const int row = t->rowCount();

    t->insertRow(row);
    t->setItem( row, tagsTableColumns::name, new QTableWidgetItem(tag.name) );
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
