// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui/tabpropertieswidget.h"
#include "ui_tabpropertieswidget.h"

TabPropertiesWidget::TabPropertiesWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TabPropertiesWidget)
{
    ui->setupUi(this);

    connect( ui->iconButton, &IconSelectButton::currentIconChanged,
             this, &TabPropertiesWidget::iconNameChanged );
    connect( ui->maxItems, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
             this, &TabPropertiesWidget::maxItemCountChanged );
    connect( ui->storeItems, &QCheckBox::toggled,
             this, &TabPropertiesWidget::storeItemsChanged );
}

TabPropertiesWidget::~TabPropertiesWidget()
{
    delete ui;
}

void TabPropertiesWidget::setTabName(const QString &name)
{
    ui->tabName->setText(name);
}

void TabPropertiesWidget::setIconName(const QString &iconName)
{
    ui->iconButton->setCurrentIcon(iconName);
}

void TabPropertiesWidget::setMaxItemCount(int maxItemCount)
{
    ui->maxItems->setValue(maxItemCount);
}

void TabPropertiesWidget::setStoreItems(bool storeItems)
{
    ui->storeItems->setChecked(storeItems);
}
