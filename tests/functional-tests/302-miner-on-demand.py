#!/usr/bin/python

# Copyright (C) 2015, Sam Thursfield (ssssam@gmail.com)
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
Test on-demand indexing of locations.

This feature exists so that users can manually add files to the Tracker
store, or trigger the indexing of a removable device.

Related bugs: https://bugzilla.gnome.org/show_bug.cgi?id=680834
"""

from gi.repository import Gio, GLib

import time
import unittest2 as ut

from common.utils.helpers import log
from common.utils.minertest import CommonTrackerMinerTest, DEFAULT_TEXT, path, uri


class MinerOnDemandIndexingTest (CommonTrackerMinerTest):
#    def test_01_index_file (self):
#        """
#        Indexing and monitoring a file outside the configured locations.
#
#        This can also be done from the commandline with `tracker index FILE`.
#        """
#
#        store = self.system.store
#
#        # This is created by CommonTrackerMinerTest.setup() from
#        # common.utils.minertest module.
#        unmonitored_file = 'test-no-monitored/file0.txt'
#        self.assertFileMissing(uri(unmonitored_file))
#
#        log("Queuing %s for indexing" % uri(unmonitored_file))
#        self.system.miner_fs.index_iface.IndexFile(uri(unmonitored_file))
#        resource_id, resource_urn = store.await_resource_inserted(
#            'nfo:TextDocument',
#            url=uri(unmonitored_file),
#            required_property='nie:plainTextContent')
#        self.assertFileContents(resource_urn, DEFAULT_TEXT)
#
#        # When you pass a file to IndexFile, Tracker doesn't set up a monitor
#        # so changes to the file are ignored. This is a bit inconsistent
#        # compared to how directories are treated. The commented code will
#        # not work, because of that.
#
#        #with open(path(unmonitored_file), 'w') as f:
#        #    f.write('Version 2.0')
#        #store.await_property_changed(resource_id, 'nie:plainTextContent')
#        #self.assertFileContents(resource_urn, 'Version 2.0')
#
#    def test_02_index_directory (self):
#        """
#        Indexing and monitoring a directory outside the configured locations.
#
#        This can also be done from the commandline with `tracker index DIR`.
#        """
#
#        store = self.system.store
#
#        # These are created by CommonTrackerMinerTest.setup() from
#        # common.utils.minertest module.
#        unmonitored_dir = 'test-no-monitored'
#        unmonitored_file = 'test-no-monitored/file0.txt'
#        self.assertFileMissing(uri(unmonitored_dir))
#        self.assertFileMissing(uri(unmonitored_file))
#
#        log("Queuing %s for indexing" % uri(unmonitored_dir))
#        self.system.miner_fs.index_iface.IndexFile(uri(unmonitored_dir))
#        resource_id, resource_urn = store.await_resource_inserted(
#            'nfo:TextDocument',
#            url=uri(unmonitored_file),
#            required_property='nie:plainTextContent')
#        self.assertFileContents(resource_urn, DEFAULT_TEXT)
#
#        with open(path(unmonitored_file), 'w') as f:
#            f.write('Version 2.0')
#        store.await_property_changed(resource_id, 'nie:plainTextContent')
#        self.assertFileContents(resource_urn, 'Version 2.0')

   def test_04_index_directory_for_process(self):
        """
        Indexing a directory tree for a specific D-Bus name.

        The idea is that the indexing stops if the D-Bus name disappears, so
        that indexing of large removable devices can be tied to the lifetime of
        certain applications that let users 'opt in' to indexing a device.
        """

        miner = self.system.miner_fs

        unmonitored_dir = 'test-no-monitored/'
        unmonitored_file = 'test-no-monitored/file0.txt'
        self.assertFileMissing(uri(unmonitored_dir))
        self.assertFileMissing(uri(unmonitored_file))

        # Open a separate connection to the session bus, so we can simulate the
        # process ending again.
        address = Gio.dbus_address_get_for_bus_sync(Gio.BusType.SESSION, None)
        fake_app = Gio.DBusConnection.new_for_address_sync(
            address,
            Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION |
            Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT,
            None, None)
        fake_app.init()

        log("Opened D-Bus connection %s" % fake_app.get_unique_name())

        # FIXME: there must be a better way of using GDBus in Python than
        # this...

        # Index a file for the fake_app process, but then close the fake_app
        # straight away so the file doesn't get indexed. We do this while the
        # miner is paused, because otherwise the file might get indexed before
        # the app disappears, which would cause a spurious test failure.
        DBUS_TIMEOUT = 5
        pause_for_process_args = GLib.Variant('(ss)', (fake_app.get_unique_name(),
            "Avoid test process racing with miner process."))
        cookie = fake_app.call_sync(
            'org.freedesktop.Tracker1.Miner.Files',
            '/org/freedesktop/Tracker1/Miner/Files',
            'org.freedesktop.Tracker1.Miner',
            'PauseForProcess',
            pause_for_process_args, None, 0, DBUS_TIMEOUT, None)

        index_file_for_process_args = GLib.Variant('(s)', (uri(unmonitored_dir),))
        fake_app.call_sync(
            'org.freedesktop.Tracker1.Miner.Files.Index',
            '/org/freedesktop/Tracker1/Miner/Files/Index',
            'org.freedesktop.Tracker1.Miner.Files.Index',
            'IndexFileForProcess',
            index_file_for_process_args, None, 0, DBUS_TIMEOUT, None)

        # This will cause Tracker to resume mining, and also stop indexing the
        # file, possibly.
        log("Closing temporary D-Bus connection.")
        fake_app.close_sync()

        bus2 = Gio.bus_get_sync(Gio.BusType.SESSION)
        print bus2.call_sync('org.freedesktop.DBus', '/', 'org.freedesktop.DBus',
                       'ListNames', None, None, 0, 1, None)

        # We don't have to unpause the miner, it gets resumed 
        # The file should never get indexed, because the process disappeared.
        #miner.miner_fs.Resume(cookie)
        #self.assertFileMissing(uri(unmonitored_file))
        # Currently this passes, when it should actually fail!
        self.system.store.await_resource_inserted('nfo:TextDocument',
                                                  url=uri(unmonitored_file))
        self.assertFilePresent(uri(unmonitored_file))


    # hammer index_file_for_process 50000 times

if __name__ == "__main__":
    ut.main()
