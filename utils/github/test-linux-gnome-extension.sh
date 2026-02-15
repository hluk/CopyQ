#!/bin/bash
# Runs GNOME extension clipboard integration test.
set -xeuo pipefail

export QT_QPA_PLATFORM=wayland

# Launch in clean environment
if [[ -z "${COPYQ_TESTS_GNOME_EXTENSION_INIT:-}" ]]; then
    tmp_dir="$(mktemp -d)"
    cleanup() {
        while ! rm -rf "$tmp_dir"; do
            sleep 0.2
        done
    }
    trap cleanup EXIT TERM INT HUP
    mkdir -p "$tmp_dir/.config" \
        "$tmp_dir/.local/share" \
        "$tmp_dir/.cache" \
        "$tmp_dir/.runtime"
    chmod 0700 "$tmp_dir/.runtime"
    env --ignore-environment \
        COPYQ_TESTS_GNOME_EXTENSION_INIT=1 \
        HOME="$tmp_dir" \
        COPYQ_LOG_LEVEL="${COPYQ_LOG_LEVEL:-DEBUG}" \
        QT_LOGGING_RULES="${QT_LOGGING_RULES:-*.debug=true;qt.*.debug=false;qt.*.warning=true}" \
        COPYQ_GNOME_EXTENSION_DEBUG="${COPYQ_GNOME_EXTENSION_DEBUG:-1}" \
        COPYQ_SESSION_NAME="${COPYQ_SESSION_NAME:-__COPYQ_GNOMEEXT}" \
        COPYQ_TESTS_EXECUTABLE="$COPYQ_TESTS_EXECUTABLE" \
        COPYQ_TESTS_TESTS_EXECUTABLE="${COPYQ_TESTS_TESTS_EXECUTABLE:-./copyq-tests}" \
        COPYQ_TESTS_SKIP_MULTIPLE_CLIPBOARD_FORMATS=1 \
        XDG_CONFIG_HOME="$tmp_dir/.config" \
        XDG_DATA_HOME="$tmp_dir/.local/share" \
        XDG_DATA_DIRS="$tmp_dir/.local/share" \
        XDG_CACHE_HOME="$tmp_dir/.cache" \
        XDG_RUNTIME_DIR="$tmp_dir/.runtime" \
        LANG="$LANG" \
        dbus-run-session -- "$0" "$@"
    exit
fi

if [[ ! -x "$COPYQ_TESTS_EXECUTABLE" ]]; then
    echo "❌ FAILED: CopyQ executable not found: $COPYQ_TESTS_EXECUTABLE"
    exit 1
fi
if [[ ! -x "$COPYQ_TESTS_TESTS_EXECUTABLE" ]]; then
    echo "❌ FAILED: copyq-tests executable not found: $COPYQ_TESTS_TESTS_EXECUTABLE"
    exit 1
fi

# Install GNOME extension before starting gnome-shell
extension_uuid="copyq-clipboard@hluk.github.com"
prefix_dir="$(cd "$(dirname "$COPYQ_TESTS_EXECUTABLE")/.." && pwd)"
installed_extension_dir="${COPYQ_GNOME_EXTENSION_DIR:-$prefix_dir/share/gnome-shell/extensions/$extension_uuid}"
if [[ ! -d "$installed_extension_dir" ]]; then
    echo "❌ FAILED: Extension not found: $installed_extension_dir"
    exit 1
fi
user_extension_dir="${HOME}/.local/share/gnome-shell/extensions/${extension_uuid}"
mkdir -p "$(dirname "$user_extension_dir")"
rm -rf "$user_extension_dir"
cp -a "$installed_extension_dir" "$user_extension_dir"

# Some CI images miss compiled GNOME schemas even when gnome-shell is installed.
schema_dir=$XDG_DATA_HOME/glib-2.0/schemas
mkdir -p "$schema_dir"
glib-compile-schemas --targetdir="$schema_dir" /usr/share/glib-2.0/schemas
if ! gsettings list-schemas 2>/dev/null | grep -qx 'org.gnome.shell'; then
    echo "❌ FAILED: GNOME schema org.gnome.shell is unavailable"
    exit 1
fi

# Avoid dependency on org.gnome.Shell.Extensions helper in minimal environments.
gsettings set org.gnome.shell disable-user-extensions false || true
gsettings set org.gnome.shell enabled-extensions "['${extension_uuid}']" || true

cleanup() {
    kill ${gnome_pid:-} ${tail_pid:-} ${copyq_pid:-} 2>/dev/null || true
}
trap cleanup EXIT TERM INT HUP

gnome-shell --headless --wayland --wayland-display=copyq-wayland --no-x11 --virtual-monitor 1280x960 &
gnome_pid=$!

check_gnome_running() {
    if ! kill -0 "$gnome_pid"; then
        echo "❌ FAILED: gnome-shell is not running"
        exit 1
    fi
}

export WAYLAND_DISPLAY=copyq-wayland
socket=$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY
for _ in {1..20}; do
    check_gnome_running
    if [[ -S "$socket" ]]; then
        break
    fi
    sleep 1
done

if [[ -S "$socket" ]]; then
    echo "✅ PASSED: Wayland display is available"
else
    echo "❌ FAILED: Wayland display not available"
    exit 1
fi

extention_service_health_check() {
    gdbus call --session \
        --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.NameHasOwner \
        com.github.hluk.copyq.GnomeClipboard | grep -q "true"
}

try=0
while ! extention_service_health_check; do
    check_gnome_running
    try=$((try + 1))
    if (( try > 20 )); then
        echo "❌ FAILED: GNOME extension service health check failed"
        exit 1
    fi
    sleep 0.5
done
echo "✅ PASSED: GNOME extension service is running"

export COPYQ_LOG_FILE="$XDG_RUNTIME_DIR/copyq.log"
touch "$COPYQ_LOG_FILE"
tail -f "$COPYQ_LOG_FILE" &
tail_pid=$!

"$COPYQ_TESTS_EXECUTABLE" 2>/dev/null &
copyq_pid=$!
COPYQ_WAIT_FOR_SERVER_MS=5000 "$COPYQ_TESTS_EXECUTABLE" '
    config("check_selection", true);
    while(!isClipboardMonitorRunning()) {};
    hide();
'

set_clipboard() {
    clipboard_type=$1
    text=$2
    if [[ "$clipboard_type" == CLIPBOARD ]]; then
        clipboard_type_id=0
    else
        clipboard_type_id=1
    fi
    gdbus call --session \
        --dest com.github.hluk.copyq.GnomeClipboard \
        --object-path /com/github/hluk/copyq/GnomeClipboard \
        --method com.github.hluk.CopyQ.GnomeClipboard1.SetClipboardData \
        "$clipboard_type_id" \
        "text/plain;charset=utf-8" \
        "<'$text'>"
}

test_clipboard() {
    clipboard_type=$1
    text=$2
    set_clipboard "$clipboard_type" "$text"
    script="
        for (i=0; i<20; ++i) {
            if (read(0) == '$text') {
                abort();
            }
            sleep(100);
        }
        fail();
    "
    if "$COPYQ_TESTS_EXECUTABLE" "$script"; then
        echo "✅ PASSED: $clipboard_type item propagated from GNOME extension"
    else
        echo "❌ FAILED: $clipboard_type item did not propagate from GNOME extension"
        exit 1
    fi
}

test_clipboard CLIPBOARD 'CLIPBOARD TEXT'
test_clipboard PRIMARY 'PRIMARY TEXT'

"$COPYQ_TESTS_EXECUTABLE" exit

"$COPYQ_TESTS_TESTS_EXECUTABLE" commandHasClipboardFormat commandCopy commandClipboard
