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

import sys,os,dbus
import unittest
import time
import random
import configuration
import commands
import signal
import gobject
from dbus.mainloop.glib import DBusGMainLoop

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

# Number of instances per batch
BATCH_SIZE = 3000

class TestSqliteBatchMisused (unittest.TestCase):
    """
    Send big batchSparqlUpdates and run queries at the same time
    Don't run this script directly, use the bash script "force-sqlite-misused.sh" instead
    to configure properly the environment
    """
    def setUp (self):
        self.main_loop = gobject.MainLoop ()
        dbus_loop = DBusGMainLoop(set_as_default=True)

        bus = dbus.SessionBus(mainloop=dbus_loop)
        tracker = bus.get_object(TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE)
        self.batch_counter = 0
        
    def test_queries_while_batch_insert (self):
        for root, dirs, files in os.walk('ttl'):
            for ttl_file in filter (lambda f: f.endswith (".ttl"), files):
                full_path = os.path.abspath(os.path.join (root, ttl_file))
                print full_path

                counter = 0
                current_batch = ""
                for line in open(full_path):
                    if (line.startswith ("@prefix")):
                        continue
                    current_batch += line
                    if len(line) > 1 and line[:-1].endswith ('.'):
                        counter += 1
                
                    if counter == BATCH_SIZE:
                        query = "INSERT {" + current_batch + "}"
                        self.resources.BatchSparqlUpdate (query,
                                                          timeout=20000,
                                                          reply_handler=self.batch_success_cb,
                                                          error_handler=self.batch_failed_cb)
                        self.run_a_query ()
                        counter = 0
                        current_batch = ""
                        self.batch_counter += 1
                        
        
        gobject.timeout_add_seconds (2, self.run_a_query)
        # Safeguard of 60 seconds. The last reply should quit the loop
        gobject.timeout_add_seconds (60, self.timeout_cb)
        self.main_loop.run ()

    def run_a_query (self):
        QUERY = "SELECT ?u ?title WHERE { ?u a nie:InformationElement; nie:title ?title. }"
        self.resources.SparqlQuery (QUERY, timeout=20000, reply_handler=self.reply_cb, error_handler=self.error_handler)
        return True
        
    def reply_cb (self, results):
        print "Query replied correctly"

    def error_handler (self, error_msg):
        print "Query failed", error_msg

    def batch_success_cb (self):
        self.batch_counter -= 1
        if (self.batch_counter == 0):
            print "Last batch was success"
            self.timeout_cb ()
        print "Success processing a batch"

    def batch_failed_cb (self, error):
        print "Failed processing a batch"

    def timeout_cb (self):
        print "Forced timeout after 60 sec."
        self.main_loop.quit ()
        return False

if __name__ == "__main__":
    unittest.main ()
