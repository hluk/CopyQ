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

#ifndef ITEMWEB_H
#define ITEMWEB_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QScopedPointer>
#include <QVariantMap>

#if QT_VERSION < 0x050000
#   include <QtWebKit/QWebView>
#else
#   include <QtWebKitWidgets/QWebView>
#endif

namespace Ui {
class ItemWebSettings;
}

class ItemWeb : public QWebView, public ItemWidget
{
    Q_OBJECT

public:
    ItemWeb(const QString &html, int maximumHeight, bool preview, QWidget *parent);

protected:
    void highlight(const QRegExp &re, const QFont &highlightFont,
                   const QPalette &highlightPalette);

    virtual void updateSize(const QSize &maximumSize, int idealWidth);

    virtual void mousePressEvent(QMouseEvent *e);

    virtual void mouseMoveEvent(QMouseEvent *e);

    virtual void wheelEvent(QWheelEvent *e);

    virtual void mouseReleaseEvent(QMouseEvent *e);

    virtual void mouseDoubleClickEvent(QMouseEvent *e);

private slots:
    void onSelectionChanged();
    void onLinkClicked(const QUrl &url);

private slots:
    void onItemChanged();

private:
    bool m_copyOnMouseUp;
    int m_maximumHeight;
    QSize m_maximumSize;
    bool m_preview;
};

class ItemWebLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemWebLoader();
    ~ItemWebLoader();

    virtual ItemWidget *create(const QModelIndex &index, QWidget *parent, bool preview) const;

    virtual int priority() const { return 10; }

    virtual QString id() const { return "itemweb"; }
    virtual QString name() const { return tr("Web"); }
    virtual QString author() const { return QString(); }
    virtual QString description() const { return tr("Display web pages."); }
    virtual QVariant icon() const { return QVariant(IconGlobe); }

    virtual QStringList formatsToSave() const;

    virtual QVariantMap applySettings();

    virtual void loadSettings(const QVariantMap &settings) { m_settings = settings; }

    virtual QWidget *createSettingsWidget(QWidget *parent);

private:
    QVariantMap m_settings;
    QScopedPointer<Ui::ItemWebSettings> ui;
};

#endif // ITEMWEB_H
