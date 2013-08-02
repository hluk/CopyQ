/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "item/itemwidget.h"

#if QT_VERSION < 0x050000
#   include <QtWebKit/QWebView>
#else
#   include <QtWebKitWidgets/QWebView>
#endif

class ItemWeb : public QWebView, public ItemWidget
{
    Q_OBJECT

public:
    ItemWeb(const QString &html, QWidget *parent);

protected:
    void highlight(const QRegExp &re, const QFont &highlightFont,
                   const QPalette &highlightPalette);

    virtual void updateSize();

    virtual void mousePressEvent(QMouseEvent *e);

    virtual void mouseDoubleClickEvent(QMouseEvent *e);

    virtual void contextMenuEvent(QContextMenuEvent *e);

    virtual void wheelEvent(QWheelEvent *e);

    virtual void mouseReleaseEvent(QMouseEvent *e);

private slots:
    void onSelectionChanged();

private slots:
    void onItemChanged();
};

class ItemWebLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    virtual ItemWidget *create(const QModelIndex &index, QWidget *parent) const;

    virtual int priority() const { return 10; }

    virtual QString id() const { return "itemweb"; }
    virtual QString name() const { return tr("Web"); }
    virtual QString author() const { return QString(); }
    virtual QString description() const { return tr("Display web pages."); }
    virtual QVariant icon() const { return QVariant(0xf0ac); }

    virtual QStringList formatsToSave() const;
};

#endif // ITEMWEB_H
