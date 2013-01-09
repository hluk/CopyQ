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

#include "itemwidget.h"

#include <QtWebKit/QWebView>

class ItemWeb : public QWebView, public ItemWidget
{
    Q_OBJECT
public:
    ItemWeb(const QString &html, QWidget *parent);

    QWidget *widget() { return this; }

protected:
    void highlight(const QRegExp &re, const QFont &highlightFont,
                   const QPalette &highlightPalette);

    virtual void updateSize();

signals:
    void itemChanged(ItemWidget *self);

private slots:
    void onItemChanged();
};

#endif // ITEMWEB_H
