#!/bin/bash
# Runs GNOME extension clipboard integration test.
set -xeuo pipefail

if [[ -z "${COPYQ_TESTS_DBUS_RUNNING:-}" ]]; then
    export COPYQ_TESTS_DBUS_RUNNING=1
    exec dbus-run-session -- "$0" "$@"
fi

copyq_bin="$COPYQ_TESTS_EXECUTABLE"
if [[ ! -x "$copyq_bin" ]]; then
    echo "❌ FAILED: CopyQ executable not found: $copyq_bin"
    exit 1
fi
copyq_tests_bin="${COPYQ_TESTS_TESTS_EXECUTABLE:-./copyq-tests}"
if [[ ! -x "$copyq_tests_bin" ]]; then
    echo "❌ FAILED: copyq-tests executable not found: $copyq_bin"
    exit 1
fi

export COPYQ_LOG_LEVEL=DEBUG
export QT_LOGGING_RULES="${QT_LOGGING_RULES:-*.debug=true;qt.*.debug=false;qt.*.warning=true}"
export COPYQ_GNOME_EXTENSION_DEBUG="${COPYQ_GNOME_EXTENSION_DEBUG:-1}"
export COPYQ_SESSION_NAME="${COPYQ_SESSION_NAME:-__COPYQ_GNOMEEXT}"
export QT_QPA_PLATFORM=wayland

settings_dir="$(mktemp -d)"
item_data_dir="$(mktemp -d)"
schema_dir=""
export COPYQ_SETTINGS_PATH="${COPYQ_SETTINGS_PATH:-$settings_dir}"
export COPYQ_ITEM_DATA_PATH="${COPYQ_ITEM_DATA_PATH:-$item_data_dir}"

# Install GNOME extension before starting gnome-shell
extension_uuid="copyq-clipboard@hluk.github.com"
prefix_dir="$(cd "$(dirname "$copyq_bin")/.." && pwd)"
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
if ! gsettings list-schemas 2>/dev/null | grep -qx 'org.gnome.shell'; then
    sys_schema_dir="/usr/share/glib-2.0/schemas"
    if [[ -d "$sys_schema_dir" ]] && command -v glib-compile-schemas >/dev/null; then
        schema_dir="$(mktemp -d)"
        cp "$sys_schema_dir"/*.xml "$schema_dir"/
        glib-compile-schemas "$schema_dir"
        export GSETTINGS_SCHEMA_DIR="$schema_dir"
    fi
fi

if ! gsettings list-schemas 2>/dev/null | grep -qx 'org.gnome.shell'; then
    echo "❌ FAILED: GNOME schema org.gnome.shell is unavailable"
    exit 1
fi

# Avoid dependency on org.gnome.Shell.Extensions helper in minimal environments.
gsettings set org.gnome.shell disable-user-extensions false || true
gsettings set org.gnome.shell enabled-extensions "['${extension_uuid}']" || true

cleanup() {
    kill ${gnome_pid:-} ${xvfb_pid:-} ${tail_pid:-} ${copyq_pid:-} 2>/dev/null || true
    rm -rf "$settings_dir" "$item_data_dir" "$schema_dir"
}
trap cleanup EXIT TERM INT HUP

export DISPLAY="${DISPLAY:-:99.0}"
Xvfb "${DISPLAY%.*}" -screen 0 1280x960x24 &
xvfb_pid=$!

gnome_shell_help="$(gnome-shell --help 2>&1 || true)"
sm_disable_arg=()
if grep -q -- '--sm-disable' <<<"$gnome_shell_help"; then
    sm_disable_arg+=(--sm-disable)
fi

if grep -q -- '--devkit' <<<"$gnome_shell_help"; then
    gnome-shell --devkit --wayland --wayland-display=copyq-wayland &
else
    gnome-shell --nested --wayland --wayland-display=copyq-wayland "${sm_disable_arg[@]}" &
fi
gnome_pid=$!

export WAYLAND_DISPLAY=copyq-wayland
socket=$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY
for _ in {1..10}; do
    if ! kill -0 "$gnome_pid"; then
        echo "❌ FAILED: gnome-shell is not running"
        exit 1
    fi
    if [[ -S "$socket" ]]; then
        break
    fi
    sleep 0.5
done

if [[ ! -S "$socket" ]]; then
    echo "❌ FAILED: Wayland display not available"
    exit 1
fi

for _ in {1..20}; do
    if gdbus call --session \
        --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.NameHasOwner \
        com.github.hluk.copyq.GnomeClipboard | grep -q "true"; then
        break
    fi
    sleep 0.5
done

if ! gdbus call --session \
    --dest org.freedesktop.DBus \
    --object-path /org/freedesktop/DBus \
    --method org.freedesktop.DBus.NameHasOwner \
    com.github.hluk.copyq.GnomeClipboard | grep -q "true"; then
    echo "❌ FAILED: GNOME clipboard extension service is not active"
    exit 1
fi

export COPYQ_LOG_FILE="$COPYQ_SETTINGS_PATH/copyq.log"
touch "$COPYQ_LOG_FILE"
tail -f "$COPYQ_LOG_FILE" &
tail_pid=$!

"$copyq_bin" 2>/dev/null &
copyq_pid=$!
"$copyq_bin" '
    config("check_selection", true);
    while(!isClipboardMonitorRunning()) {};
    hide();
'

set_clipboard() {
    clipboard_type=$1
    text=$2
    gdbus call --session \
        --dest org.gnome.Shell \
        --object-path /org/gnome/Shell \
        --method org.gnome.Shell.Eval \
        "const St = imports.gi.St; St.Clipboard.get_default().set_text(St.ClipboardType.$clipboard_type, '$text');"
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
    if "$copyq_bin" "$script"; then
        echo "✅ PASSED: $clipboard_type item propagated from GNOME extension"
    else
        echo "❌ FAILED: $clipboard_type item did not propagate from GNOME extension"
        exit 1
    fi
}

# Consume initial clipboard events that are intentionally ignored by CopyQ.
set_clipboard CLIPBOARD '__INITIAL CLIPBOARD__'
set_clipboard PRIMARY '__INITIAL PRIMARY__'
sleep 1

test_clipboard CLIPBOARD 'CLIPBOARD TEXT'
test_clipboard PRIMARY 'PRIMARY TEXT'

"$copyq_bin" exit

"$copyq_tests_bin" commandHasClipboardFormat commandCopy commandClipboard
