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

#include "commandwidget.h"
#include "option.h"

#include <QDialog>
#include <QHash>

namespace Ui {
    class ConfigurationManager;
}

class ClipboardModel;
class ClipboardBrowser;
class QAbstractButton;
class QListWidgetItem;
class QSettings;

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

    /** Set available tab names (for combo box). */
    void setTabs(const QStringList &tabs);

    /** Set fonts and color for ClipboardBrowser object. */
    void decorateBrowser(ClipboardBrowser *c) const;
signals:
    /** Emitted if configuration changes (after saveSettings() call). */
    void configurationChanged();

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
    void shortcutButtonClicked(QPushButton *button);
    void fontButtonClicked(QPushButton *button);
    void colorButtonClicked(QPushButton *button);

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

private slots:
    void on_pushButtonDown_clicked();
    void on_pushButtonUp_clicked();
    void on_pushButtonRemove_clicked();
    void on_toolButtonAddCommand_triggered(QAction *action);
    void apply();
    void on_buttonBox_clicked(QAbstractButton* button);
    void onFinished(int result);
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();
    void on_pushButton_6_clicked();
    void on_pushButton_7_clicked();
    void on_listWidgetCommands_currentItemChanged(QListWidgetItem *current,
                                                  QListWidgetItem *previous);
    void on_listWidgetCommands_itemChanged(QListWidgetItem *item);
    void on_listWidgetCommands_itemSelectionChanged();

    void on_pushButtonFont_clicked();
    void on_pushButtonEditorFont_clicked();
    void on_pushButtonFoundFont_clicked();
    void on_pushButtonNumberFont_clicked();

    void on_pushButtonColorBg_clicked();
    void on_pushButtonColorFg_clicked();
    void on_pushButtonColorAltBg_clicked();
    void on_pushButtonColorSelBg_clicked();
    void on_pushButtonColorSelFg_clicked();
    void on_pushButtonColorFindBg_clicked();
    void on_pushButtonColorFindFg_clicked();
    void on_pushButtonColorEditorBg_clicked();
    void on_pushButtonColorEditorFg_clicked();
    void on_pushButtonColorNumberFg_clicked();

    void on_pushButtonLoadTheme_clicked();
    void on_pushButtonSaveTheme_clicked();

    void on_checkBoxShowNumber_stateChanged(int);
    void on_checkBoxScrollbars_stateChanged(int);

    void on_plainTextEditFormats_textChanged();
};

#endif // CONFIGURATIONMANAGER_H
