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

#ifndef APPCONFIG_H
#define APPCONFIG_H

#include "common/settings.h"

#include <QVariant>

class QString;

#ifdef Q_OS_WIN
#   define DEFAULT_EDITOR "notepad %1"
#elif defined(Q_OS_MAC)
#   define DEFAULT_EDITOR "open -t %1"
#else
#   define DEFAULT_EDITOR "gedit %1"
#endif

QString defaultClipboardTabName();

namespace Config {

template<typename ValueType>
struct Config {
    typedef ValueType Value;
    static Value defaultValue() { return Value(); }
    static Value value(Value v) { return v; }
};

struct autostart : Config<bool> {
    static QString name() { return "autostart"; }
};

struct maxitems : Config<int> {
    static QString name() { return "maxitems"; }
    static Value defaultValue() { return 200; }
    static Value value(Value v) { return qBound(0, v, 10000); }
};

struct clipboard_tab : Config<QString> {
    static QString name() { return "clipboard_tab"; }
    static Value defaultValue() { return defaultClipboardTabName(); }
};

struct expire_tab : Config<bool> {
    static QString name() { return "expire_tab"; }
};

struct editor : Config<QString> {
    static QString name() { return "editor"; }
    static Value defaultValue() { return DEFAULT_EDITOR; }
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

struct always_on_top : Config<bool> {
    static QString name() { return "always_on_top"; }
};

struct open_windows_on_current_screen : Config<bool> {
    static QString name() { return "open_windows_on_current_screen"; }
    static Value defaultValue() { return true; }
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

struct activate_closes : Config<bool> {
    static QString name() { return "activate_closes"; }
    static Value defaultValue() { return true; }
};

struct activate_focuses : Config<bool> {
    static QString name() { return "activate_focuses"; }
};

struct activate_pastes : Config<bool> {
    static QString name() { return "activate_pastes"; }
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

struct tray_tab : Config<QString> {
    static QString name() { return "tray_tab"; }
};

struct command_history_size : Config<int> {
    static QString name() { return "command_history_size"; }
    static Value defaultValue() { return 100; }
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

struct action_has_input : Config<bool> {
    static QString name() { return "action_has_input"; }
};

struct action_has_output : Config<bool> {
    static QString name() { return "action_has_output"; }
};

struct action_separator : Config<QString> {
    static QString name() { return "action_separator"; }
    static Value defaultValue() { return "\\n"; }
};

struct action_output_tab : Config<QString> {
    static QString name() { return "action_output_tab"; }
};

struct tabs : Config<QStringList> {
    static QString name() { return "tabs"; }
};

} // namespace Config

class AppConfig
{
public:
    enum Category {
        OptionsCategory,
        ThemeCategory
    };

    explicit AppConfig(Category category = OptionsCategory);

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

    bool isOptionOn(const QString &name, bool defaultValue = false) const
    {
        const QVariant value = option(name);
        return value.isValid() ? value.toBool() : defaultValue;
    }

    void setOption(const QString &name, const QVariant &value);

    void removeOption(const QString &name);

private:
    Settings m_settings;
};

#endif // APPCONFIG_H
