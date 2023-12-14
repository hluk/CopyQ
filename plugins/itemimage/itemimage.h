// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMIMAGE_H
#define ITEMIMAGE_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QLabel>
#include <QPixmap>
#include <QVariant>

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

    QObject *tests(const TestInterfacePtr &test) const override;

    void applySettings(QSettings &settings) override;

    void loadSettings(const QSettings &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    QObject *createExternalEditor(const QModelIndex &index, const QVariantMap &data, QWidget *parent) const override;

private:
    int m_maxImageWidth = 320;
    int m_maxImageHeight = 240;
    QString m_imageEditor;
    QString m_svgEditor;
    std::unique_ptr<Ui::ItemImageSettings> ui;
};

#endif // ITEMIMAGE_H
