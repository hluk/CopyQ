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
#include "common/contenttype.h"
#include "common/textdata.h"
#include "gui/iconfont.h"
#include "gui/iconselectbutton.h"

#ifdef HAS_TESTS
#   include "tests/itemtagstests.h"
#endif

#include <QBoxLayout>
#include <QColorDialog>
#include <QLabel>
#include <QModelIndex>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QtPlugin>
#include <QUrl>

Q_DECLARE_METATYPE(ItemTags::Tag)

namespace {

const char mimeTags[] = "application/x-copyq-tags";

const char configTags[] = "tags";

const char propertyColor[] = "CopyQ_color";

namespace tagsTableColumns {
enum {
    name,
    match,
    styleSheet,
    color,
    icon
};
}

class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        QFontMetrics fm = fontMetrics();
        const QString elidedText = fm.elidedText(text(), Qt::ElideMiddle, rect().width());
        p.drawText(rect(), Qt::AlignCenter, elidedText);
    }
};

bool isTagValid(const ItemTags::Tag &tag)
{
    return !tag.name.isEmpty()
            || !tag.icon.isEmpty()
            || !tag.styleSheet.isEmpty()
            || !tag.match.isEmpty();
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

QStringList tags(const QVariant &tags)
{
    return getTextData( tags.toByteArray() )
            .split(',', QString::SkipEmptyParts);
}

QStringList tags(const QVariantMap &itemData)
{
    return tags( itemData.value(mimeTags) );
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

void addTagCommands(const QString &tagName, const QString &match, QVector<Command> *commands)
{
    Command c;

    const QString name = !tagName.isEmpty() ? tagName : match;
    const QString tagString = toScriptString(name);

    c = dummyTagCommand();
    c.name = ItemTagsLoader::tr("Tag as %1").arg(quoteString(name));
    c.matchCmd = "copyq: plugins.itemtags.hasTag(" + tagString + ") && fail()";
    c.cmd = "copyq: plugins.itemtags.tag(" + tagString + ")";
    commands->append(c);

    c = dummyTagCommand();
    c.name = ItemTagsLoader::tr("Remove tag %1").arg(quoteString(name));
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

void initTagWidget(QWidget *tagWidget, const ItemTags::Tag &tag, const QFont &font)
{
    tagWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    tagWidget->setStyleSheet(
                "* {"
                ";background:transparent"
                ";color:" + serializeColor(tag.color) +
                ";" + tag.styleSheet +
                "}"
                "QLabel {"
                ";background:transparent"
                ";border:none"
                "}"
            );

    auto layout = new QHBoxLayout(tagWidget);
    const int x = QFontMetrics(font).height() / 6;
    layout->setContentsMargins(x, x, x, x);
    layout->setSpacing(x * 2);

    if (tag.icon.size() > 1) {
        QLabel *iconLabel = new QLabel(tagWidget);
        const QPixmap icon(tag.icon);
        iconLabel->setPixmap(icon);
        layout->addWidget(iconLabel);
    } else if (tag.icon.size() == 1) {
        QLabel *iconLabel = new QLabel(tagWidget);
        iconLabel->setFont(iconFont());
        iconLabel->setText(tag.icon);
        layout->addWidget(iconLabel);
    }

    if (!tag.name.isEmpty()) {
        auto label = new ElidedLabel(tag.name, tagWidget);
        label->setFont(font);
        layout->addWidget(label);
    }
}

QFont smallerFont(QFont font)
{
    if (font.pixelSize() != -1)
        font.setPixelSize( static_cast<int>(0.75 * font.pixelSize()) );
    else
        font.setPointSizeF(0.75 * font.pointSizeF());

    return font;
}

void addTagButtons(QBoxLayout *layout, const ItemTags::Tags &tags)
{
    Q_ASSERT(layout->parentWidget());

    layout->addStretch(1);

    const QFont font = smallerFont(layout->parentWidget()->font());

    for (const auto &tag : tags) {
        QWidget *tagWidget = new QWidget(layout->parentWidget());
        initTagWidget(tagWidget, tag, font);
        layout->addWidget(tagWidget);
    }
}

ItemTags::Tag findMatchingTag(const QString &tagText, const ItemTags::Tags &tags)
{
    for (const auto &tag : tags) {
        if ( tag.match.isEmpty() ) {
            if (tag.name == tagText)
                return tag;
        } else {
            const QRegExp re(tag.match);
            if ( re.exactMatch(tagText) )
                return tag;
        }
    }

    return ItemTags::Tag();
}

class TagTableWidgetItem : public QTableWidgetItem
{
public:
    enum {
        TagRole = Qt::UserRole
    };

    explicit TagTableWidgetItem(const QString &text)
        : QTableWidgetItem(text)
    {
    }

    QVariant data(int role) const override
    {
        if (role == Qt::DecorationRole)
            return m_pixmap;

        return QTableWidgetItem::data(role);
    }

    void setData(int role, const QVariant &value) override
    {
        if (role == TagRole)
            setTag( value.value<ItemTags::Tag>() );

        QTableWidgetItem::setData(role, value);
    }

private:
    void setTag(const ItemTags::Tag &tag)
    {
        if ( isTagValid(tag) ) {
            QWidget tagWidget;
            initTagWidget(&tagWidget, tag, smallerFont(QFont()));
#if QT_VERSION < 0x050000
            m_pixmap = QPixmap( tagWidget.sizeHint() );
#else
            const auto ratio = tagWidget.devicePixelRatio();
            m_pixmap = QPixmap( tagWidget.sizeHint() * ratio );
            m_pixmap.setDevicePixelRatio(ratio);
#endif
            m_pixmap.fill(Qt::transparent);
            QPainter painter(&m_pixmap);
            tagWidget.render(&painter);
        } else {
            m_pixmap = QPixmap();
        }
    }

    QPixmap m_pixmap;
};

} // namespace

ItemTags::ItemTags(ItemWidget *childItem, const Tags &tags)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_tagWidget(new QWidget(childItem->widget()->parentWidget()))
    , m_childItem(childItem)
{
    QBoxLayout *tagLayout = new QHBoxLayout(m_tagWidget);
    tagLayout->setMargin(0);
    addTagButtons(tagLayout, tags);

    m_childItem->widget()->setObjectName("item_child");
    m_childItem->widget()->setParent(this);

    QBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);

    layout->addWidget(m_tagWidget);
    layout->addWidget( m_childItem->widget() );
}

void ItemTags::setCurrent(bool current)
{
    m_childItem->setCurrent(current);
}

void ItemTags::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_childItem->setHighlight(re, highlightFont, highlightPalette);
}

QWidget *ItemTags::createEditor(QWidget *parent) const
{
    return m_childItem->createEditor(parent);
}

void ItemTags::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    m_childItem->setEditorData(editor, index);
}

void ItemTags::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    m_childItem->setModelData(editor, model, index);
}

bool ItemTags::hasChanges(QWidget *editor) const
{
    return m_childItem->hasChanges(editor);
}

QObject *ItemTags::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    return m_childItem->createExternalEditor(index, parent);
}

void ItemTags::updateSize(QSize maximumSize, int idealWidth)
{
    setMaximumSize(maximumSize);

    m_tagWidget->adjustSize();
    m_tagWidget->setFixedWidth(idealWidth);
    setFixedWidth(idealWidth);

    m_childItem->updateSize(maximumSize, idealWidth);

    // For some child widgets, adjustSize() doesn't properly set minimum size.
    setFixedHeight(m_childItem->widget()->height() + m_tagWidget->height());
}

QStringList ItemTagsScriptable::getUserTags() const
{
    return m_userTags;
}

QString ItemTagsScriptable::getMimeTags() const
{
    return mimeTags;
}

QStringList ItemTagsScriptable::tags()
{
    const auto args = currentArguments();
    const auto rows = this->rows(args, 0);

    QStringList allTags;
    for (int row : rows)
        allTags << this->tags(row);

    return allTags;
}

void ItemTagsScriptable::tag()
{
    const auto args = currentArguments();

    auto tagName = args.value(0).toString();
    if ( tagName.isEmpty() ) {
        tagName = askTagName( addTagText(), m_userTags );
        if ( tagName.isEmpty() )
            return;
    }

    if ( args.size() <= 1 ) {
        const auto dataValueList = call("selectedItemsData").toList();

        QVariantList dataList;
        dataList.reserve( dataValueList.size() );
        for (const auto &itemDataValue : dataValueList) {
            auto itemData = itemDataValue.toMap();
            auto itemTags = ::tags(itemData);
            if ( addTag(tagName, &itemTags) )
                itemData.insert( mimeTags, itemTags.join(",") );
            dataList.append(itemData);
        }

        call( "setSelectedItemsData", QVariantList() << QVariant(dataList) );
    } else {
        for ( int row : rows(args, 1) ) {
            auto itemTags = tags(row);
            if ( addTag(tagName, &itemTags) )
                setTags(row, itemTags);
        }
    }
}

void ItemTagsScriptable::untag()
{
    const auto args = currentArguments();
    auto tagName = args.value(0).toString();

    if ( args.size() <= 1 ) {
        const auto dataValueList = call("selectedItemsData").toList();

        if ( tagName.isEmpty() ) {
            QStringList allTags;
            for (const auto &itemDataValue : dataValueList) {
                const auto itemData = itemDataValue.toMap();
                allTags.append( ::tags(itemData) );
            }

            tagName = askRemoveTagName(allTags);
            if ( allTags.isEmpty() )
                return;
        }

        QVariantList dataList;
        dataList.reserve( dataValueList.size() );
        for (const auto &itemDataValue : dataValueList) {
            auto itemData = itemDataValue.toMap();
            auto itemTags = ::tags(itemData);
            if ( removeTag(tagName, &itemTags) )
                itemData.insert( mimeTags, itemTags.join(",") );
            dataList.append(itemData);
        }

        call( "setSelectedItemsData", QVariantList() << QVariant(dataList) );
    } else {
        const auto rows = this->rows(args, 1);

        if ( tagName.isEmpty() ) {
            QStringList allTags;
            for (int row : rows)
                allTags.append( this->tags(row) );

            tagName = askRemoveTagName(allTags);
            if ( allTags.isEmpty() )
                return;
        }

        for (int row : rows) {
            auto itemTags = tags(row);
            if ( removeTag(tagName, &itemTags) )
                setTags(row, itemTags);
        }
    }
}

void ItemTagsScriptable::clearTags()
{
    const auto args = currentArguments();

    if ( args.isEmpty() ) {
        const auto dataValueList = call("selectedItemsData").toList();

        QVariantList dataList;
        for (const auto &itemDataValue : dataValueList) {
            auto itemData = itemDataValue.toMap();
            itemData.remove(mimeTags);
            dataList.append(itemData);
        }

        call( "setSelectedItemsData", QVariantList() << QVariant(dataList) );
    } else {
        const auto rows = this->rows(args, 0);
        for (int row : rows)
            setTags(row, QStringList());
    }
}

bool ItemTagsScriptable::hasTag()
{
    const auto args = currentArguments();
    const auto tagName = args.value(0).toString();

    if ( args.size() <= 1 ) {
        const auto dataValueList = call("selectedItemsData").toList();
        for (const auto &itemDataValue : dataValueList) {
            const auto itemData = itemDataValue.toMap();
            const auto itemTags = ::tags(itemData);
            if ( itemTags.contains(tagName) )
                return true;
        }
        return false;
    }

    const auto row = args.value(1).toInt();
    return tags(row).contains(tagName);
}

QString ItemTagsScriptable::askTagName(const QString &dialogTitle, const QStringList &tags)
{
    const auto value = call( "dialog", QVariantList()
          << ".title" << dialogTitle
          << dialogTitle << tags );

    return value.toString();
}

QString ItemTagsScriptable::askRemoveTagName(const QStringList &tags)
{
    if ( tags.isEmpty() )
        return QString();

    if ( tags.size() == 1 )
        return tags.first();

    return askTagName( removeTagText(), tags );
}

QList<int> ItemTagsScriptable::rows(const QVariantList &arguments, int skip)
{
    QList<int> rows;

    for (int i = skip; i < arguments.size(); ++i) {
        bool ok;
        const auto row = arguments[i].toInt(&ok);
        if (ok)
            rows.append(row);
    }

    return rows;
}

QStringList ItemTagsScriptable::tags(int row)
{
    const auto value = call("read", QVariantList() << mimeTags << row);
    return ::tags(value);
}

void ItemTagsScriptable::setTags(int row, const QStringList &tags)
{
    const auto value = tags.join(",");
    call("change", QVariantList() << row << mimeTags << value);
}

bool ItemTagsScriptable::addTag(const QString &tagName, QStringList *tags)
{
    if ( tags->contains(tagName) )
        return false;

    tags->append(tagName);
    tags->sort();
    return true;
}

bool ItemTagsScriptable::removeTag(const QString &tagName, QStringList *tags)
{
    if ( !tags->contains(tagName) )
        return false;

    tags->removeOne(tagName);
    return true;
}

ItemTagsLoader::ItemTagsLoader()
    : m_blockDataChange(false)
{
}

ItemTagsLoader::~ItemTagsLoader() = default;

QStringList ItemTagsLoader::formatsToSave() const
{
    return QStringList(mimeTags);
}

QVariantMap ItemTagsLoader::applySettings()
{
    m_tags.clear();

    QStringList tags;

    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        const Tag tag = tagFromTable(row);
        if (isTagValid(tag)) {
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
    for (const auto &tagField : m_settings.value(configTags).toStringList()) {
        Tag tag = deserializeTag(tagField);
        if (isTagValid(tag))
            m_tags.append(tag);
    }
}

QWidget *ItemTagsLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemTagsSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    // Init tag table.
    for (const auto &tag : m_tags)
        addTagToSettingsTable(tag);
    for (int i = 0; i < 10; ++i)
        addTagToSettingsTable();

    QTableWidget *t = ui->tableWidget;
    setHeaderSectionResizeMode(t, tagsTableColumns::name, QHeaderView::Stretch);
    setHeaderSectionResizeMode(t, tagsTableColumns::styleSheet, QHeaderView::Stretch);
    setHeaderSectionResizeMode(t, tagsTableColumns::match, QHeaderView::Stretch);
    setFixedColumnSize(t, tagsTableColumns::color);
    setFixedColumnSize(t, tagsTableColumns::icon);

    connect( ui->tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)),
             this, SLOT(onTableWidgetItemChanged(QTableWidgetItem*)) );

    return w;
}

ItemWidget *ItemTagsLoader::transform(ItemWidget *itemWidget, const QVariantMap &data)
{
    const Tags tags = toTags(::tags(data));
    if ( tags.isEmpty() )
        return nullptr;

    itemWidget->setTagged(true);
    return new ItemTags(itemWidget, tags);
}

bool ItemTagsLoader::matches(const QModelIndex &index, const QRegExp &re) const
{
    const QByteArray tagsData =
            index.data(contentType::data).toMap().value(mimeTags).toByteArray();
    const auto tags = getTextData(tagsData);
    return re.indexIn(tags) != -1;
}

QObject *ItemTagsLoader::tests(const TestInterfacePtr &test) const
{
#ifdef HAS_TESTS
    QStringList tags;

    for (const auto &tagName : ItemTagsTests::testTags()) {
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
    return nullptr;
#endif
}

ItemScriptable *ItemTagsLoader::scriptableObject()
{
    return new ItemTagsScriptable(userTags());
}

QVector<Command> ItemTagsLoader::commands() const
{
    QVector<Command> commands;

    if (m_tags.isEmpty()) {
        addTagCommands(tr("Important", "Tag name for example command"), QString(), &commands);
    } else {
        for (const auto &tag : m_tags)
            addTagCommands(tag.name, tag.match, &commands);
    }

    Command c;

    c = dummyTagCommand();
    c.name = addTagText();
    c.cmd = "copyq: plugins.itemtags.tag()";
    commands.append(c);

    c = dummyTagCommand();
    c.input = mimeTags;
    c.name = removeTagText();
    c.cmd = "copyq: plugins.itemtags.untag()";
    commands.append(c);

    c = dummyTagCommand();
    c.input = mimeTags;
    c.name = tr("Clear all tags");
    c.cmd = "copyq: plugins.itemtags.clearTags()";
    commands.append(c);

    return commands;
}

QStringList ItemTagsLoader::userTags() const
{
    QStringList tags;
    tags.reserve( m_tags.size() );

    for (const auto &tag : m_tags)
        tags.append(tag.name);

    return tags;
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

    onTableWidgetItemChanged();
}

void ItemTagsLoader::onTableWidgetItemChanged(QTableWidgetItem *item)
{
    // Omit calling this recursively.
    if (m_blockDataChange)
        return;

    m_blockDataChange = true;

    const int row = item->row();
    QTableWidgetItem *tagItem = ui->tableWidget->item(row, tagsTableColumns::name);
    const QVariant value = QVariant::fromValue(tagFromTable(row));
    tagItem->setData(TagTableWidgetItem::TagRole, value);

    m_blockDataChange = false;
}

void ItemTagsLoader::onTableWidgetItemChanged()
{
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
        onTableWidgetItemChanged(ui->tableWidget->item(row, 0));
}

QString ItemTagsLoader::serializeTag(const ItemTagsLoader::Tag &tag)
{
    return escapeTagField(tag.name)
            + ";;" + escapeTagField(tag.color)
            + ";;" + escapeTagField(tag.icon)
            + ";;" + escapeTagField(tag.styleSheet)
            + ";;" + escapeTagField(tag.match);
}

ItemTagsLoader::Tag ItemTagsLoader::deserializeTag(const QString &tagText)
{
    QStringList tagFields = tagText.split(";;");

    Tag tag;
    tag.name = unescapeTagField(tagFields.value(0));
    tag.color = unescapeTagField(tagFields.value(1));
    tag.icon = unescapeTagField(tagFields.value(2));
    tag.styleSheet = unescapeTagField(tagFields.value(3));
    tag.match = unescapeTagField(tagFields.value(4));

    return tag;
}

ItemTagsLoader::Tags ItemTagsLoader::toTags(const QStringList &tagList)
{
    Tags tags;

    for (const auto &tagText : tagList) {
        QString tagName = tagText.trimmed();
        Tag tag = findMatchingTag(tagName, m_tags);

        if (isTagValid(tag)) {
            if (tag.match.isEmpty()) {
                tag.name = tagName;
            } else {
                const QRegExp re(tag.match);
                tag.name = QString(tagName).replace(re, tag.name);
            }
        } else {
            tag.name = tagName;

            // Get default tag style from theme.
            const QSettings settings;
            tag.color = settings.value("Theme/num_fg").toString();
        }

        tags.append(tag);
    }

    return tags;
}

void ItemTagsLoader::addTagToSettingsTable(const ItemTagsLoader::Tag &tag)
{
    QTableWidget *t = ui->tableWidget;

    const int row = t->rowCount();

    t->insertRow(row);
    t->setItem( row, tagsTableColumns::name, new TagTableWidgetItem(tag.name) );
    t->setItem( row, tagsTableColumns::match, new QTableWidgetItem(tag.match) );
    t->setItem( row, tagsTableColumns::styleSheet, new QTableWidgetItem(tag.styleSheet) );
    t->setItem( row, tagsTableColumns::color, new QTableWidgetItem() );
    t->setItem( row, tagsTableColumns::icon, new QTableWidgetItem() );

    auto colorButton = new QPushButton(t);
    const QColor color = tag.color.isEmpty()
            ? QColor::fromRgb(50, 50, 50)
            : deserializeColor(tag.color);
    setColorIcon(colorButton, color);
    t->setCellWidget(row, tagsTableColumns::color, colorButton);
    connect(colorButton, SIGNAL(clicked()), SLOT(onColorButtonClicked()));

    auto iconButton = new IconSelectButton(t);
    iconButton->setCurrentIcon(tag.icon);
    t->setCellWidget(row, tagsTableColumns::icon, iconButton);
    connect(iconButton, SIGNAL(currentIconChanged(QString)), SLOT(onTableWidgetItemChanged()));

    onTableWidgetItemChanged(t->item(row, 0));
}

ItemTagsLoader::Tag ItemTagsLoader::tagFromTable(int row)
{
    QTableWidget *t = ui->tableWidget;

    Tag tag;
    tag.name = t->item(row, tagsTableColumns::name)->text();
    const QColor color =
            cellWidgetProperty(t, row, tagsTableColumns::color, propertyColor).value<QColor>();
    tag.color = serializeColor(color);
    tag.icon = cellWidgetProperty(t, row, tagsTableColumns::icon, "currentIcon").toString();
    tag.styleSheet = t->item(row, tagsTableColumns::styleSheet)->text();
    tag.match = t->item(row, tagsTableColumns::match)->text();
    return tag;
}

Q_EXPORT_PLUGIN2(itemtags, ItemTagsLoader)
