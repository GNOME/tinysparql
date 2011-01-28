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
from common.utils.system import TrackerSystemAbstraction
import shutil
import unittest2 as ut
import os
from common.utils import configuration as cfg

BASEDIR = os.environ['HOME']

def uri (filename):
    return "file://" + os.path.join (BASEDIR, filename)

class CommonTrackerWritebackTest (ut.TestCase):
    """
    Superclass to share methods. Shouldn't be run by itself.
    Start all processes including writeback, miner pointing to HOME/test-writeback-monitored
    """
	     
    @classmethod
    def __prepare_directories (self):
        #
        #     ~/test-writeback-monitored/
        #     ~/test-writeback-no-monitored/
        #
        
        for d in ["test-writeback-monitored",
                  "test-writeback-no-monitored"]:
            directory = os.path.join (BASEDIR, d)
            if (os.path.exists (directory)):
                shutil.rmtree (directory)
            os.makedirs (directory)


        if (os.path.exists (os.getcwd() + "/test-writeback-data")):
            # Use local directory if available
            datadir = os.getcwd() + "/test-writeback-data"
        else:
            datadir = os.path.join (cfg.DATADIR, "tracker-tests",
                                    "test-writeback-data")

        for root, dirs, testfile in os.walk (datadir):

            def is_valid_file (f):
                return not (tf.endswith ("~") or tf.startswith ("Makefile"))

            valid_files = [os.path.join (root, tf) for tf in testfile if is_valid_file (tf)]
            for f in valid_files:
                print "Copying", f, os.path.join (BASEDIR, "test-writeback-monitored")
                shutil.copy (f, os.path.join (BASEDIR, "test-writeback-monitored"))

    
    @classmethod 
    def setUpClass (self):
        #print "Starting the daemon in test mode"
        self.__prepare_directories ()
        
        self.system = TrackerSystemAbstraction ()

        if (os.path.exists (os.getcwd() + "/test-configurations/writeback")):
            # Use local directory if available
            confdir = os.getcwd() + "/test-configurations/writeback"
        else:
            confdir = os.path.join (cfg.DATADIR, "tracker-tests",
                                    "test-configurations", "writeback")
        self.system.tracker_writeback_testing_start (confdir)
        # Returns when ready
        print "Ready to go!"
        
    @classmethod
    def tearDownClass (self):
        #print "Stopping the daemon in test mode (Doing nothing now)"
        self.system.tracker_writeback_testing_stop ()
    
