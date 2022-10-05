// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TABICONS_H
#define TABICONS_H

#include <QtContainerFwd>

class QIcon;
class QComboBox;
class QString;
class QWidget;

/** Return list of saved tabs (ordered by "tabs" option if possible). */
QList<QString> savedTabs();

QString getIconNameForTabName(const QString &tabName);

void setIconNameForTabName(const QString &name, const QString &icon);

QIcon getIconForTabName(const QString &tabName);

void initTabComboBox(QComboBox *comboBox);

void setDefaultTabItemCounterStyle(QWidget *widget);

void setComboBoxItems(QComboBox *comboBox, const QList<QString> &items);

#endif // TABICONS_H
