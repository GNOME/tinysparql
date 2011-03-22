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

import shutil
import os
import time

APPLICATIONS_TMP_DIR = os.path.join (cfg.TEST_TMP_DIR, "test-applications-monitored")

CONF_OPTIONS = [
    (cfg.DCONF_MINER_SCHEMA, "index-recursive-directories", [APPLICATIONS_TMP_DIR]),
    (cfg.DCONF_MINER_SCHEMA, "index-single-directories", "[]"),
    ]

# Copy rate, 10KBps (1024b/100ms)
SLOWCOPY_RATE = 1024

class CommonTrackerApplicationTest (ut.TestCase):

    def get_urn_count_by_url (self, url):
        select = """
        SELECT ?u WHERE { ?u nie:url \"%s\" }
        """ % (url)
        return len (self.tracker.query (select))


    def get_test_image (self):
        TEST_IMAGE = "test-image-1.jpg"
        return TEST_IMAGE

    def get_test_video (self):
        TEST_VIDEO = "test-video-1.mp4"
        return TEST_VIDEO

    def get_test_music (self):
        TEST_AUDIO =  "test-music-1.mp3"
        return TEST_AUDIO

    def get_data_dir (self):
        return self.datadir

    def get_dest_dir (self):
        return APPLICATIONS_TMP_DIR

    def slowcopy_file_fd (self, src, fdest, rate=SLOWCOPY_RATE):
        """
        @rate: bytes per 100ms
        """
        print "Copying slowly\n '%s' to\n '%s'" % (src, fdest.name)
        fsrc = open (src, 'rb')
        buffer_ = fsrc.read (rate)
        while (buffer_ != ""):
            fdest.write (buffer_)
            time.sleep (0.1)
            buffer_ = fsrc.read (rate)
        fsrc.close ()
        

    def slowcopy_file (self, src, dst, rate=SLOWCOPY_RATE):
        """
        @rate: bytes per 100ms
        """
        fdest = open (dst, 'wb')
        self.slowcopy_file_fd (src, fdest, rate)
        fdest.close ()

    @classmethod
    def setUp (self):
        # Create temp directory to monitor
        if (os.path.exists (APPLICATIONS_TMP_DIR)):
            shutil.rmtree (APPLICATIONS_TMP_DIR)
        os.makedirs (APPLICATIONS_TMP_DIR)

        # Use local directory if available. Installation otherwise.
        if os.path.exists (os.path.join (os.getcwd (),
                                         "test-apps-data")):
            self.datadir = os.path.join (os.getcwd (),
                                         "test-apps-data")
        else:
            self.datadir = os.path.join (cfg.DATADIR,
                                         "tracker-tests",
                                         "test-apps-data")


        self.system = TrackerSystemAbstraction ()
        self.system.tracker_all_testing_start (CONF_OPTIONS)

        # Returns when ready
        self.tracker = StoreHelper ()
        self.tracker.wait ()

        print "Ready to go!"

    @classmethod
    def tearDown (self):
        #print "Stopping the daemon in test mode (Doing nothing now)"
        self.system.tracker_all_testing_stop ()

        # Remove monitored directory
        if (os.path.exists (APPLICATIONS_TMP_DIR)):
            shutil.rmtree (APPLICATIONS_TMP_DIR)
