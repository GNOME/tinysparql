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
import time

#sys.path.insert (0, "../..")

from common.utils.system import TrackerSystemAbstraction
from common.utils.helpers import StoreHelper
from common.utils import configuration as cfg

import unittest2 as ut
#import unittest as ut

class CommonTrackerStoreTest (ut.TestCase):
        """
        Common superclass for tests that just require a fresh store running
        """
        @classmethod 
	def setUpClass (self):
            #print "Starting the daemon in test mode"
            self.system = TrackerSystemAbstraction ()
            self.system.tracker_store_testing_start ()
            time.sleep (1)
            self.tracker = StoreHelper ()

        @classmethod
        def tearDownClass (self):
            #print "Stopping the daemon in test mode (Doing nothing now)"
            self.system.tracker_store_testing_stop ()
            time.sleep (2)
