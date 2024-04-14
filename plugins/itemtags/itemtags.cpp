// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemtags.h"
#include "ui_itemtagssettings.h"

#include "common/command.h"
#include "common/compatibility.h"
#include "common/contenttype.h"
#include "common/regexp.h"
#include "common/textdata.h"
#include "gui/iconfont.h"
#include "gui/iconselectbutton.h"
#include "gui/pixelratio.h"
#include "item/itemfilter.h"

#ifdef HAS_TESTS
#   include "tests/itemtagstests.h"
#endif

#include <QBoxLayout>
#include <QColorDialog>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QtPlugin>
#include <QUrl>

#include <memory>

Q_DECLARE_METATYPE(ItemTags::Tag)

namespace {

const QLatin1String mimeTags("application/x-copyq-tags");

const QLatin1String configTags("tags");

const char propertyColor[] = "CopyQ_color";

namespace tagsTableColumns {
enum {
    name,
    match,
    styleSheet,
    color,
    icon,
    lock
};
}

class ElidedLabel final : public QLabel
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

    return QString::fromLatin1("rgba(%1,%2,%3,%4)")
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

void setFixedColumnSize(QTableWidget *table, int logicalIndex)
{
    table->horizontalHeader()->setSectionResizeMode(logicalIndex, QHeaderView::Fixed);
    table->resizeColumnToContents(logicalIndex);
}

QVariant cellWidgetProperty(QTableWidget *table, int row, int column, const char *property)
{
    return table->cellWidget(row, column)->property(property);
}

QStringList tags(const QVariant &tags)
{
    return getTextData( tags.toByteArray() )
            .split(',', SKIP_EMPTY_PARTS);
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
    const QString quotedTag = quoteString(name);

    c = dummyTagCommand();
    c.internalId = QStringLiteral("copyq_tags_tag:") + name;
    c.name = ItemTagsLoader::tr("Toggle Tag %1").arg(quotedTag);
    c.cmd = QStringLiteral(
        "copyq: (plugins.itemtags.hasTag(%1) ? plugins.itemtags.untag : plugins.itemtags.tag)(%1)"
    ).arg(tagString);
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
    layout->setContentsMargins(x, 0, x, 0);
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
        if ( tag.name.isEmpty() && tag.icon.isEmpty() )
            continue;

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
            const auto re = anchoredRegExp(tag.match);
            if (tagText.contains(re))
                return tag;
        }
    }

    return ItemTags::Tag();
}

bool isLocked(const QModelIndex &index, const ItemTags::Tags &tags)
{
    const auto dataMap = index.data(contentType::data).toMap();
    const auto itemTags = ::tags(dataMap);
    return std::any_of(
        std::begin(itemTags), std::end(itemTags),
        [&tags](const QString &itemTag){
            const auto tag = findMatchingTag(itemTag, tags);
            return tag.lock;
        });
}

bool containsLockedItems(const QModelIndexList &indexList, const ItemTags::Tags &tags)
{
    return std::any_of(
        std::begin(indexList), std::end(indexList),
        [&tags](const QModelIndex &index){
            return isLocked(index, tags);
        });
}

class TagTableWidgetItem final : public QTableWidgetItem
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

            const auto ratio = pixelRatio(&tagWidget);
            m_pixmap = QPixmap( tagWidget.sizeHint() * ratio );
            m_pixmap.setDevicePixelRatio(ratio);

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
    , ItemWidgetWrapper(childItem, this)
    , m_tagWidget(new QWidget(childItem->widget()->parentWidget()))
{
    QBoxLayout *tagLayout = new QHBoxLayout(m_tagWidget);
    tagLayout->setContentsMargins({});
    addTagButtons(tagLayout, tags);

    childItem->widget()->setObjectName("item_child");
    childItem->widget()->setParent(this);

    QBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);

    layout->addWidget(m_tagWidget, 0);
    layout->addWidget( childItem->widget(), 1 );
}

void ItemTags::updateSize(QSize maximumSize, int idealWidth)
{
    setMaximumSize(maximumSize);
    m_tagWidget->setFixedWidth(idealWidth);
    ItemWidgetWrapper::updateSize(maximumSize, idealWidth);
    adjustSize();
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

ItemTagsSaver::ItemTagsSaver(const ItemTags::Tags &tags, const ItemSaverPtr &saver)
    : ItemSaverWrapper(saver)
    , m_tags(tags)
{
}

bool ItemTagsSaver::canRemoveItems(const QList<QModelIndex> &indexList, QString *error)
{
    if ( !containsLockedItems(indexList, m_tags) )
        return ItemSaverWrapper::canRemoveItems(indexList, error);

    if (error) {
        *error = "Removing items with locked tags is not allowed (untag items first)";
        return false;
    }

    QMessageBox::information(
                QApplication::activeWindow(),
                ItemTagsLoader::tr("Cannot Remove Items With a Locked Tag"),
                ItemTagsLoader::tr("Untag items first to remove them.") );
    return false;
}

bool ItemTagsSaver::canDropItem(const QModelIndex &index)
{
    return !isLocked(index, m_tags) && ItemSaverWrapper::canDropItem(index);
}

bool ItemTagsSaver::canMoveItems(const QList<QModelIndex> &indexList)
{
    return !containsLockedItems(indexList, m_tags)
            && ItemSaverWrapper::canMoveItems(indexList);
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

void ItemTagsLoader::applySettings(QSettings &settings)
{
    QStringList tags;

    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        const Tag tag = tagFromTable(row);
        if (isTagValid(tag))
            tags.append(serializeTag(tag));
    }

    settings.setValue(configTags, tags);
}

void ItemTagsLoader::loadSettings(const QSettings &settings)
{
    m_tags.clear();
    for (const auto &tagField : settings.value(configTags).toStringList()) {
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

    auto header = ui->tableWidget->horizontalHeader();
    header->setSectionResizeMode(tagsTableColumns::name, QHeaderView::Stretch);
    header->setSectionResizeMode(tagsTableColumns::styleSheet, QHeaderView::Stretch);
    header->setSectionResizeMode(tagsTableColumns::match, QHeaderView::Stretch);
    setFixedColumnSize(ui->tableWidget, tagsTableColumns::color);
    setFixedColumnSize(ui->tableWidget, tagsTableColumns::icon);
    setFixedColumnSize(ui->tableWidget, tagsTableColumns::lock);

    connect( ui->tableWidget, &QTableWidget::itemChanged,
             this, &ItemTagsLoader::onTableWidgetItemChanged );

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

ItemSaverPtr ItemTagsLoader::transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *)
{
    // Avoid checking for locked items if no locked tags are specified in configuration.
    const bool hasAnyLocks = std::any_of(
        std::begin(m_tags), std::end(m_tags),
        [](const ItemTags::Tag &tag){ return tag.lock; });
    return hasAnyLocks ? std::make_shared<ItemTagsSaver>(m_tags, saver) : saver;
}

bool ItemTagsLoader::matches(const QModelIndex &index, const ItemFilter &filter) const
{
    const QByteArray tagsData =
            index.data(contentType::data).toMap().value(mimeTags).toByteArray();
    const auto tags = getTextData(tagsData);
    return filter.matches(tags) || filter.matches(accentsRemoved(tags));
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
    Q_UNUSED(test)
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
        const QRegularExpression reCapture(R"(\(.*\))");
        const QRegularExpression reGroup(R"(\\\d)");
        for (const auto &tag : m_tags) {
            if ( reCapture.match(tag.match).hasMatch() && reGroup.match(tag.name).hasMatch() )
                continue;

            addTagCommands(tag.name, tag.match, &commands);
        }
    }

    Command c;

    c = dummyTagCommand();
    c.internalId = QStringLiteral("copyq_tags_tag");
    c.name = addTagText();
    c.cmd = "copyq: plugins.itemtags.tag()";
    commands.append(c);

    c = dummyTagCommand();
    c.internalId = QStringLiteral("copyq_tags_untag");
    c.input = mimeTags;
    c.name = removeTagText();
    c.cmd = "copyq: plugins.itemtags.untag()";
    commands.append(c);

    c = dummyTagCommand();
    c.internalId = QStringLiteral("copyq_tags_clear");
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
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    dialog.setCurrentColor(color);

    if ( dialog.exec() == QDialog::Accepted )
        setColorIcon( button, dialog.selectedColor() );

    onAllTableWidgetItemsChanged();
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

void ItemTagsLoader::onAllTableWidgetItemsChanged()
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
            + ";;" + escapeTagField(tag.match)
            + ";;" + (tag.lock ? "L" : "");
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
    tag.lock = unescapeTagField(tagFields.value(5)) == QLatin1String("L");

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
                const QRegularExpression re(tag.match);
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

    auto lock = new QTableWidgetItem();
    lock->setCheckState(tag.lock ? Qt::Checked : Qt::Unchecked);
    const QString toolTip = t->horizontalHeaderItem(tagsTableColumns::lock)->toolTip();
    lock->setToolTip(toolTip);
    t->setItem( row, tagsTableColumns::lock, lock );

    auto colorButton = new QPushButton(t);
    const QColor color = tag.color.isEmpty()
            ? QColor::fromRgb(50, 50, 50)
            : deserializeColor(tag.color);
    setColorIcon(colorButton, color);
    t->setCellWidget(row, tagsTableColumns::color, colorButton);
    connect(colorButton, &QAbstractButton::clicked, this, &ItemTagsLoader::onColorButtonClicked);

    auto iconButton = new IconSelectButton(t);
    iconButton->setCurrentIcon(tag.icon);
    t->setCellWidget(row, tagsTableColumns::icon, iconButton);
    connect(iconButton, &IconSelectButton::currentIconChanged, this, &ItemTagsLoader::onAllTableWidgetItemsChanged);

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
    tag.lock = t->item(row, tagsTableColumns::lock)->checkState() == Qt::Checked;
    return tag;
}
