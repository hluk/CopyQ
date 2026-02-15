import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const SERVICE_NAME = 'com.github.hluk.copyq.GnomeClipboard';
const OBJECT_PATH = '/com/github/hluk/copyq/GnomeClipboard';
const INTERFACE_NAME = 'com.github.hluk.CopyQ.GnomeClipboard1';
const CLIENT_OBJECT_PATH = '/com/github/hluk/copyq/GnomeClipboardClient';
const CLIENT_INTERFACE_NAME = 'com.github.hluk.CopyQ.GnomeClipboardClient1';
const CLIPBOARD_TYPE_CLIPBOARD = 0;
const CLIPBOARD_TYPE_PRIMARY = 1;
const CLIPBOARD_TYPE_BOTH = 2;
const DEBUG = GLib.getenv('COPYQ_GNOME_EXTENSION_DEBUG') === '1';

const INTERFACE_XML = `<node>
  <interface name="${INTERFACE_NAME}">
    <method name="RegisterClipboardClient">
      <arg type="i" name="clipboardTypes" direction="in"/>
    </method>
    <method name="UnregisterClipboardClient"/>
    <method name="SetClipboardData">
      <arg type="i" name="clipboardType" direction="in"/>
      <arg type="s" name="format" direction="in"/>
      <arg type="v" name="value" direction="in"/>
    </method>
    <method name="GetClipboardData">
      <arg type="i" name="clipboardType" direction="in"/>
      <arg type="as" name="formats" direction="in"/>
      <arg type="a{sv}" name="data" direction="out"/>
    </method>
    <method name="GetClipboardFormats">
      <arg type="i" name="clipboardType" direction="in"/>
      <arg type="as" name="formats" direction="out"/>
    </method>
  </interface>
</node>`;

function debug(message) {
    if (DEBUG)
        log(`[copyq-clipboard] ${message}`);
}

function stringToBytes(text) {
    return new TextEncoder().encode(text);
}

function toBytes(value) {
    if (value instanceof GLib.Variant) {
        if (value.is_of_type(new GLib.VariantType('ay')))
            return value.deepUnpack();
        return toBytes(value.deepUnpack());
    }
    if (value instanceof GLib.Bytes)
        return value.toArray();
    if (value instanceof Uint8Array)
        return value;
    if (Array.isArray(value))
        return Uint8Array.from(value);
    if (typeof value === 'string')
        return stringToBytes(value);
    return null;
}

function toDataVariant(value) {
    const bytes = toBytes(value);
    return bytes ? new GLib.Variant('ay', bytes) : null;
}

function metaSelectionTypeFromClipboardType(clipboardType) {
    const enumType = Meta.SelectionType ?? {};
    const primary = enumType.PRIMARY ?? enumType.SELECTION_PRIMARY ?? 0;
    const clipboard = enumType.CLIPBOARD ?? enumType.SELECTION_CLIPBOARD ?? 1;
    return clipboardType === CLIPBOARD_TYPE_PRIMARY ? primary : clipboard;
}

function supportsClipboardType(clipboardTypes, clipboardType) {
    const types = Number(clipboardTypes);
    const type = Number(clipboardType);
    return types === CLIPBOARD_TYPE_BOTH || types === type;
}

export default class CopyqClipboardExtension extends Extension {
    enable() {
        this._clients = new Map();
        this._providedDataByType = {
            [CLIPBOARD_TYPE_CLIPBOARD]: {},
            [CLIPBOARD_TYPE_PRIMARY]: {},
        };

        this._setupListener();

        this._dbusConnection = Gio.DBus.session;
        this._dbusNodeInfo = Gio.DBusNodeInfo.new_for_xml(INTERFACE_XML);
        this._dbusRegistrationId = this._dbusConnection.register_object(
            OBJECT_PATH,
            this._dbusNodeInfo.interfaces[0],
            this._handleMethodCall.bind(this),
            null,
            null
        );
        this._dbusNameOwnerId = Gio.bus_own_name_on_connection(
            this._dbusConnection,
            SERVICE_NAME,
            Gio.BusNameOwnerFlags.REPLACE,
            null,
            null
        );
    }

    disable() {
        if (this._selectionOwnerChangedId && this.selection) {
            this.selection.disconnect(this._selectionOwnerChangedId);
            this._selectionOwnerChangedId = 0;
        }

        if (this._dbusRegistrationId) {
            this._dbusConnection.unregister_object(this._dbusRegistrationId);
            this._dbusRegistrationId = 0;
        }
        if (this._dbusNameOwnerId) {
            Gio.bus_unown_name(this._dbusNameOwnerId);
            this._dbusNameOwnerId = 0;
        }

        this._clients?.clear();
        this._clients = null;
        this._providedDataByType = null;
        this.selection = null;
        this._dbusConnection = null;
        this._dbusNodeInfo = null;
    }

    _setupListener() {
        try {
            const metaDisplay = Shell.Global.get().get_display();
            const selection = metaDisplay?.get_selection?.();
            if (!selection) {
                debug('Failed to set up owner-changed listener: selection unavailable');
                return;
            }
            debug('Setting up Shell selection owner-changed listener');
            this._setupSelectionTracking(selection);
        } catch (error) {
            debug(`Failed to set up owner-changed listener: ${error}`);
        }
    }

    _setupSelectionTracking(selection) {
        this.selection = selection;
        this._selectionOwnerChangedId = selection.connect('owner-changed',
            (_selection, selectionType, _selectionSource) => {
                this._onSelectionChange(selectionType);
            });
    }

    _clipboardTypeFromSelectionType(selectionType) {
        const enumType = Meta.SelectionType ?? {};
        const primary = enumType.PRIMARY ?? enumType.SELECTION_PRIMARY ?? 0;
        const clipboard = enumType.CLIPBOARD ?? enumType.SELECTION_CLIPBOARD ?? 1;
        const value = Number(selectionType);
        const text = String(selectionType).toLowerCase();
        if (value === primary || text.includes('primary'))
            return CLIPBOARD_TYPE_PRIMARY;
        if (value === clipboard || text.includes('clipboard'))
            return CLIPBOARD_TYPE_CLIPBOARD;
        return CLIPBOARD_TYPE_CLIPBOARD;
    }

    _onSelectionChange(selectionType) {
        const clipboardType = this._clipboardTypeFromSelectionType(selectionType);
        debug(`Selection owner changed: selectionType=${selectionType}, clipboardType=${clipboardType}`);
        this._notifyClients(clipboardType);
    }

    _handleMethodCall(_connection, sender, _objectPath, _interfaceName, methodName, parameters, invocation) {
        if (methodName === 'RegisterClipboardClient') {
            const [clipboardTypes] = parameters.deepUnpack();
            const types = Number(clipboardTypes);
            this._clients.set(sender, types);
            debug(`Registered client ${sender} with clipboardTypes=${types}`);
            invocation.return_value(null);
            return;
        }

        if (methodName === 'UnregisterClipboardClient') {
            this._clients.delete(sender);
            debug(`Unregistered client ${sender}`);
            invocation.return_value(null);
            return;
        }

        if (methodName === 'SetClipboardData') {
            const [clipboardType, format, value] = parameters.deepUnpack();
            const type = Number(clipboardType);
            const normalizedValue = toDataVariant(value);
            if (!normalizedValue) {
                debug(`SetClipboardData ignored for clipboardType=${type}, format=${format}: unsupported value`);
                invocation.return_value(null);
                return;
            }
            this._providedDataByType[type][format] = normalizedValue;
            this._setClipboardData(type, format, normalizedValue);
            debug(`SetClipboardData called for clipboardType=${type}, format=${format}`);
            invocation.return_value(null);
            return;
        }

        if (methodName === 'GetClipboardData') {
            const [clipboardType, formats] = parameters.deepUnpack();
            const requestedFormats = new Set(Array.isArray(formats) ? formats : []);
            this._readClipboardData(clipboardType, requestedFormats, dataMap => {
                invocation.return_value(new GLib.Variant('(a{sv})', [dataMap]));
            });
            return;
        }

        if (methodName === 'GetClipboardFormats') {
            const [clipboardType] = parameters.deepUnpack();
            const formats = this._getClipboardFormats(clipboardType);
            invocation.return_value(new GLib.Variant('(as)', [formats]));
        }
    }

    _notifyClients(clipboardType) {
        if (!this._clients || this._clients.size === 0)
            return;
        debug(`Notifying clients for clipboardType=${clipboardType}, clients=${this._clients.size}`);

        for (const [sender, clipboardTypes] of this._clients.entries()) {
            if (!supportsClipboardType(clipboardTypes, clipboardType))
                continue;
            this._dbusConnection.call(
                sender,
                CLIENT_OBJECT_PATH,
                CLIENT_INTERFACE_NAME,
                'ClipboardChanged',
                new GLib.Variant('(i)', [clipboardType]),
                null,
                Gio.DBusCallFlags.NO_AUTO_START,
                1000,
                null,
                (_conn, result) => {
                    try {
                        _conn.call_finish(result);
                    } catch (error) {
                        log(`[copyq-clipboard] Broadcast failed for ${sender}: ${error}`);
                        this._clients.delete(sender);
                    }
                }
            );
        }
    }

    _readClipboardData(clipboardType, requestedFormats, onDone) {
        if (!this.selection) {
            const fallback = this._providedDataByType[clipboardType] ?? {};
            onDone({...fallback});
            return;
        }

        const selectionType = metaSelectionTypeFromClipboardType(clipboardType);
        const advertisedFormats = this.selection.get_mimetypes(selectionType) ?? [];
        const formats = requestedFormats && requestedFormats.size > 0
            ? [...requestedFormats]
            : advertisedFormats;

        if (formats.length === 0) {
            onDone({});
            return;
        }

        const data = {};
        let pending = formats.length;
        const complete = () => {
            pending--;
            if (pending !== 0)
                return;
            for (const format of formats) {
                if (!Object.prototype.hasOwnProperty.call(data, format)
                        && Object.prototype.hasOwnProperty.call(this._providedDataByType[clipboardType], format))
                {
                    data[format] = this._providedDataByType[clipboardType][format];
                }
            }
            onDone(data);
        };

        for (const format of formats) {
            const output = Gio.MemoryOutputStream.new_resizable();
            this.selection.transfer_async(
                selectionType,
                format,
                -1,
                output,
                null,
                (_selection, result) => {
                try {
                    this.selection.transfer_finish(result);
                    const bytes = output.steal_as_bytes()?.toArray?.() ?? [];
                    if (bytes.length > 0)
                        data[format] = new GLib.Variant('ay', Uint8Array.from(bytes));
                } catch (_error) {
                    // Ignore unavailable format and keep fallback behavior.
                }
                complete();
            });
        }
    }

    _getClipboardFormats(clipboardType) {
        const provided = Object.keys(this._providedDataByType[clipboardType] ?? {});
        if (!this.selection)
            return provided;

        const selectionType = metaSelectionTypeFromClipboardType(clipboardType);
        const selectionFormats = this.selection.get_mimetypes(selectionType) ?? [];
        const all = new Set([...selectionFormats, ...provided]);
        return [...all];
    }

    _setClipboardData(clipboardType, format, value) {
        if (!this.selection)
            this._setupListener();

        if (!this.selection) {
            debug(`Failed to set clipboard content: Meta selection unavailable for clipboardType=${clipboardType}`);
            return;
        }

        const bytes = toBytes(value);
        if (!bytes)
            return;

        try {
            const source = Meta.SelectionSourceMemory.new(format, GLib.Bytes.new(bytes));
            this.selection.set_owner(metaSelectionTypeFromClipboardType(clipboardType), source);
            debug(`Set clipboard content using Meta selection source for clipboardType=${clipboardType} format=${format}`);
        } catch (error) {
            debug(`Failed to set clipboard content using Meta selection source: ${error}`);
        }
    }
}
