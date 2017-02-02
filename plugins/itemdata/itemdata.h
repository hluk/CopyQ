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
#include <QScopedPointer>

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
    virtual void highlight(const QRegExp &re, const QFont &highlightFont,
                           const QPalette &highlightPalette);

    virtual QWidget *createEditor(QWidget *) const { return NULL; }

    virtual void mousePressEvent(QMouseEvent *e);

    virtual void mouseDoubleClickEvent(QMouseEvent *e);

    virtual void contextMenuEvent(QContextMenuEvent *e);
};

class ItemDataLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemDataLoader();
    ~ItemDataLoader();

    virtual ItemWidget *create(const QModelIndex &index, QWidget *parent, bool preview) const;

    virtual int priority() const { return -20; }

    virtual QString id() const { return "itemdata"; }
    virtual QString name() const { return tr("Data"); }
    virtual QString author() const { return QString(); }
    virtual QString description() const { return tr("Various data to save."); }
    virtual QVariant icon() const { return QVariant(IconFileText); }

    virtual QStringList formatsToSave() const;

    virtual QVariantMap applySettings();

    virtual void loadSettings(const QVariantMap &settings) { m_settings = settings; }

    virtual QWidget *createSettingsWidget(QWidget *parent);

private slots:
    void on_treeWidgetFormats_itemActivated(QTreeWidgetItem *item, int column);

private:
    QVariantMap m_settings;
    QScopedPointer<Ui::ItemDataSettings> ui;
};

#endif // ITEMDATA_H
