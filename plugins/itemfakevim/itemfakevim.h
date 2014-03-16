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

#ifndef ITEMFAKEVIM_H
#define ITEMFAKEVIM_H

#include "item/itemwidget.h"

#include <QScopedPointer>

namespace Ui {
class ItemFakeVimSettings;
}

class QWidget;

class ItemFakeVim : public ItemWidget
{
public:
    ItemFakeVim(ItemWidget *childItem, const QString &sourceFileName);

    virtual void setCurrent(bool current);

protected:
    virtual void highlight(const QRegExp &re, const QFont &highlightFont,
                           const QPalette &highlightPalette);

    virtual void updateSize(const QSize &maximumSize);

    virtual QWidget *createEditor(QWidget *parent) const;

    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const;

    virtual void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const;

    virtual bool hasChanges(QWidget *editor) const;

    virtual QObject *createExternalEditor(const QModelIndex &index, QWidget *parent) const;

private:
    QScopedPointer<ItemWidget> m_childItem;
    QString m_sourceFileName;
};

class ItemFakeVimLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemFakeVimLoader();
    ~ItemFakeVimLoader();

    virtual QString id() const { return "itemfakevim"; }
    virtual QString name() const { return tr("FakeVim"); }
    virtual QString author() const
    { return tr("FakeVim plugin is part of Qt Creator")
                + " (Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies))."; }
    virtual QString description() const { return tr("Emulate Vim editor while editing items."); }
    virtual QVariant icon() const;

    virtual QVariantMap applySettings();

    virtual void loadSettings(const QVariantMap &settings);

    virtual QWidget *createSettingsWidget(QWidget *parent);

    virtual ItemWidget *transform(ItemWidget *itemWidget, const QModelIndex &index);

    virtual QObject *tests(const TestInterfacePtr &test) const;

private:
    bool m_enabled;
    QString m_sourceFileName;
    QScopedPointer<Ui::ItemFakeVimSettings> ui;
};

#endif // ITEMFAKEVIM_H
