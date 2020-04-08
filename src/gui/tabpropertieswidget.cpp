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
