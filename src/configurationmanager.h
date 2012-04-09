/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

#include <QDialog>
#include <QMutex>
#include <QMap>
#include <QApplication>
#include <QIcon>
#include <QSettings>


namespace Ui {
    class ConfigurationManager;
}

class ClipboardModel;
class ClipboardBrowser;
class QAbstractButton;
class QListWidgetItem;

struct _Option;
typedef _Option Option;

class ConfigurationManager : public QDialog
{
    Q_OBJECT

public:
    typedef QList<Command> Commands;

    ~ConfigurationManager();

    static ConfigurationManager *instance(QWidget *parent = 0)
    {
        static QMutex mutex;

        if (!m_Instance) {
            QMutexLocker lock(&mutex);
            if (!m_Instance)
                m_Instance = new ConfigurationManager(parent);
        }

        return m_Instance;
    }

    static void drop()
    {
        static QMutex mutex;
        QMutexLocker lock(&mutex);
        delete m_Instance;
        m_Instance = NULL;
    }

    void loadSettings();
    void saveSettings();
    void loadTheme(QSettings &settings);
    void saveTheme(QSettings &settings) const;

    QVariant value(const QString &name) const;
    void setValue(const QString &name, const QVariant &value);
    QStringList options() const;
    QString optionToolTip(const QString &name) const;

    void loadItems(ClipboardModel &model, const QString &id);
    void saveItems(const ClipboardModel &model, const QString &id);
    void removeItems(const QString &id);

    QByteArray windowGeometry( const QString &widget_name = QString(),
                               const QByteArray &geometry = QByteArray() );

    Commands commands() const;
    void addCommand(const Command &cmd);

    void setTabs(const QStringList &tabs);

    static bool defaultCommand(int index, Command *c);

    void decorateBrowser(ClipboardBrowser *c) const;
signals:
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

    explicit ConfigurationManager(QWidget *parent = 0);

    ConfigurationManager(const ConfigurationManager &);
    ConfigurationManager& operator=(const ConfigurationManager &);

    void updateCommandItem(QListWidgetItem *item);
    void shortcutButtonClicked(QPushButton *button);
    void fontButtonClicked(QPushButton *button);
    void colorButtonClicked(QPushButton *button);

private slots:
    void on_pushButtonDown_clicked();
    void on_pushButtonUp_clicked();
    void on_pushButtonRemove_clicked();
    void on_pushButtoAdd_clicked();
    void apply();
    void on_buttonBox_clicked(QAbstractButton* button);
    void onFinished(int result);
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_listWidgetCommands_currentItemChanged(QListWidgetItem *current,
                                                  QListWidgetItem *previous);
    void on_listWidgetCommands_itemChanged(QListWidgetItem *item);
    void on_comboBoxCommands_currentIndexChanged(int index);
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

    void on_checkBoxShowNumber_stateChanged(int arg1);
};

#endif // CONFIGURATIONMANAGER_H
