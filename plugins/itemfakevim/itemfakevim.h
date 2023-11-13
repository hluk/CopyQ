// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMFAKEVIM_H
#define ITEMFAKEVIM_H

#include "item/itemwidgetwrapper.h"

namespace Ui {
class ItemFakeVimSettings;
}

class QWidget;

class ItemFakeVimLoader final : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemFakeVimLoader();
    ~ItemFakeVimLoader();

    QString id() const override { return "itemfakevim"; }
    QString name() const override { return tr("FakeVim"); }
    QString author() const override
    { return tr("FakeVim plugin is part of Qt Creator")
                + " (Copyright (C) 2016 The Qt Company Ltd.)"; }
    QString description() const override { return tr("Emulate Vim editor while editing items."); }
    QVariant icon() const override;

    void setEnabled(bool enabled) override;

    void applySettings(QSettings &settings) override;

    void loadSettings(const QSettings &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    QObject *tests(const TestInterfacePtr &test) const override;

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void deleteAllWrappers();

private:
    void updateCurrentlyEnabledState();

    void wrapEditWidget(QObject *obj);

    bool m_reallyEnabled = false;
    bool m_currentlyEnabled = false;
    QString m_sourceFileName;
    QScopedPointer<Ui::ItemFakeVimSettings> ui;

    int m_oldCursorFlashTime = -1;
};

#endif // ITEMFAKEVIM_H
