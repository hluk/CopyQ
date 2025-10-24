// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "item/itemwidget.h"

#include <QObject>
#include <QVariant>

class KeyClicker;

class ItemTestsScriptable final : public ItemScriptable
{
    Q_OBJECT
public slots:
    void keys();
};

class ItemTestsLoader final : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    QString id() const override { return "itemtests"; }
    QString name() const override { return "Tests"; }
    QString author() const override { return {}; }
    QString description() const override { return {}; }
    QVariant icon() const override { return {}; }
    ItemScriptable *scriptableObject() override;
    QVariant scriptCallback(const QVariantList &arguments) override;


private:
    KeyClicker *keyClicker();
    KeyClicker *m_keyClicker = nullptr;
};
