#!/usr/bin/python

# Copyright (C) 2012, Codethink Ltd. (sam.thursfield@codethink.co.uk)
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
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.

"""
Test miner stability crawling removable devices, which can be disconnected
and reconnected during mining.
"""

from common.utils import configuration as cfg
from common.utils.helpers import MinerFsHelper, StoreHelper, ExtractorHelper, TestMounterHelper, log
from common.utils.minertest import CommonTrackerMinerTest, MINER_TMP_DIR, get_test_uri, get_test_path
from common.utils.system import TrackerSystemAbstraction

import os
import shutil
import time
import unittest2 as ut


CONF_OPTIONS = [
    (cfg.DCONF_MINER_SCHEMA, "enable-writeback", "false"),
    (cfg.DCONF_MINER_SCHEMA, "index-recursive-directories", "[]"),
    (cfg.DCONF_MINER_SCHEMA, "index-single-directories", "[]"),
    (cfg.DCONF_MINER_SCHEMA, "index-optical-discs", "true"),
    (cfg.DCONF_MINER_SCHEMA, "index-removable-devices", "true"),
    ]

REASONABLE_TIMEOUT = 30


class CommonMinerRemovalDevicesTest (CommonTrackerMinerTest):
    # Use the same instances of store and miner-fs for the whole test suite,
    # because they take so long to do first-time init.
    # FIXME: you can move these from all to base class.
    @classmethod
    def setUpClass (self):
        self.system = TrackerSystemAbstraction ()
        self.system.set_up_environment (CONF_OPTIONS, None)

        self.store = StoreHelper ()
        self.store.start ()
        self.miner_fs = MinerFsHelper ()
        self.miner_fs.start ()

        self.extractor = ExtractorHelper ()
        self.extractor.start ()

        self.test_mounter_helper = TestMounterHelper ()
        self.test_mounter_helper.start()


    @classmethod
    def tearDownClass (self):
        self.test_mounter_helper.stop ();
        self.extractor.stop ()
        self.miner_fs.stop ()
        self.store.stop ()


class MinerBasicRemovableDevicesTest (CommonMinerRemovalDevicesTest):
    @classmethod
    def setUpClass (self):
        super(MinerBasicRemovableDevicesTest, self).setUpClass()
        self._create_test_data_simple ()

    def setUp (self):
        pass

    def tearDown (self):
        self.system.unset_up_environment ()

    def test_01_basic (self):
        """
        Ensure USB is crawled correctly.
        """
        data_dir = os.path.join (MINER_TMP_DIR, 'test-monitored')

        self.test_mounter_helper.mount (data_dir);

        if self.miner_fs.wait_for_device_complete () == False:
            self.fail ("Timeout waiting for DeviceCompleted notification")

        self.assertEquals (self._get_n_text_documents (), 3)
        self.test_mounter_helper.unmount (data_dir)

class MinerSlowRemovableDevicesTest (CommonMinerRemovalDevicesTest):
    @classmethod
    def setUpClass (self):
        super(MinerSlowRemovableDevicesTest, self).setUpClass()
        self._create_test_data_many_files ()

    def setUp (self):
        pass

    def tearDown (self):
        self.system.unset_up_environment ()

    def mount_and_remove (self, timeout):
        self.test_mounter_helper.mount (os.path.join (MINER_TMP_DIR, 'slow-extraction-data'))
        time.sleep (timeout)
        self.test_mounter_helper.unmount (os.path.join (MINER_TMP_DIR, 'slow-extraction-data'))
        self.miner_fs.wait_for_idle (3)


    def test_01_quick (self):
        """
        Ensure miner functions correctly when USB is unmounted during extraction
        """

        self.mount_and_remove (2)

        # We shouldn't have finished crawling yet
        self.miner_fs.wait_for_idle ()
        self.assertLess (self._get_n_text_documents (), 10000)

        self.assertEqual (self.miner_fs.completed_devices, 0)

    def test_02_in_the_middle (self):
        """
        Ensure miner functions correctly when USB is unmounted during extraction
        """

        self.mount_and_remove (5)

        # We shouldn't have finished crawling yet
        self.miner_fs.wait_for_idle ()
        self.assertLess (self._get_n_text_documents (), 10000)

        self.assertEqual (self.miner_fs.completed_devices, 0)

    def test_03_complete (self):
        """
        Finally, make sure the miner process hasn't gone crazy
        """

        self.test_mounter_helper.mount (os.path.join (MINER_TMP_DIR, 'slow-extraction-data'))
        self.miner_fs.wait_for_device_complete ()

        print "\n\n\n\nGOT DEVICE COMPLETE\n"

        self.assertEquals (self._get_n_text_documents (), 10000)

        self.test_mounter_helper.unmount (os.path.join (MINER_TMP_DIR, 'slow-extraction-data'))

if __name__ == "__main__":
    ut.main()
