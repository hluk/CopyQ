// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include "item/itemwidget.h"

#include <QDialog>
#include <QHash>

#include <memory>

namespace Ui {
    class ConfigTabGeneral;
    class ConfigTabHistory;
    class ConfigTabLayout;
    class ConfigTabNotifications;
    class ConfigTabTray;
    class ConfigurationManager;
}

class AppConfig;
class ConfigTabAppearance;
class ConfigTabTabs;
class ItemFactory;
class ItemOrderList;
class Option;
class ShortcutsWidget;
class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidgetItem;
class QSpinBox;

/**
 * Configuration dialog.
 */
class ConfigurationManager final : public QDialog
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
    bool setOptionValue(const QString &name, const QVariant &value, AppConfig *appConfig);

    /** Return tooltip text for option with given @a name. */
    QString optionToolTip(const QString &name) const;

    void setVisible(bool visible) override;

    /** Load settings from default file. */
    void loadSettings(AppConfig *appConfig);

    /** Enable/disable autostarting the application. */
    void setAutostartEnable(AppConfig *appConfig);

    void done(int result) override;

signals:
    /** Emitted if configuration changes (after saveSettings() call). */
    void configurationChanged(AppConfig *appConfig);

    void error(const QString &error);

private:
    void connectSlots();

    void apply(AppConfig *appConfig);
    void onButtonBoxClicked(QAbstractButton* button);

    void onCheckBoxMenuTabIsCurrentStateChanged(int);
    void onSpinBoxTrayItemsValueChanged(int value);

    void updateCommandItem(QListWidgetItem *item);
    void shortcutButtonClicked(QObject *button);

    void initTabIcons();

    void initPluginWidgets(ItemFactory *itemFactory);

    void initLanguages();

    /** Update autostarting the application. */
    void updateAutostart();

    void initOptions();

    template <typename Config, typename Widget>
    void bind(Widget *obj);

    template <typename Config>
    void bind();

    void bind(const QString &optionKey, QCheckBox *obj, bool defaultValue);
    void bind(const QString &optionKey, QSpinBox  *obj, int defaultValue);
    void bind(const QString &optionKey, QLineEdit *obj, const QString &defaultValue);
    void bind(const QString &optionKey, QComboBox *obj, int defaultValue);
    void bind(const QString &optionKey, const QVariant &defaultValue, const char *description);

    void updateTabComboBoxes();

    Ui::ConfigurationManager *ui;

    ConfigTabAppearance *m_tabAppearance = nullptr;
    ConfigTabTabs *m_tabTabs = nullptr;
    ItemOrderList *m_tabItems = nullptr;
    ShortcutsWidget *m_tabShortcuts = nullptr;
    std::shared_ptr<Ui::ConfigTabGeneral> m_tabGeneral;
    std::shared_ptr<Ui::ConfigTabHistory> m_tabHistory;
    std::shared_ptr<Ui::ConfigTabLayout> m_tabLayout;
    std::shared_ptr<Ui::ConfigTabNotifications> m_tabNotifications;
    std::shared_ptr<Ui::ConfigTabTray> m_tabTray;

    QHash<QString, Option> m_options;
};

#endif // CONFIGURATIONMANAGER_H
