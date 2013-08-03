/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include <QDialog>
#include <QHash>
#include <QScopedPointer>

namespace Ui {
    class ConfigurationManager;
}

class ClipboardBrowser;
class ClipboardModel;
class CommandWidget;
class ConfigTabAppearance;
class IconFactory;
class ItemFactory;
class Option;
class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidgetItem;
class QPushButton;
class QSettings;
class QSpinBox;
class QTreeWidgetItem;

struct Command;

/**
 * Configuration management.
 * Singleton.
 */
class ConfigurationManager : public QDialog
{
    Q_OBJECT

public:
    typedef QList<Command> Commands;

    ~ConfigurationManager();

    /** Return singleton instance. */
    static ConfigurationManager *instance();

    /** Destroy singleton instance. */
    static void drop();

    /** Load settings from default file. */
    void loadSettings();
    /** Load settings to default file. Emits configurationChanged() signal. */
    void saveSettings();

    /** Return value for option with given @a name. */
    QVariant value(const QString &name) const;
    /** Set @a value for option with given @a name. */
    void setValue(const QString &name, const QVariant &value);
    /** Return list of options that can be set or view using command line. */
    QStringList options() const;
    /** Return tooltip text for option with given @a name. */
    QString optionToolTip(const QString &name) const;

    /** Load items from configuration file. */
    void loadItems(
            ClipboardModel &model, //!< Model for items.
            const QString &id //!< See ClipboardBrowser::getID().
            );
    /** Save items to configuration file. */
    void saveItems(
            const ClipboardModel &model, //!< Model containing items to save.
            const QString &id //!< See ClipboardBrowser::getID().
            );
    /** Remove configuration file for items. */
    void removeItems(
            const QString &id //!< See ClipboardBrowser::getID().
            );

    /**
     * Restore widget's geometry (usually size and position of a window).
     * @return True only if geometry is available.
     */
    bool loadGeometry(QWidget *widget) const;
    /** Save widget's geometry (usually size and position of a window). */
    void saveGeometry(const QWidget *widget);

    /** Return enabled commands. */
    Commands commands(bool onlyEnabled = true) const;
    /** Create new command. */
    void addCommand(const Command &cmd);

    /** Set available tab names (for combo boxes). */
    void setTabs(const QStringList &tabs);

    const ConfigTabAppearance *tabAppearance() const;

    void setVisible(bool visible);

    ItemFactory *itemFactory() const { return m_itemFactory; }
    IconFactory *iconFactory() const { return m_iconFactory.data(); }

signals:
    /** Emitted if configuration changes (after saveSettings() call). */
    void configurationChanged();

private:
    static ConfigurationManager *m_Instance;
    Ui::ConfigurationManager *ui;
    QString m_datfilename;
    QHash<QString, Option> m_options;

    ItemFactory *m_itemFactory;
    QScopedPointer<IconFactory> m_iconFactory;

    explicit ConfigurationManager();

    ConfigurationManager(const ConfigurationManager &);
    ConfigurationManager& operator=(const ConfigurationManager &);

    void updateCommandItem(QListWidgetItem *item);
    void shortcutButtonClicked(QObject *button);

    /**
     * Some example commands.
     * @return True if command with given index available.
     */
    static bool defaultCommand(int index, Command *c);

    /**
     * @return File name for data file with items.
     */
    QString itemFileName(const QString &id) const;

    /**
     * @return Name of option to save/restore geometry of @a widget.
     */
    QString getGeomentryOptionName(const QWidget *widget) const;

    void loadCommands();

    /**
     * Update icons in dialog.
     */
    void updateIcons();

    void initTabIcons();

    void initPluginWidgets();

    void initCommandWidgets();

    /** Update autostarting the application. */
    void updateAutostart();

    /** Enable/disable autostarting the application. */
    void setAutostartEnable();

    void initOptions();
    void bind(const char *optionKey, QCheckBox *obj, bool defaultValue);
    void bind(const char *optionKey, QSpinBox  *obj, int defaultValue);
    void bind(const char *optionKey, QLineEdit *obj, const char *defaultValue);
    void bind(const char *optionKey, QComboBox *obj, int defaultValue);
    void bind(const char *optionKey, QPushButton *obj, const char *defaultValue);
    void bind(const char *optionKey, const QVariant &defaultValue);

private slots:
    void apply();
    void on_buttonBox_clicked(QAbstractButton* button);
    void onFinished(int result);

    void on_itemOrderListCommands_addButtonClicked(QAction *action);

    void onShortcutButtonClicked();

    void on_checkBoxMenuTabIsCurrent_stateChanged(int);

    void onCurrentCommandWidgetIconChanged(const QIcon &icon);
    void onCurrentCommandWidgetNameChanged(const QString &name);
    void on_spinBoxTrayItems_valueChanged(int value);
};

const QIcon &getIconFromResources(const QString &iconName);

const QIcon getIcon(const QString &themeName, ushort iconId, const QColor &color = QColor());

#endif // CONFIGURATIONMANAGER_H
