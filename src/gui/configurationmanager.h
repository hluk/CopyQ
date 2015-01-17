/*
    Copyright (c) 2015, Lukas Holecek <hluk@email.cz>

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

    /** Return value for option with given @a name. */
    QVariant value(const QString &name) const;
    /** Set @a value for option with given @a name. */
    void setValue(const QString &name, const QVariant &value);

    /** Return list of options that can be set or view using command line. */
    QStringList options() const;
    /** Return tooltip text for option with given @a name. */
    QString optionToolTip(const QString &name) const;

    /** Load items from configuration file. */
    ItemLoaderInterfacePtr loadItems(
            ClipboardModel &model //!< Model for items.
            );
    /** Save items to configuration file. */
    bool saveItems(const ClipboardModel &model //!< Model containing items to save.
            , const ItemLoaderInterfacePtr &loader);
    /** Save items with other plugin with higher priority than current one (@a loader). */
    bool saveItemsWithOther(ClipboardModel &model //!< Model containing items to save.
            , ItemLoaderInterfacePtr *loader);
    /** Remove configuration file for items. */
    void removeItems(const QString &tabName //!< See ClipboardBrowser::getID().
            );
    /** Move configuration file for items. */
    void moveItems(
            const QString &oldId, //!< See ClipboardBrowser::getID().
            const QString &newId //!< See ClipboardBrowser::getID().
            );

    /** Set available tab names (for combo boxes). */
    void setTabs(const QStringList &tabs);

    /** Return list of saved tabs (ordered by "tabs" option if possible). */
    QStringList savedTabs() const;

    ConfigTabAppearance *tabAppearance() const;

    ConfigTabShortcuts *tabShortcuts() const;

    QString getIconNameForTabName(const QString &tabName) const;
    void setIconNameForTabName(const QString &name, const QString &icon);
    QIcon getIconForTabName(const QString &tabName) const;

    void setVisible(bool visible);

    ItemFactory *itemFactory() const { return m_itemFactory; }
    IconFactory *iconFactory() const { return m_iconFactory.data(); }

    /**
     * Register window for saving and restoring geometry.
     */
    void registerWindowGeometry(QWidget *window);

    void saveWindowGeometry(QWidget *window);
    void restoreWindowGeometry(QWidget *window);

    bool eventFilter(QObject *object, QEvent *event);

    QByteArray mainWindowState(const QString &mainWindowObjectName);
    void saveMainWindowState(const QString &mainWindowObjectName, const QByteArray &state);

    QString defaultTabName() const;

    QStringList tabs() const;

    void initTabComboBox(QComboBox *comboBox) const;

signals:
    /** Emitted if configuration changes (after saveSettings() call). */
    void configurationChanged();

    /** Emitted if configuration dialog opens. */
    void started();
    /** Emitted if configuration dialog closes. */
    void stopped();

    void error(const QString &error);

protected:
    static ConfigurationManager *createInstance(QWidget *parent);

private slots:
    void apply();
    void on_buttonBox_clicked(QAbstractButton* button);
    void onFinished(int result);

    void on_checkBoxMenuTabIsCurrent_stateChanged(int);
    void on_spinBoxTrayItems_valueChanged(int value);

    void restoreWindowGeometryOnTimer();

private:
    explicit ConfigurationManager(QWidget *parent);

    void updateCommandItem(QListWidgetItem *item);
    void shortcutButtonClicked(QObject *button);

    /**
     * @return File name for data file with items.
     */
    QString itemFileName(const QString &id) const;

    bool createItemDirectory();

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

    void updateIcons();

    void initTabComboBox(QComboBox *comboBox, const QStringList &tabs) const;
    void updateTabComboBoxes(const QStringList &tabs);
    void updateTabComboBoxes();

    static ConfigurationManager *m_Instance;
    Ui::ConfigurationManager *ui;
    QHash<QString, Option> m_options;
    QHash<QString, QString> m_tabIcons;

    ItemFactory *m_itemFactory;
    QScopedPointer<IconFactory> m_iconFactory;

    bool m_optionWidgetsLoaded;
};

QIcon getIconFromResources(const QString &iconName);

QIcon getIcon(const QString &themeName, ushort iconId);

QIcon getIcon(const QVariant &iconOrIconId);

void setDefaultTabItemCounterStyle(QWidget *widget);

void setComboBoxItems(QComboBox *comboBox, const QStringList &items);

#endif // CONFIGURATIONMANAGER_H
