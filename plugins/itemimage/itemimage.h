/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include <QPixmap>

#include <memory>

class QMovie;

namespace Ui {
class ItemImageSettings;
}

class ItemImage final : public QLabel, public ItemWidget
{
    Q_OBJECT

public:
    ItemImage(
            const QPixmap &pix,
            const QByteArray &animationData, const QByteArray &animationFormat,
            QWidget *parent);

    void updateSize(QSize maximumSize, int idealWidth) override;

    void setCurrent(bool current) override;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void startAnimation();
    void stopAnimation();

    QPixmap m_pixmap;
    QByteArray m_animationData;
    QByteArray m_animationFormat;
    QMovie *m_animation;
};

class ItemImageLoader final : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemImageLoader();
    ~ItemImageLoader();

    ItemWidget *create(const QVariantMap &data, QWidget *parent, bool preview) const override;

    int priority() const override { return 15; }

    QString id() const override { return "itemimage"; }
    QString name() const override { return tr("Images"); }
    QString author() const override { return QString(); }
    QString description() const override { return tr("Display images."); }
    QVariant icon() const override { return QVariant(IconCamera); }

    QStringList formatsToSave() const override;

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override { m_settings = settings; }

    QWidget *createSettingsWidget(QWidget *parent) override;

    QObject *createExternalEditor(const QModelIndex &index, const QVariantMap &data, QWidget *parent) const override;

private:
    QVariantMap m_settings;
    std::unique_ptr<Ui::ItemImageSettings> ui;
};

#endif // ITEMIMAGE_H
