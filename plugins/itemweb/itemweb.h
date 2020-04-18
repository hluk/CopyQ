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

#ifndef ITEMWEB_H
#define ITEMWEB_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QVariantMap>

#include <QtWebKitWidgets/QWebView>

#include <memory>

namespace Ui {
class ItemWebSettings;
}

class ItemWeb final : public QWebView, public ItemWidget
{
    Q_OBJECT

public:
    ItemWeb(const QString &html, int maximumHeight, bool preview, QWidget *parent);

protected:
    void highlight(const QRegularExpression &re, const QFont &highlightFont,
                   const QPalette &highlightPalette) override;

    void updateSize(QSize maximumSize, int idealWidth) override;

    void mousePressEvent(QMouseEvent *e) override;

    void mouseMoveEvent(QMouseEvent *e) override;

    void wheelEvent(QWheelEvent *e) override;

    void mouseReleaseEvent(QMouseEvent *e) override;

    void mouseDoubleClickEvent(QMouseEvent *e) override;

    QSize sizeHint() const override;

private:
    void onSelectionChanged();
    void onLinkClicked(const QUrl &url);

    void onItemChanged();

    bool m_copyOnMouseUp;
    int m_maximumHeight;
    QSize m_maximumSize;
    int m_idealWidth = 100;
    bool m_resizing = false;
    bool m_preview;
};

class ItemWebLoader final : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemWebLoader();
    ~ItemWebLoader();

    ItemWidget *create(const QVariantMap &data, QWidget *parent, bool preview) const override;

    int priority() const override { return 10; }

    QString id() const override { return "itemweb"; }
    QString name() const override { return tr("Web"); }
    QString author() const override { return QString(); }
    QString description() const override { return tr("Display web pages."); }
    QVariant icon() const override { return QVariant(IconGlobe); }

    QStringList formatsToSave() const override;

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override { m_settings = settings; }

    QWidget *createSettingsWidget(QWidget *parent) override;

private:
    QVariantMap m_settings;
    std::unique_ptr<Ui::ItemWebSettings> ui;
};

#endif // ITEMWEB_H
