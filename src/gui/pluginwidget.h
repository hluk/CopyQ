// SPDX-License-Identifier: GPL-3.0-or-later

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
