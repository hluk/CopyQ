// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QWidget>

class ItemOrderList;
class QSettings;

class ConfigTabTabs final : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigTabTabs(QWidget *parent = nullptr);

    void saveTabs(QSettings *settings);

private:
    ItemOrderList *m_list;
};
