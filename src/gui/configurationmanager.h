/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include <QScopedPointer>

namespace Ui {
    class ConfigurationManager;
}

class ClipboardModel;
class ConfigTabAppearance;
class ConfigTabShortcuts;
class IconFactory;
class ItemFactory;
class Option;
class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidgetItem;
class QMainWindow;
class QSettings;
class QSpinBox;

/**
 * Configuration management.
 * Singleton.
 */
class ConfigurationManager : public QDialog
{
    Q_OBJECT

    friend class MainWindow;

public:
    ~ConfigurationManager();

    /** Return singleton instance. */
    static ConfigurationManager *instance();

    /** Destroy singleton instance. */
    static void drop();

    /** Load settings from default file. */
    void loadSettings();

    /** Return list of options that can be set or view using command line. */
    QStringList options() const;
    /** Return value of an option. */
    QString optionValue(const QString &name) const;
    /** Return tooltip text for option with given @a name. */
    QString optionToolTip(const QString &name) const;

    ConfigTabAppearance *tabAppearance() const;

    ConfigTabShortcuts *tabShortcuts() const;

    void setVisible(bool visible);

    QString defaultTabName() const;

public slots:
    void done(int result);

signals:
    /** Emitted if configuration changes (after saveSettings() call). */
    void configurationChanged();

    void error(const QString &error);

protected:
    static ConfigurationManager *createInstance(ItemFactory *itemFactory, QWidget *parent);

private slots:
    void apply();
    void on_buttonBox_clicked(QAbstractButton* button);

    void on_checkBoxMenuTabIsCurrent_stateChanged(int);
    void on_spinBoxTrayItems_valueChanged(int value);

private:
    explicit ConfigurationManager(ItemFactory *itemFactory, QWidget *parent);

    void updateCommandItem(QListWidgetItem *item);
    void shortcutButtonClicked(QObject *button);

    void initTabIcons();

    void initPluginWidgets();

    void initLanguages();

    /** Update autostarting the application. */
    void updateAutostart();

    /** Enable/disable autostarting the application. */
    void setAutostartEnable();

    void initOptions();
    void bind(const char *optionKey, QCheckBox *obj, bool defaultValue);
    void bind(const char *optionKey, QSpinBox  *obj, int defaultValue);
    void bind(const char *optionKey, QLineEdit *obj, const QString &defaultValue);
    void bind(const char *optionKey, QComboBox *obj, int defaultValue);
    void bind(const char *optionKey, const QVariant &defaultValue);

    void updateTabComboBoxes();

    static ConfigurationManager *m_Instance;
    Ui::ConfigurationManager *ui;
    QHash<QString, Option> m_options;

    ItemFactory *m_itemFactory;

    bool m_optionWidgetsLoaded;
};

#endif // CONFIGURATIONMANAGER_H
