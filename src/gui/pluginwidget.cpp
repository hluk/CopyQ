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

#include "pluginwidget.h"
#include "ui_pluginwidget.h"

#include <QSettings>

PluginWidget::PluginWidget(const ItemLoaderInterfacePtr &loader, QWidget *parent)
    : QWidget(parent)
    , ui(NULL)
    , m_loader(loader)
{
}

PluginWidget::~PluginWidget()
{
    delete ui;
}

void PluginWidget::applySettings(QSettings *settings, bool isPluginEnabled)
{
    settings->beginGroup(m_loader->id());

    if (ui) {
        QVariantMap s = m_loader->applySettings();
        foreach (const QString &name, s.keys())
            settings->setValue(name, s[name]);
    }

    settings->setValue("enabled", isPluginEnabled);

    settings->endGroup();
}

void PluginWidget::showEvent(QShowEvent *event)
{
    init();
    QWidget::showEvent(event);
}

void PluginWidget::init()
{
    if (ui)
        return;

    ui = new Ui::PluginWidget;
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
