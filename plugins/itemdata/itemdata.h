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

#ifndef ITEMDATA_H
#define ITEMDATA_H

#include "item/itemwidget.h"
#include "gui/icons.h"

#include <QLabel>

#include <memory>

namespace Ui {
class ItemDataSettings;
}

class QTreeWidgetItem;

class ItemData : public QLabel, public ItemWidget
{
    Q_OBJECT

public:
    ItemData(const QModelIndex &index, int maxBytes, QWidget *parent);

protected:
    void highlight(const QRegExp &re, const QFont &highlightFont,
                           const QPalette &highlightPalette) override;

    QWidget *createEditor(QWidget *) const override { return nullptr; }

    void mousePressEvent(QMouseEvent *e) override;

    void mouseDoubleClickEvent(QMouseEvent *e) override;

    void contextMenuEvent(QContextMenuEvent *e) override;
};

class ItemDataLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemDataLoader();
    ~ItemDataLoader();

    ItemWidget *create(const QModelIndex &index, QWidget *parent, bool preview) const override;

    int priority() const override { return -20; }

    QString id() const override { return "itemdata"; }
    QString name() const override { return tr("Data"); }
    QString author() const override { return QString(); }
    QString description() const override { return tr("Various data to save."); }
    QVariant icon() const override { return QVariant(IconFileText); }

    QStringList formatsToSave() const override;

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override { m_settings = settings; }

    QWidget *createSettingsWidget(QWidget *parent) override;

private slots:
    void on_treeWidgetFormats_itemActivated(QTreeWidgetItem *item, int column);

private:
    QVariantMap m_settings;
    std::unique_ptr<Ui::ItemDataSettings> ui;
};

#endif // ITEMDATA_H
