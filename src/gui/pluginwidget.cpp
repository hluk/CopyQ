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

#include "pluginwidget.h"
#include "ui_pluginwidget.h"

PluginWidget::PluginWidget(const ItemLoaderInterfacePtr &loader, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PluginWidget)
    , m_loader(loader)
    , m_loaderSettings(NULL)
{
    ui->setupUi(this);

    const QString author = loader->author();
    if (author.isEmpty())
        ui->labelAuthor->hide();
    else
        ui->labelAuthor->setText(author);

    const QString description = loader->description();
    if (description.isEmpty())
        ui->labelDescription->hide();
    else
        ui->labelDescription->setText(loader->description());

    m_loaderSettings = m_loader->createSettingsWidget(this);
    if (m_loaderSettings != NULL) {
        ui->verticalLayout->insertWidget(2, m_loaderSettings);
    }
}

PluginWidget::~PluginWidget()
{
    delete ui;
}
