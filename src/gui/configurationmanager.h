/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include "item/itemwidget.h"

#include <QDialog>
#include <QHash>

namespace Ui {
    class ConfigurationManager;
}

class ItemFactory;
class Option;
class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidgetItem;
class QSpinBox;

/**
 * Configuration dialog.
 */
class ConfigurationManager : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigurationManager(ItemFactory *itemFactory, QWidget *parent = nullptr);

    /// Simple version of dialog for accessing and settings options from API.
    ConfigurationManager();

    ~ConfigurationManager();

    /** Return list of options that can be set or view using command line. */
    QStringList options() const;

    /** Return value of an option. */
    QVariant optionValue(const QString &name) const;

    /** Set value of an option and returns true only if the value changes. */
    bool setOptionValue(const QString &name, const QString &value);

    /** Return tooltip text for option with given @a name. */
    QString optionToolTip(const QString &name) const;

    void setVisible(bool visible) override;

    /** Load settings from default file. */
    void loadSettings();

public slots:
    void done(int result) override;

signals:
    /** Emitted if configuration changes (after saveSettings() call). */
    void configurationChanged();

    void error(const QString &error);

private slots:
    void apply();
    void on_buttonBox_clicked(QAbstractButton* button);

    void on_checkBoxMenuTabIsCurrent_stateChanged(int);
    void on_spinBoxTrayItems_valueChanged(int value);

private:
    void updateCommandItem(QListWidgetItem *item);
    void shortcutButtonClicked(QObject *button);

    void initTabIcons();

    void initPluginWidgets(ItemFactory *itemFactory);

    void initLanguages();

    /** Update autostarting the application. */
    void updateAutostart();

    /** Enable/disable autostarting the application. */
    void setAutostartEnable();

    void initOptions();

    template <typename Config, typename Widget>
    void bind(Widget *obj);

    template <typename Config>
    void bind();

    void bind(const QString &optionKey, QCheckBox *obj, bool defaultValue);
    void bind(const QString &optionKey, QSpinBox  *obj, int defaultValue);
    void bind(const QString &optionKey, QLineEdit *obj, const QString &defaultValue);
    void bind(const QString &optionKey, QComboBox *obj, int defaultValue);
    void bind(const QString &optionKey, const QVariant &defaultValue);

    void updateTabComboBoxes();

    Ui::ConfigurationManager *ui;
    QHash<QString, Option> m_options;
};

#endif // CONFIGURATIONMANAGER_H
