#!/usr/bin/env python
#
# Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
#
from common.utils import configuration as cfg
from common.utils.system import TrackerSystemAbstraction
from common.utils.helpers import StoreHelper
import unittest2 as ut

from gi.repository import GLib

import shutil
import os

MINER_TMP_DIR = cfg.TEST_MONITORED_TMP_DIR

def path (filename):
    return os.path.join (MINER_TMP_DIR, filename)

def uri (filename):
    return "file://" + os.path.join (MINER_TMP_DIR, filename)


DEFAULT_TEXT = "Some stupid content, to have a test file"

index_dirs = [os.path.join (MINER_TMP_DIR, "test-monitored")]
CONF_OPTIONS = {
    cfg.DCONF_MINER_SCHEMA: {
        'index-recursive-directories': GLib.Variant.new_strv(index_dirs),
        'index-single-directories': GLib.Variant.new_strv([]),
        'index-optical-discs': GLib.Variant.new_boolean(False),
        'index-removable-devices': GLib.Variant.new_boolean(False),
        'throttle': GLib.Variant.new_int32(5),
    }
}


class CommonTrackerMinerTest (ut.TestCase):

    @classmethod
    def __prepare_directories (self):
        #
        #     ~/test-monitored/
        #                     /file1.txt
        #                     /dir1/
        #                          /file2.txt
        #                          /dir2/
        #                               /file3.txt
        #
        #
        #     ~/test-no-monitored/
        #                        /file0.txt
        #
        
        for d in ["test-monitored",
                  "test-monitored/dir1",
                  "test-monitored/dir1/dir2",
                  "test-no-monitored"]:
            directory = os.path.join (MINER_TMP_DIR, d)
            if (os.path.exists (directory)):
                shutil.rmtree (directory)
            os.makedirs (directory)

        for tf in ["test-monitored/file1.txt",
                   "test-monitored/dir1/file2.txt",
                   "test-monitored/dir1/dir2/file3.txt",
                   "test-no-monitored/file0.txt"]:
            testfile = os.path.join (MINER_TMP_DIR, tf)
            if (os.path.exists (testfile)):
                os.remove (testfile)
            f = open (testfile, 'w')
            f.write (DEFAULT_TEXT)
            f.close ()

    
    @classmethod 
    def setUpClass (self):
        #print "Starting the daemon in test mode"
        self.__prepare_directories ()
        
        self.system = TrackerSystemAbstraction ()

        if (os.path.exists (os.getcwd() + "/test-configurations/miner-basic-ops")):
            # Use local directory if available
            confdir = os.getcwd() + "/test-configurations/miner-basic-ops"
        else:
            confdir = os.path.join (cfg.DATADIR, "tracker-tests",
                                    "test-configurations", "miner-basic-ops")
        self.system.tracker_miner_fs_testing_start (CONF_OPTIONS)
        self.system.tracker_miner_fs_wait_for_idle ()
        self.tracker = self.system.store
        
    @classmethod
    def tearDownClass (self):
        #print "Stopping the daemon in test mode (Doing nothing now)"
        self.system.tracker_miner_fs_testing_stop ()

