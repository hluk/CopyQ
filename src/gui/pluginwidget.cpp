// SPDX-License-Identifier: GPL-3.0-or-later

#include "pluginwidget.h"
#include "ui_pluginwidget.h"

#include "item/itemwidget.h"

PluginWidget::PluginWidget(const ItemLoaderPtr &loader, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PluginWidget)
    , m_loader(loader)
{
    ui->setupUi(this);

    const QString author = m_loader->author();
    if (author.isEmpty())
        ui->labelAuthor->hide();
    else
        ui->labelAuthor->setText(author);

    const QString description = m_loader->description();
    if (description.isEmpty())
        ui->labelDescription->hide();
    else
        ui->labelDescription->setText(m_loader->description());

    QWidget *loaderSettings = m_loader->createSettingsWidget(this);
    if (loaderSettings) {
        ui->verticalLayout->insertWidget(2, loaderSettings);
        ui->verticalLayout->setStretch(2, 1);
    }
}

PluginWidget::~PluginWidget()
{
    delete ui;
}
