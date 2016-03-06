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

#ifdef HAS_TESTS
#   include "tests/itemtagstests.h"
#endif

#include <QBoxLayout>
#include <QColorDialog>
#include <QLabel>
#include <QModelIndex>
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
    styleSheet,
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
            .arg(color.alpha());
}

QColor deserializeColor(const QString &colorName)
{
    if ( colorName.startsWith("rgba(") ) {
        QStringList list = colorName.mid(5, colorName.indexOf(')') - 5).split(',');
        int r = list.value(0).toInt();
        int g = list.value(1).toInt();
        int b = list.value(2).toInt();
        int a = list.value(3).toInt();

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

QVariant cellWidgetProperty(QTableWidget *table, int row, int column, const char *property)
{
    return table->cellWidget(row, column)->property(property);
}

QString tags(const QModelIndex &index)
{
    const QByteArray tagsData =
            index.data(contentType::data).toMap().value(mimeTags).toByteArray();
    return getTextData(tagsData);
}

QString toScriptString(const QString &text)
{
    return "decodeURIComponent('" + QUrl::toPercentEncoding(text) + "')";
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

    const QString tagString = toScriptString(tagName);

    c = dummyTagCommand();
    c.name = ItemTagsLoader::tr("Tag as %1").arg(quoteString(tagName));
    c.matchCmd = "copyq: plugins.itemtags.hasTag(" + tagString + ") && fail()";
    c.cmd = "copyq: plugins.itemtags.tag(" + tagString + ")";
    commands->append(c);

    c = dummyTagCommand();
    c.name = ItemTagsLoader::tr("Remove tag %1").arg(quoteString(tagName));
    c.matchCmd = "copyq: plugins.itemtags.hasTag(" + tagString + ") || fail()";
    c.cmd = "copyq: plugins.itemtags.untag(" + tagString + ")";
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

void addTagButtons(QBoxLayout *layout, const ItemTags::Tags &tags)
{
    Q_ASSERT(layout->parentWidget());

    QFont font = layout->parentWidget()->font();
    if (font.pixelSize() != -1)
        font.setPixelSize(0.75 * font.pixelSize());
    else
        font.setPointSizeF(0.75 * font.pointSizeF());

    const int fontHeight = QFontMetrics(font).height();
    const QString radius = QString::number(fontHeight / 3) + "px";
    const QString borderWidth = QString::number(fontHeight / 8) + "px";

    layout->addStretch(1);

    foreach (const ItemTags::Tag &tag, tags) {
        QLabel *tagWidget = new QLabel(layout->parentWidget());
        layout->addWidget(tagWidget);

        QColor bg(deserializeColor(tag.color));
        const int l = bg.lightness();
        const int fgLightness = (l == 0) ? 200 : l * (l > 100 ? 0.3 : 5.0);
        QColor fg = QColor::fromHsl(
                    bg.hue(), bg.saturation(), qBound(0, fgLightness, 255), bg.alpha());

        if (tag.icon.size() > 1) {
            QPixmap icon(tag.icon);
            tagWidget->setPixmap(icon);
        } else if (tag.icon.size() == 1) {
            tagWidget->setFont(iconFont());
            tagWidget->setText(tag.icon);

            const QSize size = tagWidget->fontMetrics().boundingRect(tag.icon).size();
            const int x = qMax(size.width(), size.height()) + fontHeight / 2;
            tagWidget->setFixedSize(x, x);

            qSwap(fg, bg);
        } else {
            tagWidget->setFont(font);
            tagWidget->setText(tag.name);
        }

        const QColor borderColor = bg.darker(150);
        tagWidget->setAlignment(Qt::AlignCenter);
        tagWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        tagWidget->setStyleSheet(
                    ";background:" + serializeColor(bg) +
                    ";color:" + serializeColor(fg) +
                    ";border: " + borderWidth + " solid " + serializeColor(borderColor) +
                    ";border-radius: " + radius +
                    ";padding: " + borderWidth +
                    ";" + tag.styleSheet
                    );
    }
}

} // namespace

ItemTags::ItemTags(ItemWidget *childItem, const Tags &tags)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_tagWidget(new QWidget(childItem->widget()->parentWidget()))
    , m_childItem(childItem)
{
    QBoxLayout *tagLayout = new QHBoxLayout(m_tagWidget);
    addTagButtons(tagLayout, tags);

    m_childItem->widget()->setObjectName("item_child");
    m_childItem->widget()->setParent(this);

    QBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);

    layout->addWidget(m_tagWidget);
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

void ItemTags::updateSize(const QSize &maximumSize, int idealWidth)
{
    setMaximumSize(maximumSize);

    m_tagWidget->setFixedWidth(idealWidth);

    if ( !m_childItem.isNull() )
        m_childItem->updateSize(maximumSize, idealWidth);

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
                    cellWidgetProperty(t, row, tagsTableColumns::color, propertyColor).value<QColor>();
            tag.color = serializeColor(color);
            tag.icon = cellWidgetProperty(t, row, tagsTableColumns::icon, "currentIcon").toString();
            tag.styleSheet = t->item(row, tagsTableColumns::styleSheet)->text();
            tags.append(serializeTag(tag));
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
        Tag tag = deserializeTag(tagField);
        if (!tag.name.isEmpty())
            m_tags.append(tag);
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
    setHeaderSectionResizeMode(t, tagsTableColumns::styleSheet, QHeaderView::Stretch);
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

QObject *ItemTagsLoader::tests(const TestInterfacePtr &test) const
{
#ifdef HAS_TESTS
    QStringList tags;

    foreach (const QString &tagName, ItemTagsTests::testTags()) {
        Tag tag;
        tag.name = tagName;
        tags.append(serializeTag(tag));
    }

    QVariantMap settings;
    settings[configTags] = tags;

    QObject *tests = new ItemTagsTests(test);
    tests->setProperty("CopyQ_test_settings", settings);
    return tests;
#else
    Q_UNUSED(test);
    return NULL;
#endif
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
            + ";;" + escapeTagField(tag.icon)
            + ";;" + escapeTagField(tag.styleSheet);
}

ItemTagsLoader::Tag ItemTagsLoader::deserializeTag(const QString &tagText)
{
    QStringList tagFields = tagText.split(";;");

    Tag tag;
    tag.name = unescapeTagField(tagFields.value(0));
    tag.color = unescapeTagField(tagFields.value(1));
    tag.icon = unescapeTagField(tagFields.value(2));
    tag.styleSheet = unescapeTagField(tagFields.value(3));

    return tag;
}

ItemTagsLoader::Tags ItemTagsLoader::toTags(const QString &tagsContent)
{
    Tags tags;

    foreach (const QString &tagText, tagsContent.split(',', QString::SkipEmptyParts)) {
        QString tagName = tagText.trimmed();
        bool userTagFound = false;

        foreach (const Tag &userTag, m_tags) {
            const QRegExp re(userTag.name);
            if (re.exactMatch(tagName)) {
                tags.append(userTag);
                tags.last().name = tagName;
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
    t->setItem( row, tagsTableColumns::styleSheet, new QTableWidgetItem(tag.styleSheet) );
    t->setItem( row, tagsTableColumns::color, new QTableWidgetItem() );
    t->setItem( row, tagsTableColumns::icon, new QTableWidgetItem() );

    QPushButton *colorButton = new QPushButton(t);
    setColorIcon(colorButton, deserializeColor(tag.color));
    connect(colorButton, SIGNAL(clicked()), SLOT(onColorButtonClicked()));
    t->setCellWidget(row, tagsTableColumns::color, colorButton);

    IconSelectButton *iconButton = new IconSelectButton(t);
    iconButton->setCurrentIcon(tag.icon);
    t->setCellWidget(row, tagsTableColumns::icon, iconButton);
}

Q_EXPORT_PLUGIN2(itemtags, ItemTagsLoader)
