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

#include "configurationmanager.h"
#include "ui_configurationmanager.h"

#include "common/appconfig.h"
#include "common/command.h"
#include "common/common.h"
#include "common/config.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/option.h"
#include "common/settings.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/pluginwidget.h"
#include "gui/tabicons.h"
#include "gui/windowgeometryguard.h"
#include "item/clipboardmodel.h"
#include "item/itemdelegate.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "platform/platformnativeinterface.h"

#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QTranslator>

#ifdef Q_OS_WIN
#   define DEFAULT_EDITOR "notepad %1"
#elif defined(Q_OS_MAC)
#   define DEFAULT_EDITOR "open -t %1"
#else
#   define DEFAULT_EDITOR "gedit %1"
#endif

namespace {

class PluginItem : public ItemOrderList::Item {
public:
    explicit PluginItem(ItemLoaderInterface *loader)
        : m_loader(loader)
    {
    }

    QVariant data() const { return m_loader->id(); }

private:
    QWidget *createWidget(QWidget *parent) const
    {
        return new PluginWidget(m_loader, parent);
    }

    ItemLoaderInterface *m_loader;
};

QString nativeLanguageName(const QString &localeName)
{
    // Traditional Chinese
    if (localeName == "zh_TW")
        return QString::fromUtf8("\xe6\xad\xa3\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87");

    // Simplified Chinese
    if (localeName == "zh_CN")
        return QString::fromUtf8("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87");

    return QLocale(localeName).nativeLanguageName();
}

} // namespace

ConfigurationManager::ConfigurationManager(ItemFactory *itemFactory, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConfigurationManager)
    , m_options()
{
    ui->setupUi(this);
    setWindowIcon(appIcon());

    if ( itemFactory && itemFactory->hasLoaders() )
        initPluginWidgets(itemFactory);
    else
        ui->tabItems->hide();

    initOptions();

    connect( ui->configTabShortcuts, SIGNAL(openCommandDialogRequest()),
             this, SIGNAL(openCommandDialogRequest()));

    ui->configTabAppearance->createPreview(itemFactory);

    loadSettings();
}

ConfigurationManager::~ConfigurationManager()
{
    delete ui;
}

void ConfigurationManager::initTabIcons()
{
    QTabWidget *tw = ui->tabWidget;
    if ( !tw->tabIcon(0).isNull() )
        return;

    tw->setTabIcon( tw->indexOf(ui->tabGeneral), getIcon("", IconWrench) );
    tw->setTabIcon( tw->indexOf(ui->tabLayout), getIcon("", IconColumns) );
    tw->setTabIcon( tw->indexOf(ui->tabHistory), getIcon("", IconListAlt) );
    tw->setTabIcon( tw->indexOf(ui->tabItems), getIcon("", IconThList) );
    tw->setTabIcon( tw->indexOf(ui->tabTray), getIcon("", IconInbox) );
    tw->setTabIcon( tw->indexOf(ui->tabNotifications), getIcon("", IconInfoSign) );
    tw->setTabIcon( tw->indexOf(ui->tabShortcuts), getIcon("", IconKeyboard) );
    tw->setTabIcon( tw->indexOf(ui->tabAppearance), getIcon("", IconPicture) );
}

void ConfigurationManager::initPluginWidgets(ItemFactory *itemFactory)
{
    ui->itemOrderListPlugins->clearItems();

    foreach ( ItemLoaderInterface *loader, itemFactory->loaders() ) {
        ItemOrderList::ItemPtr pluginItem(new PluginItem(loader));
        const QIcon icon = getIcon(loader->icon());
        ui->itemOrderListPlugins->appendItem(
                    loader->name(), itemFactory->isLoaderEnabled(loader), false, icon, pluginItem );
    }
}

void ConfigurationManager::initLanguages()
{
    ui->comboBoxLanguage->addItem("English");
    ui->comboBoxLanguage->setItemData(0, "en");

    const QString currentLocale = QLocale().name();
    bool currentLocaleFound = false; // otherwise not found or partial match ("uk" partially matches locale "uk_UA")
    QSet<QString> languages;

    foreach ( const QString &path, qApp->property("CopyQ_translation_directories").toStringList() ) {
        foreach ( const QString &item, QDir(path).entryList(QStringList("copyq_*.qm")) ) {
            const int i = item.indexOf('_');
            const QString locale = item.mid(i + 1, item.lastIndexOf('.') - i - 1);
            const QString language = nativeLanguageName(locale);

            if (!language.isEmpty()) {
                languages.insert(language);
                const int index = ui->comboBoxLanguage->count();
                ui->comboBoxLanguage->addItem(language);
                ui->comboBoxLanguage->setItemData(index, locale);

                if (!currentLocaleFound) {
                    currentLocaleFound = (locale == currentLocale);
                    if (currentLocaleFound || currentLocale.startsWith(locale + "_"))
                        ui->comboBoxLanguage->setCurrentIndex(index);
                }
            }
        }
    }

    ui->comboBoxLanguage->setSizeAdjustPolicy(QComboBox::AdjustToContents);
}

void ConfigurationManager::updateAutostart()
{
    PlatformPtr platform = createPlatformNativeInterface();

    if ( platform->canAutostart() ) {
        bind<Config::autostart>(ui->checkBoxAutostart);
    } else {
        ui->checkBoxAutostart->hide();
    }
}

void ConfigurationManager::setAutostartEnable()
{
    PlatformPtr platform = createPlatformNativeInterface();
    platform->setAutostartEnabled( AppConfig().option<Config::autostart>() );
}

void ConfigurationManager::initOptions()
{
    /* general options */
    bind<Config::autostart>(ui->checkBoxAutostart);
    bind<Config::clipboard_tab>(ui->comboBoxClipboardTab->lineEdit());
    bind<Config::maxitems>(ui->spinBoxItems);
    bind<Config::expire_tab>(ui->spinBoxExpireTab);
    bind<Config::editor>(ui->lineEditEditor);
    bind<Config::item_popup_interval>(ui->spinBoxNotificationPopupInterval);
    bind<Config::notification_position>(ui->comboBoxNotificationPosition);
    bind<Config::clipboard_notification_lines>(ui->spinBoxClipboardNotificationLines);
    bind<Config::notification_horizontal_offset>(ui->spinBoxNotificationHorizontalOffset);
    bind<Config::notification_vertical_offset>(ui->spinBoxNotificationVerticalOffset);
    bind<Config::notification_maximum_width>(ui->spinBoxNotificationMaximumWidth);
    bind<Config::notification_maximum_height>(ui->spinBoxNotificationMaximumHeight);
    bind<Config::edit_ctrl_return>(ui->checkBoxEditCtrlReturn);
    bind<Config::move>(ui->checkBoxMove);
    bind<Config::check_clipboard>(ui->checkBoxClip);
    bind<Config::confirm_exit>(ui->checkBoxConfirmExit);
    bind<Config::vi>(ui->checkBoxViMode);
    bind<Config::save_filter_history>(ui->checkBoxSaveFilterHistory);
    bind<Config::always_on_top>(ui->checkBoxAlwaysOnTop);
    bind<Config::open_windows_on_current_screen>(ui->checkBoxOpenWindowsOnCurrentScreen);
    bind<Config::transparency_focused>(ui->spinBoxTransparencyFocused);
    bind<Config::transparency>(ui->spinBoxTransparencyUnfocused);
    bind<Config::hide_tabs>(ui->checkBoxHideTabs);
    bind<Config::hide_toolbar>(ui->checkBoxHideToolbar);
    bind<Config::hide_toolbar_labels>(ui->checkBoxHideToolbarLabels);
    bind<Config::disable_tray>(ui->checkBoxDisableTray);
    bind<Config::hide_main_window>(ui->checkBoxHideWindow);
    bind<Config::tab_tree>(ui->checkBoxTabTree);
    bind<Config::show_tab_item_count>(ui->checkBoxShowTabItemCount);
    bind<Config::text_wrap>(ui->checkBoxTextWrap);

    bind<Config::activate_closes>(ui->checkBoxActivateCloses);
    bind<Config::activate_focuses>(ui->checkBoxActivateFocuses);
    bind<Config::activate_pastes>(ui->checkBoxActivatePastes);

    bind<Config::tray_items>(ui->spinBoxTrayItems);
    bind<Config::tray_item_paste>(ui->checkBoxPasteMenuItem);
    bind<Config::tray_commands>(ui->checkBoxTrayShowCommands);
    bind<Config::tray_tab_is_current>(ui->checkBoxMenuTabIsCurrent);
    bind<Config::tray_images>(ui->checkBoxTrayImages);
    bind<Config::tray_tab>(ui->comboBoxMenuTab->lineEdit());

    /* other options */
    bind<Config::command_history_size>();
#ifdef COPYQ_WS_X11
    /* X11 clipboard selection monitoring and synchronization */
    bind<Config::check_selection>(ui->checkBoxSel);
    bind<Config::copy_clipboard>(ui->checkBoxCopyClip);
    bind<Config::copy_selection>(ui->checkBoxCopySel);
#else
    ui->checkBoxCopySel->hide();
    ui->checkBoxSel->hide();
    ui->checkBoxCopyClip->hide();
#endif

    // values of last submitted action
    bind<Config::action_has_input>();
    bind<Config::action_has_output>();
    bind<Config::action_separator>();
    bind<Config::action_output_tab>();
}

template <typename Config, typename Widget>
void ConfigurationManager::bind(Widget *obj)
{
    bind(Config::name(), obj, Config::defaultValue());
}

template <typename Config>
void ConfigurationManager::bind()
{
    bind(Config::name(), QVariant::fromValue(Config::defaultValue()));
}

void ConfigurationManager::bind(const QString &optionKey, QCheckBox *obj, bool defaultValue)
{
    m_options[optionKey] = Option(defaultValue, "checked", obj);
}

void ConfigurationManager::bind(const QString &optionKey, QSpinBox *obj, int defaultValue)
{
    m_options[optionKey] = Option(defaultValue, "value", obj);
}

void ConfigurationManager::bind(const QString &optionKey, QLineEdit *obj, const QString &defaultValue)
{
    m_options[optionKey] = Option(defaultValue, "text", obj);
}

void ConfigurationManager::bind(const QString &optionKey, QComboBox *obj, int defaultValue)
{
    m_options[optionKey] = Option(defaultValue, "currentIndex", obj);
}

void ConfigurationManager::bind(const QString &optionKey, const QVariant &defaultValue)
{
    m_options[optionKey] = Option(defaultValue);
}

void ConfigurationManager::updateTabComboBoxes()
{
    initTabComboBox(ui->comboBoxClipboardTab);
    initTabComboBox(ui->comboBoxMenuTab);
}

QStringList ConfigurationManager::options() const
{
    QStringList options;
    foreach ( const QString &option, m_options.keys() ) {
        if ( m_options[option].value().canConvert(QVariant::String)
             && !optionToolTip(option).isEmpty() )
        {
            options.append(option);
        }
    }

    return options;
}

QString ConfigurationManager::optionValue(const QString &name) const
{
    return m_options.value(name).value().toString();
}

bool ConfigurationManager::setOptionValue(const QString &name, const QString &value)
{
    if ( !m_options.contains(name) )
        return false;

    const QString oldValue = optionValue(name);
    m_options[name].setValue(value);
    if ( optionValue(name) == oldValue )
        return false;

    AppConfig().setOption(name, m_options[name].value());
    emit configurationChanged();
    return true;
}

QString ConfigurationManager::optionToolTip(const QString &name) const
{
    return m_options[name].tooltip();
}

void ConfigurationManager::loadSettings()
{
    QSettings settings;

    settings.beginGroup("Options");
    foreach ( const QString &key, m_options.keys() ) {
        if ( settings.contains(key) ) {
            QVariant value = settings.value(key);
            if ( !value.isValid() || !m_options[key].setValue(value) )
                log( tr("Invalid value for option \"%1\"").arg(key), LogWarning );
        } else {
            m_options[key].reset();
        }
    }
    settings.endGroup();

    settings.beginGroup("Shortcuts");
    ui->configTabShortcuts->loadShortcuts(settings);
    settings.endGroup();

    settings.beginGroup("Theme");
    ui->configTabAppearance->loadTheme(settings);
    settings.endGroup();

    ui->configTabAppearance->setEditor( AppConfig().option<Config::editor>() );

    on_checkBoxMenuTabIsCurrent_stateChanged( ui->checkBoxMenuTabIsCurrent->checkState() );

    updateTabComboBoxes();

    updateAutostart();
}

void ConfigurationManager::on_buttonBox_clicked(QAbstractButton* button)
{
    int answer;

    switch( ui->buttonBox->buttonRole(button) ) {
    case QDialogButtonBox::ApplyRole:
        apply();
        emit configurationChanged();
        break;
    case QDialogButtonBox::AcceptRole:
        accept();
        break;
    case QDialogButtonBox::RejectRole:
        reject();
        break;
    case QDialogButtonBox::ResetRole:
        // ask before resetting values
        answer = QMessageBox::question(
                    this,
                    tr("Reset preferences?"),
                    tr("This action will reset all your preferences (in all tabs) to default values.<br /><br />"
                       "Do you really want to <strong>reset all preferences</strong>?"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            foreach ( const QString &key, m_options.keys() ) {
                m_options[key].reset();
            }
        }
        break;
    default:
        return;
    }
}

void ConfigurationManager::setVisible(bool visible)
{
    QDialog::setVisible(visible);

    if (visible) {
        initTabIcons();
        initLanguages();
    }
}

void ConfigurationManager::apply()
{
    Settings settings;

    settings.beginGroup("Options");
    foreach ( const QString &key, m_options.keys() ) {
        settings.setValue( key, m_options[key].value() );
    }
    settings.endGroup();

    // Save configuration without command line alternatives only if option widgets are initialized
    // (i.e. clicked OK or Apply in configuration dialog).
    settings.beginGroup("Shortcuts");
    ui->configTabShortcuts->saveShortcuts(*settings.settingsData());
    settings.endGroup();

    settings.beginGroup("Theme");
    ui->configTabAppearance->saveTheme(*settings.settingsData());
    settings.endGroup();

    // Save settings for each plugin.
    settings.beginGroup("Plugins");

    QStringList pluginPriority;

    for (int i = 0; i < ui->itemOrderListPlugins->itemCount(); ++i) {
        const QString loaderId = ui->itemOrderListPlugins->data(i).toString();
        Q_ASSERT(!loaderId.isEmpty());

        pluginPriority.append(loaderId);

        settings.beginGroup(loaderId);

        QWidget *w = ui->itemOrderListPlugins->widget(i);
        if (w) {
            PluginWidget *pluginWidget = qobject_cast<PluginWidget *>(w);
            ItemLoaderInterface *loader = pluginWidget->loader();
            const QVariantMap s = loader->applySettings();
            foreach (const QString &name, s.keys())
                settings.setValue(name, s[name]);
        }

        const bool isPluginEnabled = ui->itemOrderListPlugins->isItemChecked(i);
        settings.setValue("enabled", isPluginEnabled);

        settings.endGroup();
    }

    settings.endGroup();

    if (!pluginPriority.isEmpty())
        settings.setValue("plugin_priority", pluginPriority);

    ui->configTabAppearance->setEditor( AppConfig().option<Config::editor>() );

    setAutostartEnable();

    // Language changes after restart.
    const int newLocaleIndex = ui->comboBoxLanguage->currentIndex();
    const QString newLocaleName = ui->comboBoxLanguage->itemData(newLocaleIndex).toString();
    settings.setValue("Options/language", newLocaleName);
    const QLocale oldLocale;
    if (QLocale(newLocaleName).name() != oldLocale.name()) {
        QMessageBox::information( this, tr("Restart Required"),
                                  tr("Language will be changed after application is restarted.") );
    }
}

void ConfigurationManager::done(int result)
{
    if (result == QDialog::Accepted) {
        apply();
        emit configurationChanged();
    }

    QDialog::done(result);
}

void ConfigurationManager::on_checkBoxMenuTabIsCurrent_stateChanged(int state)
{
    ui->comboBoxMenuTab->setEnabled(state == Qt::Unchecked);
}

void ConfigurationManager::on_spinBoxTrayItems_valueChanged(int value)
{
    ui->checkBoxPasteMenuItem->setEnabled(value > 0);
}
