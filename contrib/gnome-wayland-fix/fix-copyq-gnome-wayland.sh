#!/usr/bin/env bash
set -euo pipefail

config_dir="${XDG_CONFIG_HOME:-$HOME/.config}"
data_dir="${XDG_DATA_HOME:-$HOME/.local/share}"
copyq_conf="$config_dir/copyq/copyq.conf"
autostart="$config_dir/autostart/copyq.desktop"
launcher_src="/usr/share/applications/com.github.hluk.copyq.desktop"
launcher="$data_dir/applications/com.github.hluk.copyq.desktop"

set_exec() {
    local file="$1" exec_line="$2"
    if grep -q '^Exec=' "$file"; then
        sed -i "s|^Exec=.*|Exec=$exec_line|" "$file"
    else
        printf 'Exec=%s\n' "$exec_line" >> "$file"
    fi
}

set_option() {
    local key="$1" value="$2"
    if grep -q "^$key=" "$copyq_conf"; then
        sed -i "s|^$key=.*|$key=$value|" "$copyq_conf"
    else
        sed -i "/^\[Options\]/a $key=$value" "$copyq_conf"
    fi
}

copyq exit >/dev/null 2>&1 || true
sleep 2

mkdir -p "$config_dir/autostart" "$data_dir/applications"
[ -f "$autostart" ] || cp "$launcher_src" "$autostart"
cp "$autostart" "$autostart.bak"
set_exec "$autostart" "env QT_QPA_PLATFORM=xcb /usr/bin/copyq"

cp "$launcher_src" "$launcher"
set_exec "$launcher" "env QT_QPA_PLATFORM=xcb /usr/bin/copyq --start-server show"

if [ -f "$copyq_conf" ]; then
    cp "$copyq_conf" "$copyq_conf.bak"
    for event in added modified moved removed edited; do
        set_option "save_delay_ms_on_item_$event" 1000
    done
    if grep -q '^show_item_content=' "$copyq_conf"; then
        sed -i 's|^show_item_content=.*|show_item_content=|' "$copyq_conf"
    elif grep -q '^\[Shortcuts\]' "$copyq_conf"; then
        sed -i '/^\[Shortcuts\]/a show_item_content=' "$copyq_conf"
    else
        printf '\n[Shortcuts]\nshow_item_content=\n' >> "$copyq_conf"
    fi
fi

QT_QPA_PLATFORM=xcb setsid /usr/bin/copyq >/dev/null 2>&1 &
echo "CopyQ restarted under XWayland. Backups: $autostart.bak ${copyq_conf}.bak"
