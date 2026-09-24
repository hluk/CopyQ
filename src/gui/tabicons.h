// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once


#include <QtContainerFwd>

class QIcon;
class QComboBox;
class QString;
class QWidget;

/** Return list of saved tabs (ordered by "tabs" option if possible). */
QList<QString> savedTabs();

QString getIconNameForTabName(const QString &tabName);

void setIconNameForTabName(const QString &tabName, const QString &icon);

QIcon getIconForTabName(const QString &tabName);

/**
 * Return mapping of tab names to their icon names for all configured tabs.
 *
 * Use this with the overload of getIconForTabName() below when looking up
 * icons for many tabs at once, to avoid rescanning the configuration for
 * each tab.
 */
QHash<QString, QString> tabIconNames();

QIcon getIconForTabName(const QString &tabName, const QHash<QString, QString> &iconNames);

void initTabComboBox(QComboBox *comboBox);

void setDefaultTabItemCounterStyle(QWidget *widget);

void setComboBoxItems(QComboBox *comboBox, const QList<QString> &items);
