// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TABPROPERTIESWIDGET_H
#define TABPROPERTIESWIDGET_H

#include <QWidget>

namespace Ui {
class TabPropertiesWidget;
}

class TabPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TabPropertiesWidget(QWidget *parent = nullptr);
    ~TabPropertiesWidget();

    void setTabName(const QString &name);
    void setIconName(const QString &iconName);
    void setMaxItemCount(int maxItemCount);
    void setStoreItems(bool storeItems);

signals:
    void iconNameChanged(const QString &iconName);
    void maxItemCountChanged(int maxItemCount);
    void storeItemsChanged(bool storeItems);

private:
    Ui::TabPropertiesWidget *ui;
};

#endif // TABPROPERTIESWIDGET_H
