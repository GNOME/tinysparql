#!/usr/bin/python

# Copyright (C) 2010, Nokia (ivan.frade@nokia.com)
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
#

from gi.repository import GLib

from common.utils.system import TrackerSystemAbstraction
import shutil
import unittest2 as ut
import os
from common.utils import configuration as cfg
from common.utils.helpers import log
import time

TEST_FILE_JPEG = "writeback-test-1.jpeg"
TEST_FILE_TIFF = "writeback-test-2.tif"
TEST_FILE_PNG = "writeback-test-4.png"

WRITEBACK_TMP_DIR = os.path.join (cfg.TEST_MONITORED_TMP_DIR, "writeback")

index_dirs = [WRITEBACK_TMP_DIR]
CONF_OPTIONS = {
    cfg.DCONF_MINER_SCHEMA: {
        'index-recursive-directories': GLib.Variant.new_strv(index_dirs),
        'index-single-directories': GLib.Variant.new_strv([]),
        'index-optical-discs': GLib.Variant.new_boolean(False),
        'index-removable-devices': GLib.Variant.new_boolean(False),
    }
}


def uri (filename):
    return "file://" + os.path.join (WRITEBACK_TMP_DIR, filename)

class CommonTrackerWritebackTest (ut.TestCase):
    """
    Superclass to share methods. Shouldn't be run by itself.
    Start all processes including writeback, miner pointing to WRITEBACK_TMP_DIR
    """
	     
    @classmethod
    def __prepare_directories (self):
        if (os.path.exists (os.getcwd() + "/test-writeback-data")):
            # Use local directory if available
            datadir = os.getcwd() + "/test-writeback-data"
        else:
            datadir = os.path.join (cfg.DATADIR, "tracker-tests",
                                    "test-writeback-data")

        if not os.path.exists(WRITEBACK_TMP_DIR):
            os.makedirs(WRITEBACK_TMP_DIR)
        else:
            if not os.path.isdir(WRITEBACK_TMP_DIR):
                raise Exception("%s exists already and is not a directory" % WRITEBACK_TMP_DIR)

        for testfile in [TEST_FILE_JPEG, TEST_FILE_PNG,TEST_FILE_TIFF]:
            origin = os.path.join (datadir, testfile)
            log ("Copying %s -> %s" % (origin, WRITEBACK_TMP_DIR))
            shutil.copy (origin, WRITEBACK_TMP_DIR)


    @classmethod 
    def setUpClass (self):
        #print "Starting the daemon in test mode"
        self.__prepare_directories ()
        
        self.system = TrackerSystemAbstraction ()

        self.system.tracker_writeback_testing_start (CONF_OPTIONS)

        def await_resource_extraction(url):
            # Make sure a resource has been crawled by the FS miner and by
            # tracker-extract. The extractor adds nie:contentCreated for
            # image resources, so know once this property is set the
            # extraction is complete.
            self.system.store.await_resource_inserted('nfo:Image', url=url, required_property='nfo:width')

        await_resource_extraction (self.get_test_filename_jpeg())
        await_resource_extraction (self.get_test_filename_tiff())
        await_resource_extraction (self.get_test_filename_png())

        # Returns when ready
        log ("Ready to go!")
        
    @classmethod
    def tearDownClass (self):
        #print "Stopping the daemon in test mode (Doing nothing now)"
        self.system.tracker_writeback_testing_stop ()
    

    @staticmethod
    def get_test_filename_jpeg ():
        return uri (TEST_FILE_JPEG)

    @staticmethod
    def get_test_filename_tiff ():
        return uri (TEST_FILE_TIFF)

    @staticmethod
    def get_test_filename_png ():
        return uri (TEST_FILE_PNG)

    def get_mtime (self, filename):
        return os.stat(filename).st_mtime

    def wait_for_file_change (self, filename, initial_mtime):
        start = time.time()
        while time.time() < start + 5:
            mtime = os.stat(filename).st_mtime
            if mtime > initial_mtime:
                return
            time.sleep(0.2)

        raise Exception(
            "Timeout waiting for %s to be updated (mtime has not changed)" %
            filename)
