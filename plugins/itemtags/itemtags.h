// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMTAGS_H
#define ITEMTAGS_H

#include "gui/icons.h"
#include "item/itemwidgetwrapper.h"
#include "item/itemsaverwrapper.h"

#include <QVariant>
#include <QVector>
#include <QWidget>

namespace Ui {
class ItemTagsSettings;
}

class QTableWidgetItem;

class ItemTags final : public QWidget, public ItemWidgetWrapper
{
    Q_OBJECT

public:
    struct Tag {
        QString name;
        QString color;
        QString icon;
        QString styleSheet;
        QString match;
        bool lock;
    };

    using Tags = QVector<ItemTags::Tag>;

    ItemTags(ItemWidget *childItem, const Tags &tags);

signals:
    void runCommand(const Command &command);

protected:
    void updateSize(QSize maximumSize, int idealWidth) override;

private:
    QWidget *m_tagWidget;
};

class ItemTagsLoader;

class ItemTagsScriptable final : public ItemScriptable
{
    Q_OBJECT
    Q_PROPERTY(QStringList userTags READ getUserTags CONSTANT)
    Q_PROPERTY(QString mimeTags READ getMimeTags CONSTANT)

public:
    explicit ItemTagsScriptable(const QStringList &userTags)
        : m_userTags(userTags)
    {
    }

    QStringList getUserTags() const;

    QString getMimeTags() const;

public slots:
    QStringList tags();
    void tag();
    void untag();
    void clearTags();
    bool hasTag();

private:
    QString askTagName(const QString &dialogTitle, const QStringList &tags);
    QString askRemoveTagName(const QStringList &tags);
    QList<int> rows(const QVariantList &arguments, int skip);
    QStringList tags(int row);
    void setTags(int row, const QStringList &tags);
    bool addTag(const QString &tagName, QStringList *tags);
    bool removeTag(const QString &tagName, QStringList *tags);

    QStringList m_userTags;
};

class ItemTagsSaver final : public ItemSaverWrapper
{
public:
    ItemTagsSaver(const ItemTags::Tags &tags, const ItemSaverPtr &saver);

    bool canRemoveItems(const QList<QModelIndex> &indexList, QString *error) override;

    bool canDropItem(const QModelIndex &index) override;

    bool canMoveItems(const QList<QModelIndex> &indexList) override;

private:
    ItemTags::Tags m_tags;
};

class ItemTagsLoader final : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemTagsLoader();
    ~ItemTagsLoader();

    QString id() const override { return "itemtags"; }
    QString name() const override { return tr("Tags"); }
    QString author() const override { return QString(); }
    QString description() const override { return tr("Display tags for items."); }
    QVariant icon() const override { return QVariant(IconTag); }

    QStringList formatsToSave() const override;

    void applySettings(QSettings &settings) override;

    void loadSettings(const QSettings &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    ItemWidget *transform(ItemWidget *itemWidget, const QVariantMap &data) override;

    ItemSaverPtr transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *model) override;

    bool matches(const QModelIndex &index, const ItemFilter &filter) const override;

    QObject *tests(const TestInterfacePtr &test) const override;

    const QObject *signaler() const override { return this; }

    ItemScriptable *scriptableObject() override;

    QVector<Command> commands() const override;

private:
    void onColorButtonClicked();
    void onTableWidgetItemChanged(QTableWidgetItem *item);
    void onAllTableWidgetItemsChanged();

    QStringList userTags() const;

    using Tag = ItemTags::Tag;
    using Tags = ItemTags::Tags;

    static QString serializeTag(const Tag &tag);
    static Tag deserializeTag(const QString &tagText);

    Tags toTags(const QStringList &tagList);

    void addTagToSettingsTable(const Tag &tag = Tag());

    Tag tagFromTable(int row);

    Tags m_tags;
    std::unique_ptr<Ui::ItemTagsSettings> ui;

    bool m_blockDataChange;
};

#endif // ITEMTAGS_H
