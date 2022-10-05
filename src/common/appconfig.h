// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPCONFIG_H
#define APPCONFIG_H

#include "common/settings.h"

#include <QVariant>

class QString;

QString defaultClipboardTabName();

namespace Config {

const int maxItems = 10000;

template<typename ValueType>
struct Config {
    using Value = ValueType;
    static Value defaultValue() { return Value(); }
    static Value value(Value v) { return v; }
    static const char *description() { return nullptr; }
};

struct autostart : Config<bool> {
    static QString name() { return "autostart"; }
    static Value defaultValue();
};

struct maxitems : Config<int> {
    static QString name() { return "maxitems"; }
    static Value defaultValue() { return 200; }
    static Value value(Value v) { return qBound(0, v, maxItems); }
};

struct clipboard_tab : Config<QString> {
    static QString name() { return "clipboard_tab"; }
    static Value defaultValue() { return defaultClipboardTabName(); }
};

struct expire_tab : Config<int> {
    static QString name() { return "expire_tab"; }
};

struct editor : Config<QString> {
    static QString name() { return "editor"; }
    static Value defaultValue();
};

struct item_popup_interval : Config<int> {
    static QString name() { return "item_popup_interval"; }
};

struct notification_position : Config<int> {
    static QString name() { return "notification_position"; }
    static Value defaultValue() { return 3; }
};

struct clipboard_notification_lines : Config<int> {
    static QString name() { return "clipboard_notification_lines"; }
    static Value value(Value v) { return qBound(0, v, 10000); }
};

struct notification_horizontal_offset : Config<int> {
    static QString name() { return "notification_horizontal_offset"; }
    static Value defaultValue() { return 10; }
};

struct notification_vertical_offset : Config<int> {
    static QString name() { return "notification_vertical_offset"; }
    static Value defaultValue() { return 10; }
};

struct notification_maximum_width : Config<int> {
    static QString name() { return "notification_maximum_width"; }
    static Value defaultValue() { return 300; }
};

struct notification_maximum_height : Config<int> {
    static QString name() { return "notification_maximum_height"; }
    static Value defaultValue() { return 100; }
};

struct edit_ctrl_return : Config<bool> {
    static QString name() { return "edit_ctrl_return"; }
    static Value defaultValue() { return true; }
};

struct move : Config<bool> {
    static QString name() { return "move"; }
    static Value defaultValue() { return true; }
};

struct show_simple_items : Config<bool> {
    static QString name() { return "show_simple_items"; }
    static Value defaultValue() { return false; }
};

struct number_search : Config<bool> {
    static QString name() { return "number_search"; }
    static Value defaultValue() { return false; }
};

struct check_clipboard : Config<bool> {
    static QString name() { return "check_clipboard"; }
    static Value defaultValue() { return true; }
};

struct confirm_exit : Config<bool> {
    static QString name() { return "confirm_exit"; }
    static Value defaultValue() { return true; }
};

struct vi : Config<bool> {
    static QString name() { return "vi"; }
};

struct save_filter_history : Config<bool> {
    static QString name() { return "save_filter_history"; }
};

struct filter_regular_expression : Config<bool> {
    static QString name() { return "filter_regular_expression"; }
    static Value defaultValue() { return false; }
    static const char *description() {
        return "Use regular expressions to search items";
    }
};

struct filter_case_insensitive : Config<bool> {
    static QString name() { return "filter_case_insensitive"; }
    static Value defaultValue() { return true; }
    static const char *description() {
        return "Use case-insensitive item search";
    }
};

struct always_on_top : Config<bool> {
    static QString name() { return "always_on_top"; }
};

struct close_on_unfocus : Config<bool> {
    static QString name() { return "close_on_unfocus"; }
    static Value defaultValue() { return true; }
};

struct close_on_unfocus_delay_ms : Config<int> {
    static QString name() { return "close_on_unfocus_delay_ms"; }
    static Value defaultValue() { return 500; }
    static const char *description() {
        return "Delay to close the main window when unfocused/deactivated";
    }
};

struct open_windows_on_current_screen : Config<bool> {
    static QString name() { return "open_windows_on_current_screen"; }
    static Value defaultValue() { return true; }
};

struct restore_geometry : Config<bool> {
    static QString name() { return "restore_geometry"; }
    static Value defaultValue() { return true; }
    static const char *description() {
        return "Restore position and size for the main window and other dialogs";
    }
};

struct transparency_focused : Config<int> {
    static QString name() { return "transparency_focused"; }
    static Value value(Value v) { return qBound(0, v, 100); }
};

struct transparency : Config<int> {
    static QString name() { return "transparency"; }
    static Value value(Value v) { return qBound(0, v, 100); }
};

struct hide_tabs : Config<bool> {
    static QString name() { return "hide_tabs"; }
};

struct hide_toolbar : Config<bool> {
    static QString name() { return "hide_toolbar"; }
};

struct hide_toolbar_labels : Config<bool> {
    static QString name() { return "hide_toolbar_labels"; }
    static Value defaultValue() { return true; }
};

struct disable_tray : Config<bool> {
    static QString name() { return "disable_tray"; }
};

struct hide_main_window : Config<bool> {
    static QString name() { return "hide_main_window"; }
};

struct hide_main_window_in_task_bar : Config<bool> {
    static QString name() { return "hide_main_window_in_task_bar"; }
    static Value defaultValue() { return false; }
    static const char *description() {
        return "Avoid showing entry for the main window in the task bar";
    }
};

struct tab_tree : Config<bool> {
    static QString name() { return "tab_tree"; }
};

struct show_tab_item_count : Config<bool> {
    static QString name() { return "show_tab_item_count"; }
};

struct text_wrap : Config<bool> {
    static QString name() { return "text_wrap"; }
    static Value defaultValue() { return true; }
};

struct text_tab_width : Config<int> {
    static QString name() { return "text_tab_width"; }
    static Value defaultValue() { return 8; }
    static const char *description() {
        return "Width of Tab character in number of spaces";
    }
};

struct activate_item_with_single_click : Config<bool> {
    static QString name() { return "activate_item_with_single_click"; }
    static Value defaultValue() { return false; }
};

struct activate_closes : Config<bool> {
    static QString name() { return "activate_closes"; }
    static Value defaultValue() { return true; }
};

struct activate_focuses : Config<bool> {
    static QString name() { return "activate_focuses"; }
    static Value defaultValue() { return true; }
};

struct activate_pastes : Config<bool> {
    static QString name() { return "activate_pastes"; }
    static Value defaultValue() { return true; }
};

struct tray_items : Config<int> {
    static QString name() { return "tray_items"; }
    static Value defaultValue() { return 5; }
    static Value value(Value v) { return qBound(0, v, 99); }
};

struct tray_item_paste : Config<bool> {
    static QString name() { return "tray_item_paste"; }
    static Value defaultValue() { return true; }
};

struct tray_commands : Config<bool> {
    static QString name() { return "tray_commands"; }
    static Value defaultValue() { return true; }
};

struct tray_tab_is_current : Config<bool> {
    static QString name() { return "tray_tab_is_current"; }
    static Value defaultValue() { return true; }
};

struct tray_images : Config<bool> {
    static QString name() { return "tray_images"; }
    static Value defaultValue() { return true; }
};

struct tray_menu_open_on_left_click : Config<bool> {
    static QString name() { return "tray_menu_open_on_left_click"; }
    static Value defaultValue() { return false; }
    static const char *description() {
        return "Open tray menu on left mouse button";
    }
};

struct tray_tab : Config<QString> {
    static QString name() { return "tray_tab"; }
};

struct command_history_size : Config<int> {
    static QString name() { return "command_history_size"; }
    static Value defaultValue() { return 100; }
    static const char *description() {
        return "Number of commands to keep in action dialog history";
    }
};

struct check_selection : Config<bool> {
    static QString name() { return "check_selection"; }
};

struct copy_clipboard : Config<bool> {
    static QString name() { return "copy_clipboard"; }
};

struct copy_selection : Config<bool> {
    static QString name() { return "copy_selection"; }
};

struct run_selection : Config<bool> {
    static QString name() { return "run_selection"; }
    static Value defaultValue() { return true; }
};

struct tabs : Config<QStringList> {
    static QString name() { return "tabs"; }
    static Value value(Value v) {
        v.removeAll(QString());
        v.removeDuplicates();
        return v;
    }
    static const char *description() {
        return "Ordered list of tabs";
    }
};

struct show_advanced_command_settings : Config<bool> {
    static QString name() { return "show_advanced_command_settings"; }
    static Value defaultValue() { return false; }
    static const char *description() {
        return "Show advanced command configuration in Command dialog";
    }
};

struct autocompletion : Config<bool> {
    static QString name() { return "autocompletion"; }
    static Value defaultValue() { return true; }
};

struct max_process_manager_rows : Config<uint> {
    static QString name() { return "max_process_manager_rows"; }
    static Value defaultValue() { return 1000; }
    static const char *description() {
        return "Maximum number of rows in process manager dialog";
    }
};

struct save_delay_ms_on_item_added : Config<int> {
    static QString name() { return "save_delay_ms_on_item_added"; }
    static Value defaultValue() { return 5 * 60 * 1000; }
    static const char *description() {
        return "Tab save delay after an item is added to the tab";
    }
};

struct save_delay_ms_on_item_modified : Config<int> {
    static QString name() { return "save_delay_ms_on_item_modified"; }
    static Value defaultValue() { return 5 * 60 * 1000; }
    static const char *description() {
        return "Tab save delay after an item is modified in the tab";
    }
};

struct save_delay_ms_on_item_removed : Config<int> {
    static QString name() { return "save_delay_ms_on_item_removed"; }
    static Value defaultValue() { return 10 * 60 * 1000; }
    static const char *description() {
        return "Tab save delay after an item is removed from the tab";
    }
};

struct save_delay_ms_on_item_moved : Config<int> {
    static QString name() { return "save_delay_ms_on_item_moved"; }
    static Value defaultValue() { return 30 * 60 * 1000; }
    static const char *description() {
        return "Tab save delay after an item is moved in the tab";
    }
};

struct save_delay_ms_on_item_edited : Config<int> {
    static QString name() { return "save_delay_ms_on_item_edited"; }
    static Value defaultValue() { return 1000; }
    static const char *description() {
        return "Tab save delay after an item is edited in the tab";
    }
};

struct save_on_app_deactivated : Config<bool> {
    static QString name() { return "save_on_app_deactivated"; }
    static Value defaultValue() { return true; }
    static const char *description() {
        return "Save unsaved tabs immediately after the app is deactivated (main window loses focus)";
    }
};

struct native_menu_bar : Config<bool> {
    static QString name() { return "native_menu_bar"; }
#ifdef Q_OS_MAC
    // Native menu bar doesn't show on macOS.
    // See: https://github.com/hluk/CopyQ/issues/1444
    static Value defaultValue() { return false; }
#else
    static Value defaultValue() { return true; }
#endif
    static const char *description() {
        return "Use native menu bar, otherwise the menu bar shows in the main window";
    }
};

struct native_tray_menu : Config<bool> {
    static QString name() { return "native_tray_menu"; }
    static Value defaultValue() { return false; }
    static const char *description() {
        return "Use native tray menu (menu items search may not work)";
    }
};

struct script_paste_delay_ms : Config<int> {
    static QString name() { return "script_paste_delay_ms"; }
    static Value defaultValue() { return 250; }
    static const char *description() {
        return "Delay after pasting from script (when using `paste()`)";
    }
};

struct window_paste_with_ctrl_v_regex : Config<QString> {
    static QString name() { return "window_paste_with_ctrl_v_regex"; }
    static const char *description() {
        return "Regular expression matching window titles where Ctrl+V should be used for pasting"
               " instead of the default Shift+Insert";
    }
};

struct window_wait_before_raise_ms : Config<int> {
    static QString name() { return "window_wait_before_raise_ms"; }
#ifdef Q_OS_WIN
    static Value defaultValue() { return 0; }
#else
    static Value defaultValue() { return 20; }
#endif
    static const char *description() {
        return "Delay before trying to raise target window for pasting";
    }
};

struct window_wait_raised_ms : Config<int> {
    static QString name() { return "window_wait_raised_ms"; }
    static Value defaultValue() { return 150; }
    static const char *description() {
        return "Wait interval for raising target window for pasting";
    }
};

struct window_wait_after_raised_ms : Config<int> {
    static QString name() { return "window_wait_after_raised_ms"; }
#ifdef Q_OS_WIN
    static Value defaultValue() { return 150; }
#elif defined(Q_OS_MAC)
    static Value defaultValue() { return 0; }
#else
    static Value defaultValue() { return 50; }
#endif
    static const char *description() {
        return "Delay after raising target window for pasting";
    }
};

struct window_key_press_time_ms : Config<int> {
    static QString name() { return "window_key_press_time_ms"; }
#ifdef Q_OS_WIN
    static Value defaultValue() { return 0; }
#elif defined(Q_OS_MAC)
    static Value defaultValue() { return 100; }
#else
    static Value defaultValue() { return 50; }
#endif
    static const char *description() {
        return "Interval to keep simulated keys (Shift+Insert) pressed for pasting"
               " (needed for some apps)";
    }
};

struct window_wait_for_modifier_released_ms : Config<int> {
    static QString name() { return "window_wait_for_modifier_released_ms"; }
    static Value defaultValue() { return 2000; }
    static const char *description() {
        return "Wait for any keyboard modifiers to be released before simulating/sending"
               " shortcut for pasting";
    }
};

struct change_clipboard_owner_delay_ms : Config<int> {
    static QString name() { return "change_clipboard_owner_delay_ms"; }
    static Value defaultValue() { return 150; }
    static const char *description() {
        return "Delay to save new clipboard owner window title";
    }
};

struct row_index_from_one : Config<bool> {
    static QString name() { return "row_index_from_one"; }
    static Value defaultValue() { return true; }
    static const char *description() {
        return "Index items/rows in UI starting from 1 instead of 0"
               " (in scripts, rows are always indexed from 0)";
    }
};

struct style : Config<QString> {
    static QString name() { return "style"; }
    static const char *description() {
        return "Current application style (available styles can be listed with `copyq styles` command)";
    }
};

struct native_notifications : Config<bool> {
    static QString name() { return "native_notifications"; }
    static Value defaultValue() { return true; }
};

} // namespace Config

class AppConfig final
{
public:
    QVariant option(const QString &name) const;

    template <typename T>
    T option(const QString &name, T defaultValue) const
    {
        const QVariant value = option(name);
        return value.isValid() ? value.value<T>() : defaultValue;
    }

    template <typename T>
    typename T::Value option() const
    {
        typename T::Value value = option(T::name(), T::defaultValue());
        return T::value(value);
    }

    void setOption(const QString &name, const QVariant &value);

    void removeOption(const QString &name);

    Settings &settings() { return m_settings; }

private:
    Settings m_settings;
};

#endif // APPCONFIG_H
