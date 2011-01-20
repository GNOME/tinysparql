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

def path (filename):
    return os.path.join (APPLICATIONS_TMP_DIR, filename)

def uri (filename):
    return "file://" + os.path.join (APPLICATIONS_TMP_DIR, filename)

# Being rate defined in amount of BYTES per 100ms
def slowcopy (src, dest, rate):
    fsrc = open (src, 'rb')
    fdest = open (dest, 'wb')

    buffer = fsrc.read (rate)
    while (buffer != ""):
        print "Slow write..."
        fdest.write (buffer)
        time.sleep (0.1)
        buffer = fsrc.read (rate)

    fsrc.close ()
    fdest.close ()


class CommonTrackerApplicationTest (ut.TestCase):

    @classmethod
    def setUpClass (self):
        # Create temp directory to monitor
        if (os.path.exists (APPLICATIONS_TMP_DIR)):
            shutil.rmtree (APPLICATIONS_TMP_DIR)
        os.makedirs (APPLICATIONS_TMP_DIR)

        self.system = TrackerSystemAbstraction ()

        if (os.path.exists (os.path.join (os.getcwd(),
                                          "test-configurations",
                                          "applications"))):
            # Use local directory if available
            confdir = os.path.join (os.getcwd(),
                                    "test-configurations",
                                    "applications")
        else:
            confdir = os.path.join (os.getcwd(),
                                    "test-configurations",
                                    "applications")
            confdir = os.path.join (cfg.DATADIR,
                                    "tracker-tests",
                                    "test-configurations",
                                    "applications")

        self.system.tracker_all_testing_start (confdir)

        # Returns when ready
        self.tracker = StoreHelper ()
        self.tracker.wait ()

        print "Ready to go!"
        print "    Using configuration dir at '%s'..." % (confdir)
        print "    Using temp dir at '%s'..." % (APPLICATIONS_TMP_DIR)

    @classmethod
    def tearDownClass (self):
        #print "Stopping the daemon in test mode (Doing nothing now)"
        self.system.tracker_all_testing_stop ()

        # Remove monitored directory
        if (os.path.exists (APPLICATIONS_TMP_DIR)):
            shutil.rmtree (APPLICATIONS_TMP_DIR)
