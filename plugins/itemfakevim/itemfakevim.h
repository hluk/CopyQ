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

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    QObject *tests(const TestInterfacePtr &test) const override;

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void deleteAllWrappers();

private:
    void updateCurrentlyEnabledState();

    void wrapEditWidget(QObject *obj);

    bool m_enabled = true;
    bool m_reallyEnabled = false;
    bool m_currentlyEnabled = false;
    QString m_sourceFileName;
    QScopedPointer<Ui::ItemFakeVimSettings> ui;

    int m_oldCursorFlashTime = -1;
};

#endif // ITEMFAKEVIM_H
