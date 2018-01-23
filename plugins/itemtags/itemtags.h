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

#ifndef ITEMTAGS_H
#define ITEMTAGS_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QVariant>
#include <QVector>
#include <QWidget>

#include <memory>

namespace Ui {
class ItemTagsSettings;
}

class QTableWidgetItem;

class ItemTags : public QWidget, public ItemWidget
{
    Q_OBJECT

public:
    struct Tag {
        QString name;
        QString color;
        QString icon;
        QString styleSheet;
        QString match;
    };

    using Tags = QVector<ItemTags::Tag>;

    ItemTags(ItemWidget *childItem, const Tags &tags);

    void setCurrent(bool current) override;

signals:
    void runCommand(const Command &command);

protected:
    void highlight(const QRegExp &re, const QFont &highlightFont,
                           const QPalette &highlightPalette) override;

    QWidget *createEditor(QWidget *parent) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const override;

    bool hasChanges(QWidget *editor) const override;

    QObject *createExternalEditor(const QModelIndex &index, QWidget *parent) const override;

    void updateSize(QSize maximumSize, int idealWidth) override;

private:
    QWidget *m_tagWidget;
    std::unique_ptr<ItemWidget> m_childItem;
};

class ItemTagsLoader;

class ItemTagsScriptable : public ItemScriptable
{
    Q_OBJECT
    Q_PROPERTY(QStringList userTags READ getUserTags)
    Q_PROPERTY(QString mimeTags READ getMimeTags)

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

class ItemTagsLoader : public QObject, public ItemLoaderInterface
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

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    ItemWidget *transform(ItemWidget *itemWidget, const QVariantMap &data) override;

    bool matches(const QModelIndex &index, const QRegExp &re) const override;

    QObject *tests(const TestInterfacePtr &test) const override;

    const QObject *signaler() const override { return this; }

    ItemScriptable *scriptableObject() override;

    QVector<Command> commands() const override;

private slots:
    void onColorButtonClicked();
    void onTableWidgetItemChanged(QTableWidgetItem *item);
    void onTableWidgetItemChanged();

private:
    QStringList userTags() const;

    using Tag = ItemTags::Tag;
    using Tags = ItemTags::Tags;

    static QString serializeTag(const Tag &tag);
    static Tag deserializeTag(const QString &tagText);

    Tags toTags(const QStringList &tagList);

    void addTagToSettingsTable(const Tag &tag = Tag());

    Tag tagFromTable(int row);

    QVariantMap m_settings;
    Tags m_tags;
    std::unique_ptr<Ui::ItemTagsSettings> ui;

    bool m_blockDataChange;
};

#endif // ITEMTAGS_H
