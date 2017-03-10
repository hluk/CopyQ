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

protected:
    void highlight(const QRegExp &re, const QFont &highlightFont,
                           const QPalette &highlightPalette) override;

    void updateSize(const QSize &maximumSize, int idealWidth) override;

    QWidget *createEditor(QWidget *parent) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const override;

    bool hasChanges(QWidget *editor) const override;

    QObject *createExternalEditor(const QModelIndex &index, QWidget *parent) const override;

    void setTagged(bool tagged) override;

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

    QString id() const override { return "itemfakevim"; }
    QString name() const override { return tr("FakeVim"); }
    QString author() const override
    { return tr("FakeVim plugin is part of Qt Creator")
                + " (Copyright (C) override 2013 Digia Plc and/or its subsidiary(-ies))."; }
    QString description() const override { return tr("Emulate Vim editor while editing items."); }
    QVariant icon() const override;

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    ItemWidget *transform(ItemWidget *itemWidget, const QModelIndex &index) override;

    QObject *tests(const TestInterfacePtr &test) const override;

private:
    bool m_enabled;
    QString m_sourceFileName;
    QScopedPointer<Ui::ItemFakeVimSettings> ui;
};

#endif // ITEMFAKEVIM_H
