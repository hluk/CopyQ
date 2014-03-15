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

#ifndef ITEMIMAGE_H
#define ITEMIMAGE_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QLabel>
#include <QScopedPointer>

namespace Ui {
class ItemImageSettings;
}

class ItemImage : public QLabel, public ItemWidget
{
    Q_OBJECT

public:
    ItemImage(const QPixmap &pix, const QString &imageEditor, const QString &svgEditor,
              QWidget *parent);

    virtual QWidget *createEditor(QWidget *) const { return NULL; }

    virtual QObject *createExternalEditor(const QModelIndex &index, QWidget *parent) const;

private:
    QString m_editor;
    QString m_svgEditor;
};

class ItemImageLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ~ItemImageLoader();

    virtual ItemWidget *create(const QModelIndex &index, QWidget *parent) const;

    virtual int priority() const { return 10; }

    virtual QString id() const { return "itemimage"; }
    virtual QString name() const { return tr("Images"); }
    virtual QString author() const { return QString(); }
    virtual QString description() const { return tr("Display images."); }
    virtual QVariant icon() const { return QVariant(IconCamera); }

    virtual QStringList formatsToSave() const;

    virtual QVariantMap applySettings();

    virtual void loadSettings(const QVariantMap &settings) { m_settings = settings; }

    virtual QWidget *createSettingsWidget(QWidget *parent);

private:
    QVariantMap m_settings;
    QScopedPointer<Ui::ItemImageSettings> ui;
};

#endif // ITEMIMAGE_H
