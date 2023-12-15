// SPDX-License-Identifier: GPL-3.0-or-later

#include "configurationmanager.h"
#include "ui_configurationmanager.h"

#include "ui_configtabgeneral.h"
#include "ui_configtabhistory.h"
#include "ui_configtablayout.h"
#include "ui_configtabnotifications.h"
#include "ui_configtabtray.h"

#include "common/appconfig.h"
#include "common/command.h"
#include "common/common.h"
#include "common/config.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/option.h"
#include "common/settings.h"
#include "gui/configtabappearance.h"
#include "gui/configtabtabs.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/pluginwidget.h"
#include "gui/shortcutswidget.h"
#include "gui/tabicons.h"
#include "gui/windowgeometryguard.h"
#include "item/clipboardmodel.h"
#include "item/itemdelegate.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "platform/platformnativeinterface.h"

#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QMimeData>
#include <QScrollArea>
#include <QTranslator>

namespace {

class TabItem final : public ItemOrderList::Item {
public:
    explicit TabItem(QWidget *widget) noexcept
        : m_widget(widget)
    {
        m_widget->hide();
    }

    QVariant data() const override { return QVariant(); }

private:
    QWidget *createWidget(QWidget *) override
    {
        return m_widget;
    }

    QWidget *m_widget;
};

template <typename Ui>
ItemOrderList::ItemPtr makeTab(std::shared_ptr<Ui> &ui, QWidget *parent)
{
    Q_ASSERT(!ui);
    auto widget = new QScrollArea(parent);
    ui = std::make_shared<Ui>();
    ui->setupUi(widget);
    return std::make_shared<TabItem>(widget);
}

template <typename Widget>
ItemOrderList::ItemPtr makeTab(Widget **widget, QWidget *parent)
{
    Q_ASSERT(!*widget);
    *widget = new Widget(parent);
    return std::make_shared<TabItem>(*widget);
}

class PluginItem final : public ItemOrderList::Item {
public:
    explicit PluginItem(const ItemLoaderPtr &loader)
        : m_loader(loader)
    {
    }

    QVariant data() const override { return m_loader->id(); }

private:
    QWidget *createWidget(QWidget *parent) override
    {
        return new PluginWidget(m_loader, parent);
    }

    ItemLoaderPtr m_loader;
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
    initTabIcons();
    connectSlots();
    setWindowIcon(appIcon());

    m_tabHistory->spinBoxItems->setMaximum(Config::maxItems);

    if ( itemFactory && itemFactory->hasLoaders() )
        initPluginWidgets(itemFactory);

    initOptions();

    if (itemFactory)
        m_tabAppearance->createPreview(itemFactory);

    AppConfig appConfig;
    loadSettings(&appConfig);

    if (itemFactory)
        m_tabShortcuts->addCommands( itemFactory->commands() );
}

ConfigurationManager::ConfigurationManager()
    : ui(new Ui::ConfigurationManager)
    , m_options()
{
    ui->setupUi(this);
    initTabIcons();
    connectSlots();
    initOptions();
}

ConfigurationManager::~ConfigurationManager()
{
    delete ui;
}

void ConfigurationManager::initTabIcons()
{
    ItemOrderList *list = ui->itemOrderList;
    list->setItemsMovable(false);
    list->appendItem( tr("General"), getIcon("", IconWrench), makeTab(m_tabGeneral, this) );
    list->appendItem( tr("Layout"), getIcon("", IconTableColumns), makeTab(m_tabLayout, this) );
    list->appendItem( tr("History"), getIcon("", IconList), makeTab(m_tabHistory, this) );
    list->appendItem( tr("Tray"), getIcon("", IconInbox), makeTab(m_tabTray, this) );
    list->appendItem( tr("Notifications"), getIcon("", IconCircleInfo), makeTab(m_tabNotifications, this) );
    list->appendItem( tr("Tabs"), getIconFromResources("tab_rename"), makeTab(&m_tabTabs, this) );
    list->appendItem( tr("Items"), getIcon("", IconList), makeTab(&m_tabItems, this) );
    list->appendItem( tr("Shortcuts"), getIcon("", IconKeyboard), makeTab(&m_tabShortcuts, this) );
    list->appendItem( tr("Appearance"), getIcon("", IconImage), makeTab(&m_tabAppearance, this) );
}

void ConfigurationManager::initPluginWidgets(ItemFactory *itemFactory)
{
    m_tabItems->clearItems();
    m_tabItems->setItemsMovable(true);

    for ( const auto &loader : itemFactory->loaders() ) {
        if ( loader->name().isEmpty() )
            continue;

        ItemOrderList::ItemPtr pluginItem(new PluginItem(loader));
        const QIcon icon = getIcon(loader->icon());
        const auto state = loader->isEnabled()
                ? ItemOrderList::Checked
                : ItemOrderList::Unchecked;
        m_tabItems->appendItem( loader->name(), icon, pluginItem, state );
    }
}

void ConfigurationManager::initLanguages()
{
    m_tabGeneral->comboBoxLanguage->addItem("English");
    m_tabGeneral->comboBoxLanguage->setItemData(0, "en");

    const QString currentLocale = QLocale().name();
    bool currentLocaleFound = false; // otherwise not found or partial match ("uk" partially matches locale "uk_UA")
    QSet<QString> languages;

    for ( const auto &path : qApp->property("CopyQ_translation_directories").toStringList() ) {
        for ( const auto &item : QDir(path).entryList(QStringList("copyq_*.qm")) ) {
            const int i = item.indexOf('_');
            const QString locale = item.mid(i + 1, item.lastIndexOf('.') - i - 1);
            const QString language = nativeLanguageName(locale);

            if (!language.isEmpty()) {
                languages.insert(language);
                const int index = m_tabGeneral->comboBoxLanguage->count();
                m_tabGeneral->comboBoxLanguage->addItem(language);
                m_tabGeneral->comboBoxLanguage->setItemData(index, locale);

                if (!currentLocaleFound) {
                    currentLocaleFound = (locale == currentLocale);
                    if (currentLocaleFound || currentLocale.startsWith(locale + "_"))
                        m_tabGeneral->comboBoxLanguage->setCurrentIndex(index);
                }
            }
        }
    }

    m_tabGeneral->comboBoxLanguage->setSizeAdjustPolicy(QComboBox::AdjustToContents);
}

void ConfigurationManager::updateAutostart()
{
    auto platform = platformNativeInterface();

    if ( platform->canAutostart() ) {
        bind<Config::autostart>(m_tabGeneral->checkBoxAutostart);
    } else {
        m_tabGeneral->checkBoxAutostart->hide();
    }
}

void ConfigurationManager::setAutostartEnable(AppConfig *appConfig)
{
    auto platform = platformNativeInterface();
    platform->setAutostartEnabled( appConfig->option<Config::autostart>() );
}

void ConfigurationManager::initOptions()
{
    /* general options */
    bind<Config::autostart>(m_tabGeneral->checkBoxAutostart);
    bind<Config::clipboard_tab>(m_tabHistory->comboBoxClipboardTab->lineEdit());
    bind<Config::maxitems>(m_tabHistory->spinBoxItems);
    bind<Config::expire_tab>(m_tabHistory->spinBoxExpireTab);
    bind<Config::editor>(m_tabHistory->lineEditEditor);
    bind<Config::item_popup_interval>(m_tabNotifications->spinBoxNotificationPopupInterval);
    bind<Config::notification_position>(m_tabNotifications->comboBoxNotificationPosition);
    bind<Config::clipboard_notification_lines>(m_tabNotifications->spinBoxClipboardNotificationLines);
    bind<Config::notification_horizontal_offset>(m_tabNotifications->spinBoxNotificationHorizontalOffset);
    bind<Config::notification_vertical_offset>(m_tabNotifications->spinBoxNotificationVerticalOffset);
    bind<Config::notification_maximum_width>(m_tabNotifications->spinBoxNotificationMaximumWidth);
    bind<Config::notification_maximum_height>(m_tabNotifications->spinBoxNotificationMaximumHeight);
    bind<Config::native_notifications>(m_tabNotifications->checkBoxUseNativeNotifications);
    bind<Config::edit_ctrl_return>(m_tabHistory->checkBoxEditCtrlReturn);
    bind<Config::show_simple_items>(m_tabHistory->checkBoxShowSimpleItems);
    bind<Config::number_search>(m_tabHistory->checkBoxNumberSearch);
    bind<Config::move>(m_tabHistory->checkBoxMove);
    bind<Config::check_clipboard>(m_tabGeneral->checkBoxClip);
    bind<Config::confirm_exit>(m_tabGeneral->checkBoxConfirmExit);
    bind<Config::vi>(m_tabGeneral->checkBoxViMode);
    bind<Config::save_filter_history>(m_tabGeneral->checkBoxSaveFilterHistory);
    bind<Config::autocompletion>(m_tabGeneral->checkBoxAutocompleteCommands);
    bind<Config::always_on_top>(m_tabGeneral->checkBoxAlwaysOnTop);
    bind<Config::close_on_unfocus>(m_tabGeneral->checkBoxCloseOnUnfocus);
    bind<Config::open_windows_on_current_screen>(m_tabGeneral->checkBoxOpenWindowsOnCurrentScreen);
    bind<Config::transparency_focused>(m_tabLayout->spinBoxTransparencyFocused);
    bind<Config::transparency>(m_tabLayout->spinBoxTransparencyUnfocused);
    bind<Config::hide_tabs>(m_tabLayout->checkBoxHideTabs);
    bind<Config::hide_toolbar>(m_tabLayout->checkBoxHideToolbar);
    bind<Config::hide_toolbar_labels>(m_tabLayout->checkBoxHideToolbarLabels);
    bind<Config::disable_tray>(m_tabTray->checkBoxDisableTray);
    bind<Config::hide_main_window>(m_tabLayout->checkBoxHideWindow);
    bind<Config::tab_tree>(m_tabLayout->checkBoxTabTree);
    bind<Config::show_tab_item_count>(m_tabLayout->checkBoxShowTabItemCount);
    bind<Config::text_wrap>(m_tabGeneral->checkBoxTextWrap);

    bind<Config::activate_item_with_single_click>(m_tabHistory->checkBoxSingleClickActivate);

    bind<Config::activate_closes>(m_tabHistory->checkBoxActivateCloses);
    bind<Config::activate_focuses>(m_tabHistory->checkBoxActivateFocuses);
    bind<Config::activate_pastes>(m_tabHistory->checkBoxActivatePastes);

    bind<Config::tray_items>(m_tabTray->spinBoxTrayItems);
    bind<Config::tray_item_paste>(m_tabTray->checkBoxPasteMenuItem);
    bind<Config::tray_commands>(m_tabTray->checkBoxTrayShowCommands);
    bind<Config::tray_tab_is_current>(m_tabTray->checkBoxMenuTabIsCurrent);
    bind<Config::tray_images>(m_tabTray->checkBoxTrayImages);
    bind<Config::tray_tab>(m_tabTray->comboBoxMenuTab->lineEdit());

    /* other options */
    bind<Config::item_data_threshold>();
    bind<Config::command_history_size>();
#ifdef HAS_MOUSE_SELECTIONS
    /* X11 clipboard selection monitoring and synchronization */
    bind<Config::check_selection>(m_tabGeneral->checkBoxSel);
    bind<Config::copy_clipboard>(m_tabGeneral->checkBoxCopyClip);
    bind<Config::copy_selection>(m_tabGeneral->checkBoxCopySel);
    bind<Config::run_selection>(m_tabGeneral->checkBoxRunSel);
#else
    m_tabGeneral->checkBoxCopySel->hide();
    m_tabGeneral->checkBoxSel->hide();
    m_tabGeneral->checkBoxCopyClip->hide();
    m_tabGeneral->checkBoxRunSel->hide();
#endif

    bind<Config::hide_main_window_in_task_bar>();
    bind<Config::max_process_manager_rows>();
    bind<Config::show_advanced_command_settings>();
    bind<Config::text_tab_width>();

    bind<Config::save_delay_ms_on_item_added>();
    bind<Config::save_delay_ms_on_item_modified>();
    bind<Config::save_delay_ms_on_item_removed>();
    bind<Config::save_delay_ms_on_item_moved>();
    bind<Config::save_delay_ms_on_item_edited>();
    bind<Config::save_on_app_deactivated>();
    bind<Config::tray_menu_open_on_left_click>();

    bind<Config::filter_regular_expression>();
    bind<Config::filter_case_insensitive>();

    bind<Config::native_menu_bar>();
    bind<Config::native_tray_menu>();

    bind<Config::script_paste_delay_ms>();

    bind<Config::window_paste_with_ctrl_v_regex>();
    bind<Config::window_wait_before_raise_ms>();
    bind<Config::window_wait_raised_ms>();
    bind<Config::window_wait_after_raised_ms>();
    bind<Config::window_key_press_time_ms>();
    bind<Config::window_wait_for_modifier_released_ms>();

    bind<Config::update_clipboard_owner_delay_ms>();

    bind<Config::style>();

    bind<Config::row_index_from_one>();

    bind<Config::tabs>();

    bind<Config::restore_geometry>();

    bind<Config::close_on_unfocus_delay_ms>();
}

template <typename Config, typename Widget>
void ConfigurationManager::bind(Widget *obj)
{
    bind(Config::name(), obj, Config::defaultValue());
}

template <typename Config>
void ConfigurationManager::bind()
{
    bind(Config::name(), QVariant::fromValue(Config::defaultValue()), Config::description());
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

void ConfigurationManager::bind(const QString &optionKey, const QVariant &defaultValue, const char *description)
{
    m_options[optionKey] = Option(defaultValue, description);
}

void ConfigurationManager::updateTabComboBoxes()
{
    initTabComboBox(m_tabHistory->comboBoxClipboardTab);
    initTabComboBox(m_tabTray->comboBoxMenuTab);
}

QStringList ConfigurationManager::options() const
{
    QStringList options;
    for (auto it = m_options.constBegin(); it != m_options.constEnd(); ++it) {
        const auto &option = it.key();
        if ( it.value().value().canConvert(QVariant::String) )
            options.append(option);
        else if ( it.value().value().canConvert(QVariant::StringList) )
            options.append(option);
    }

    return options;
}

QVariant ConfigurationManager::optionValue(const QString &name) const
{
    return m_options.value(name).value();
}

bool ConfigurationManager::setOptionValue(const QString &name, const QVariant &value, AppConfig *appConfig)
{
    if ( !m_options.contains(name) )
        return false;

    const QString oldValue = optionValue(name).toString();
    Option &option = m_options[name];
    option.setValue(value);
    if ( option.value() == oldValue )
        return false;

    appConfig->setOption(name, option.value());
    option.setValue(appConfig->option(name));
    return option.value() != oldValue;
}

QString ConfigurationManager::optionToolTip(const QString &name) const
{
    return m_options[name].tooltip();
}

void ConfigurationManager::loadSettings(AppConfig *appConfig)
{
    Settings &settings = appConfig->settings();

    settings.beginGroup("Options");
    for (auto it = m_options.begin(); it != m_options.end(); ++it) {
        const auto &option = it.key();
        auto &value = it.value();
        if ( settings.contains(option) ) {
            const auto newValue = settings.value(option);
            if ( !newValue.isValid() || !value.setValue(newValue) )
                log( tr("Invalid value for option \"%1\"").arg(option), LogWarning );
        } else {
            value.reset();
        }
    }
    settings.endGroup();

    settings.beginGroup("Shortcuts");
    m_tabShortcuts->loadShortcuts(settings);
    settings.endGroup();

    settings.beginGroup("Theme");
    m_tabAppearance->loadTheme(settings);
    settings.endGroup();

    m_tabAppearance->setEditor( appConfig->option<Config::editor>() );

    onCheckBoxMenuTabIsCurrentStateChanged( m_tabTray->checkBoxMenuTabIsCurrent->checkState() );

    updateTabComboBoxes();

    updateAutostart();
}

void ConfigurationManager::onButtonBoxClicked(QAbstractButton* button)
{
    int answer;

    switch( ui->buttonBox->buttonRole(button) ) {
    case QDialogButtonBox::ApplyRole: {
        AppConfig appConfig;
        apply(&appConfig);
        emit configurationChanged(&appConfig);
        break;
    }
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
            for (auto it = m_options.begin(); it != m_options.end(); ++it)
                it.value().reset();
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
        initLanguages();
    }
}

void ConfigurationManager::connectSlots()
{
    connect(ui->buttonBox, &QDialogButtonBox::clicked,
            this, &ConfigurationManager::onButtonBoxClicked);
    connect(m_tabTray->checkBoxMenuTabIsCurrent, &QCheckBox::stateChanged,
            this, &ConfigurationManager::onCheckBoxMenuTabIsCurrentStateChanged);
    connect(m_tabTray->spinBoxTrayItems, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &ConfigurationManager::onSpinBoxTrayItemsValueChanged);
}

void ConfigurationManager::apply(AppConfig *appConfig)
{
    Settings &settings = appConfig->settings();

    settings.beginGroup("Options");
    for (auto it = m_options.constBegin(); it != m_options.constEnd(); ++it)
        settings.setValue( it.key(), it.value().value() );
    settings.endGroup();

    m_tabTabs->saveTabs(&settings);

    // Save configuration without command line alternatives only if option widgets are initialized
    // (i.e. clicked OK or Apply in configuration dialog).
    settings.beginGroup("Shortcuts");
    m_tabShortcuts->saveShortcuts(&settings);
    settings.endGroup();

    settings.beginGroup("Theme");
    m_tabAppearance->saveTheme(&settings);
    settings.endGroup();

    // Save settings for each plugin.
    settings.beginGroup("Plugins");

    QStringList pluginPriority;
    pluginPriority.reserve( m_tabItems->itemCount() );

    for (int i = 0; i < m_tabItems->itemCount(); ++i) {
        const QString loaderId = m_tabItems->data(i).toString();
        if ( loaderId.isEmpty() )
            continue;

        pluginPriority.append(loaderId);

        settings.beginGroup(loaderId);

        QWidget *w = m_tabItems->widget(i);
        if (w) {
            PluginWidget *pluginWidget = qobject_cast<PluginWidget *>(w);
            const auto &loader = pluginWidget->loader();
            loader->applySettings(settings);
        }

        const bool isPluginEnabled = m_tabItems->isItemChecked(i);
        settings.setValue("enabled", isPluginEnabled);

        settings.endGroup();
    }

    settings.endGroup();

    if (!pluginPriority.isEmpty())
        settings.setValue("plugin_priority", pluginPriority);

    m_tabAppearance->setEditor( appConfig->option<Config::editor>() );

    setAutostartEnable(appConfig);

    // Language changes after restart.
    const int newLocaleIndex = m_tabGeneral->comboBoxLanguage->currentIndex();
    const QString newLocaleName = m_tabGeneral->comboBoxLanguage->itemData(newLocaleIndex).toString();
    QString oldLocaleName = settings.value("Options/language").toString();
    if (oldLocaleName.isEmpty())
        oldLocaleName = "en";
    const QLocale oldLocale;

    settings.setValue("Options/language", newLocaleName);

    if (QLocale(newLocaleName).name() != oldLocale.name() && newLocaleName != oldLocaleName) {
        QMessageBox::information( this, tr("Restart Required"),
                                  tr("Language will be changed after application is restarted.") );
    }
}

void ConfigurationManager::done(int result)
{
    if (result == QDialog::Accepted) {
        AppConfig appConfig;
        apply(&appConfig);
        emit configurationChanged(&appConfig);
    }

    QDialog::done(result);
}

void ConfigurationManager::onCheckBoxMenuTabIsCurrentStateChanged(int state)
{
    m_tabTray->comboBoxMenuTab->setEnabled(state == Qt::Unchecked);
}

void ConfigurationManager::onSpinBoxTrayItemsValueChanged(int value)
{
    m_tabTray->checkBoxPasteMenuItem->setEnabled(value > 0);
}
