# Copyright (C) 2018-2020, Sam Thursfield <sam@afuera.me.uk>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.


from gi.repository import Gio
from gi.repository import GLib

import logging
import os
import shutil
import signal
import subprocess
import threading

from . import mainloop

DEFAULT_TIMEOUT = 10

log = logging.getLogger(__name__)


class DaemonNotStartedError(Exception):
    pass


def await_bus_name(conn, bus_name, timeout=DEFAULT_TIMEOUT):
    """Blocks until 'bus_name' has an owner."""

    log.info("Blocking until name %s has owner", bus_name)
    loop = mainloop.MainLoop()

    def name_appeared_cb(connection, name, name_owner):
        log.info("Name %s appeared (owned by %s)", name, name_owner)
        loop.quit()

    def timeout_cb():
        log.info("Timeout fired after %s seconds", timeout)
        raise AwaitTimeoutException(
            f"Timeout awaiting bus name '{bus_name}'")

    watch_id = Gio.bus_watch_name_on_connection(
        conn, bus_name, Gio.BusNameWatcherFlags.NONE, name_appeared_cb, None)
    timeout_id = GLib.timeout_add_seconds(timeout, timeout_cb)

    loop.run_checked()

    Gio.bus_unwatch_name(watch_id)
    GLib.source_remove(timeout_id)


class DBusDaemon:
    """A private D-Bus daemon instance."""

    def __init__(self, config_file=None, name='dbus-daemon'):
        self.name = name
        self.config_file = config_file

        self.process = None

        self.address = None
        self.pid = None

        self._gdbus_connection = None
        self._previous_sigterm_handler = None

        self._threads = []

    def get_address(self):
        if self.address is None:
            raise DaemonNotStartedError()
        return self.address

    def create_connection(self):
        """Creates a new GDBusConnection."""
        if self.address is None:
            raise DaemonNotStartedError()
        return Gio.DBusConnection.new_for_address_sync(
            self.address,
            Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT |
            Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)

    def get_connection(self):
        """Returns a shared GDBusConnection for general use."""
        if self._gdbus_connection is None:
            raise DaemonNotStartedError()
        return self._gdbus_connection

    def _dbus_daemon_path(self):
        dbus_daemon = shutil.which('dbus-daemon')

        if dbus_daemon is None:
            raise RuntimeError("Could not find `dbus-daemon` binary in PATH (%s)." % os.environ.get('PATH'))

        return dbus_daemon

    def start(self, env=None, new_session=False):
        dbus_command = [self._dbus_daemon_path(), '--print-address=1', '--print-pid=1']
        if self.config_file:
            dbus_command += ['--config-file=' + self.config_file]
        else:
            dbus_command += ['--session']

        log.debug("Running: %s", dbus_command)
        self.process = subprocess.Popen(
            dbus_command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            start_new_session=new_session)

        self._previous_sigterm_handler = signal.signal(
            signal.SIGTERM, self._sigterm_handler)

        try:
            self.address = self.process.stdout.readline().strip().decode('ascii')
            self.pid = int(self.process.stdout.readline().strip().decode('ascii'))
        except ValueError:
            error = self.process.stderr.read().strip().decode('unicode-escape')
            raise RuntimeError(f"Failed to start D-Bus daemon.\n{error}")

        log.debug("Using new D-Bus session with address '%s' with PID %d",
                    self.address, self.pid)

        stderr_log = logging.getLogger(self.name + '.stderr')
        stdout_log = logging.getLogger(self.name + '.stdout')

        # We must read from the pipes continuously, otherwise the daemon
        # process will block.
        self._threads=[threading.Thread(target=self.pipe_to_log, args=(self.process.stdout, stdout_log), daemon=True),
                        threading.Thread(target=self.pipe_to_log, args=(self.process.stderr, stderr_log), daemon=True)]
        self._threads[0].start()
        self._threads[1].start()

        self._gdbus_connection = self.create_connection()

        log.debug("Pinging the new D-Bus daemon...")
        self.ping_sync()

        # Uncomment to output D-Bus events to the console.
        #monitor = subprocess.Popen(['dbus-monitor', '--address', self.address])

    def stop(self):
        if self._previous_sigterm_handler:
            signal.signal(signal.SIGTERM, self._previous_sigterm_handler)
            self._previous_sigterm_handler = None
        if self.process:
            log.debug("  Stopping DBus daemon")
            self.process.terminate()
            self.process.wait()
            self.process = None
        if len(self._threads) > 0:
            log.debug("  Stopping %i pipe reader threads", len(self._threads))
            for thread in self._threads:
                thread.join()
            self.threads = []
        log.debug("DBus daemon stopped")

    def pipe_to_log(self, pipe, dbuslog):
        """This function processes the output from our dbus-daemon instance."""
        while True:
            line_raw = pipe.readline()

            if len(line_raw) == 0:
                break

            line = line_raw.decode('utf-8').rstrip()

            if line.startswith('(tracker-'):
                # We set G_MESSAGES_PREFIXED=all, meaning that all log messages
                # output by Tracker processes have a prefix. Note that
                # g_print() will NOT be captured here.
                dbuslog.info(line)
            else:
                # Log messages from other daemons, including the dbus-daemon
                # itself, go here. Any g_print() messages also end up here.
                dbuslog.debug(line)
        log.debug("Thread stopped")

        # I'm not sure why this is needed, or if it's correct, but without it
        # we see warnings like this:
        #
        #    ResourceWarning: unclosed file <_io.BufferedReader name=3>
        pipe.close()

    def _sigterm_handler(self, signal, frame):
        log.info("Received signal %s", signal)
        self.stop()

    def ping_sync(self):
        """Call the daemon Ping() method to check that it is alive."""
        self._gdbus_connection.call_sync(
            'org.freedesktop.DBus', '/', 'org.freedesktop.DBus', 'GetId',
            None, None, Gio.DBusCallFlags.NONE, 10000, None)

    def list_names_sync(self):
        """Get the name of every client connected to the bus."""
        conn = self.get_connection()
        result = conn.call_sync('org.freedesktop.DBus',
                                '/org/freedesktop/DBus',
                                'org.freedesktop.DBus', 'ListNames', None,
                                GLib.VariantType('(as)'),
                                Gio.DBusCallFlags.NONE, -1, None)
        return result[0]

    def get_connection_unix_process_id_sync(self, name):
        """Get the process ID for one of the names connected to the bus."""
        conn = self.get_connection()
        try:
            result = conn.call_sync('org.freedesktop.DBus',
                                    '/org/freedesktop/DBus',
                                    'org.freedesktop.DBus',
                                    'GetConnectionUnixProcessID',
                                    GLib.Variant('(s)', [name]),
                                    GLib.VariantType('(u)'),
                                    Gio.DBusCallFlags.NONE, -1, None)
            return result[0]
        except GLib.GError as e:
            if e.message.startswith('GDBus.Error:org.freedesktop.DBus.Error.NameHasNoOwner'):
                # This can happen if a daemon disconnects between when we call
                # list_names_sync() and when we query its PID.
                log.debug("Received %s", e)
                return None
            else:
                raise

    def activate_service(self, bus_name, object_path):
        GDBUS_DEFAULT_TIMEOUT = -1
        self.get_connection().call_sync(
            bus_name, object_path, 'org.freedesktop.DBus.Peer', 'Ping',
            None, None, Gio.DBusCallFlags.NONE, GDBUS_DEFAULT_TIMEOUT, None)
        self.await_bus_name(bus_name)

    def await_bus_name(self, bus_name, timeout=DEFAULT_TIMEOUT):
        await_bus_name(self.get_connection(), bus_name, timeout)
