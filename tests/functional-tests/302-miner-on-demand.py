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

This feature exists so that applications can trigger the indexing of a
removable device.

See: https://bugzilla.gnome.org/show_bug.cgi?id=680834
"""

from gi.repository import Gio

import time
import unittest2 as ut

from common.utils.helpers import log
from common.utils.minertest import CommonTrackerMinerTest, uri


class MinerOnDemandIndexingTest (CommonTrackerMinerTest):
#    def test_01_index_file (self):
#        """
#        Indexing a file outside the configured indexing locations.
#
#        This can also be done from the commandline with `tracker index FILE`.
#        """
#
#        # This is created by CommonTrackerMinerTest.setup() from
#        # common.utils.minertest module.
#        unmonitored_file = 'test-no-monitored/file0.txt'
#
#        self.assertFileMissing(uri(unmonitored_file))
#
#        log("Queuing %s for indexing" % uri(unmonitored_file))
#        self.system.miner_fs.index_iface.IndexFile(uri(unmonitored_file))
#        self.system.store.await_resource_inserted('nfo:TextDocument',
#                                                  url=uri(unmonitored_file))
#
    def test_02_index_file_for_process(self):
        """
        Indexing a directory tree for a specific D-Bus name.

        The idea is that the indexing stops if the D-Bus name disappears, so
        that indexing of large removable devices can be tied to the lifetime of
        certain applications that let users 'opt in' to indexing a device.
        """
        unmonitored_file = 'test-no-monitored/file0.txt'
        self.assertFileMissing(uri(unmonitored_file))

        miner = self.system.miner_fs

        fake_app = Gio.bus_get_sync(Gio.BusType.SESSION)
        log("Opened D-Bus connection %s" % fake_app.get_unique_name())

        # Index a file for the fake_app process, but then close the fake_app
        # straight away so the file doesn't get indexed. We do this while the
        # miner is paused, because otherwise the file might get indexed before
        # the app disappears, which would cause a spurious test failure.
        cookie = miner.miner_fs.PauseForProcess(
            fake_app.get_unique_name(),
            "Avoid test process racing with miner process.")
        miner.index_iface.IndexFile(uri(unmonitored_file))
        fake_app.close()
        log("Closed temporary D-Bus connection.")

        # The file should never get indexed, because the process disappeared.
        miner.miner_fs.Resume(cookie)
        time.sleep(5)
        #self.assertFileMissing(uri(unmonitored_file))
        self.system.store.await_resource_inserted('nfo:TextDocument',
                                                  url=uri(unmonitored_file))
        self.assertFilePresent(uri(unmonitored_file))


    # hammer index_file_for_process 50000 times

if __name__ == "__main__":
    ut.main()
