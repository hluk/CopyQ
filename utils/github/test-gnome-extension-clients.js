// Run against the extension on an isolated session bus and GNOME Shell.
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import System from 'system';

const service = 'com.github.hluk.copyq.GnomeClipboard';
const path = '/com/github/hluk/copyq/GnomeClipboard';
const iface = 'com.github.hluk.CopyQ.GnomeClipboard1';
const clientPath = '/com/github/hluk/copyq/GnomeClipboardClient';
const clientIface = 'com.github.hluk.CopyQ.GnomeClipboardClient1';
const clientInfo = Gio.DBusNodeInfo.new_for_xml(`<node><interface name="${clientIface}">
    <method name="ClipboardChanged"><arg type="i" direction="in"/></method>
    </interface></node>`).interfaces[0];
const loop = new GLib.MainLoop(null, false);
let exitCode = 0;

function sleep(ms) {
    return new Promise(resolve => GLib.timeout_add(GLib.PRIORITY_DEFAULT, ms, () => {
        resolve();
        return GLib.SOURCE_REMOVE;
    }));
}

function call(connection, method, parameters = null) {
    return new Promise((resolve, reject) => connection.call(
        service, path, iface, method, parameters, null, Gio.DBusCallFlags.NONE, 3000, null,
        (conn, result) => {
            try { resolve(conn.call_finish(result)); } catch (error) { reject(error); }
        }));
}

async function waitFor(predicate, message) {
    const deadline = GLib.get_monotonic_time() + 3000000;
    while (!predicate()) {
        if (GLib.get_monotonic_time() > deadline)
            throw new Error(message);
        await sleep(10);
    }
}

function publish(text) {
    return call(Gio.DBus.session, 'SetClipboardData',
        new GLib.Variant('(isv)', [0, 'text/plain;charset=utf-8', new GLib.Variant('s', text)]));
}

async function run() {
    const connection = Gio.DBusConnection.new_for_address_sync(
        GLib.getenv('DBUS_SESSION_BUS_ADDRESS'),
        Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION,
        null, null);
    let count = 0;
    let delay = 0;
    const handler = (_conn, _sender, _path, _iface, _method, _parameters, invocation) => {
        ++count;
        if (delay) {
            GLib.timeout_add(GLib.PRIORITY_DEFAULT, delay, () => {
                invocation.return_value(null);
                return GLib.SOURCE_REMOVE;
            });
        } else {
            invocation.return_value(null);
        }
    };
    const registration = connection.register_object(clientPath, clientInfo, handler, null, null);
    try {
        await call(connection, 'RegisterClipboardClient', new GLib.Variant('(i)', [0]));
        delay = 1500; // The extension's notification deadline is 1000 ms.
        await publish('slow-notification');
        await waitFor(() => count === 1, 'Initial notification was not delivered');
        await sleep(1700);
        delay = 0;
        await publish('after-timeout');
        await waitFor(() => count === 2, 'Notification timeout removed a live client');
        print('PASS: a live client still receives notifications after a timeout');
    } finally {
        if (!connection.is_closed()) {
            await call(connection, 'UnregisterClipboardClient');
            connection.unregister_object(registration);
            connection.close_sync(null);
        }
    }
}

run().catch(error => {
    logError(error);
    exitCode = 1;
}).finally(() => loop.quit());
loop.run();
System.exit(exitCode);
