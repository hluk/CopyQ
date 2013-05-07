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

namespace Ui {
    class ConfigurationManager;
}

class ClipboardBrowser;
class ClipboardModel;
class Command;
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
    /** Load theme from settings file. */
    void loadTheme(QSettings &settings);
    /** Load theme to settings file. */
    void saveTheme(QSettings &settings) const;

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
    Commands commands() const;
    /** Create new command. */
    void addCommand(const Command &cmd);

    /** Set available tab names (for combo boxes). */
    void setTabs(const QStringList &tabs);

    /** Set fonts and color for ClipboardBrowser object. */
    void decorateBrowser(ClipboardBrowser *c) const;
signals:
    /** Emitted if configuration changes (after saveSettings() call). */
    void configurationChanged();

    void applyPluginConfiguration();
    void loadPluginConfiguration();

protected:
    void showEvent(QShowEvent *);

private:
    static ConfigurationManager *m_Instance;
    Ui::ConfigurationManager *ui;
    QString m_datfilename;
    QHash<QString, Option> m_options;
    QHash<QString, Option> m_theme;
    Commands m_commands;

    explicit ConfigurationManager();

    ConfigurationManager(const ConfigurationManager &);
    ConfigurationManager& operator=(const ConfigurationManager &);

    void updateCommandItem(QListWidgetItem *item);
    void shortcutButtonClicked(QObject *button);
    void fontButtonClicked(QObject *button);
    void colorButtonClicked(QObject *button);

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
     * Update icons in dialog.
     */
    void updateIcons();

    /** Set available MIME types (for combo boxes). */
    void updateFormats();

    void initOptions();
    void bind(const char *optionKey, QCheckBox *obj, bool defaultValue);
    void bind(const char *optionKey, QSpinBox  *obj, int defaultValue);
    void bind(const char *optionKey, QLineEdit *obj, const char *defaultValue);
    void bind(const char *optionKey, QComboBox *obj, int defaultValue);
    void bind(const char *optionKey, QPushButton *obj, const char *defaultValue);
    void bind(const char *optionKey, const QVariant &defaultValue);

private slots:
    void on_pushButtonDown_clicked();
    void on_pushButtonUp_clicked();
    void on_pushButtonRemove_clicked();
    void on_toolButtonAddCommand_triggered(QAction *action);
    void apply();
    void on_buttonBox_clicked(QAbstractButton* button);
    void onFinished(int result);

    void onShortcutButtonClicked();
    void onFontButtonClicked();
    void onColorButtonClicked();

    void on_listWidgetCommands_currentItemChanged(QListWidgetItem *current,
                                                  QListWidgetItem *previous);
    void on_listWidgetCommands_itemChanged(QListWidgetItem *item);
    void on_listWidgetCommands_itemSelectionChanged();

    void on_pushButtonLoadTheme_clicked();
    void on_pushButtonSaveTheme_clicked();

    void on_checkBoxShowNumber_stateChanged(int);
    void on_checkBoxScrollbars_stateChanged(int);

    void on_checkBoxMenuTabIsCurrent_stateChanged(int);
    void on_pushButtonPluginPriorityUp_clicked();
    void on_pushButtonPluginPriorityDown_clicked();
};

#endif // CONFIGURATIONMANAGER_H
