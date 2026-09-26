// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once


#include <QtContainerFwd>

class QIcon;
class QComboBox;
class QString;
class QWidget;
class Tabs;

/** Return list of saved tabs (ordered by "tabs" option if possible). */
QList<QString> savedTabs();

QString getIconNameForTabName(const QString &tabName);

void setIconNameForTabName(const QString &tabName, const QString &icon);

QIcon getIconForTabName(const QString &tabName);

/**
 * Return icon for a tab using already loaded tab properties.
 *
 * Prefer this over getIconForTabName(const QString &) when looking up icons
 * for many tabs, so the configuration is read only once.
 */
QIcon getIconForTabName(const QString &tabName, const Tabs &tabs);

void initTabComboBox(QComboBox *comboBox);

void setDefaultTabItemCounterStyle(QWidget *widget);

void setComboBoxItems(QComboBox *comboBox, const QList<QString> &items);
