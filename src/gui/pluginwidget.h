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

#ifndef PLUGINWIDGET_H
#define PLUGINWIDGET_H

#include "item/itemwidget.h"

#include <QWidget>

class QSettings;

namespace Ui {
class PluginWidget;
}

class PluginWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit PluginWidget(const ItemLoaderPtr &loader, QWidget *parent = nullptr);
    ~PluginWidget();

    const ItemLoaderPtr &loader() const { return m_loader; }

private:
    Ui::PluginWidget *ui;
    ItemLoaderPtr m_loader;
};

#endif // PLUGINWIDGET_H
