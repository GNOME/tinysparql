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

AMOUNT_OF_TEST_INSTANCES = 100
AMOUNT_OF_QUERIES = 10

class TestConcurrentQuery (unittest.TestCase):
    """
    Send a bunch of queries to the daemon asynchronously, to test the queue
    holding those queries
    """
    def setUp (self):
        self.main_loop = gobject.MainLoop ()
        dbus_loop = DBusGMainLoop(set_as_default=True)

        bus = dbus.SessionBus(mainloop=dbus_loop)
        tracker = bus.get_object(TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE)
        self.mock_data_insert ()
        self.finish_counter = 0
        
    def mock_data_insert (self):
        query = "INSERT {\n"
        for i in range (0, AMOUNT_OF_TEST_INSTANCES):
            query += "<test-09:instance-%d> a nco:PersonContact ; nco:fullname 'moe %d'.\n" % (i, i)
        query += "}"
        self.resources.SparqlUpdate (query)
        
    def mock_data_delete (self):
        query = "DELETE {\n"
        for i in range (0, AMOUNT_OF_TEST_INSTANCES):
            query += "<test-09:instance-%d> a rdfs:Resource.\n" % (i)
        query += "}"
        self.resources.SparqlUpdate (query)

        query = "DELETE {\n"
        for i in range (0, AMOUNT_OF_QUERIES):
            query += "<test-09:picture-%d> a rdfs:Resource.\n" % (i)
        query += "}"
        self.resources.SparqlUpdate (query)


    def test_async_queries (self):
        QUERY = "SELECT ?u WHERE { ?u a nco:PersonContact. FILTER regex (?u, 'test-09:ins')}"
        UPDATE = "INSERT { <test-09:picture-%d> a nmm:Photo. }"
        for i in range (0, AMOUNT_OF_QUERIES):
            self.resources.SparqlQuery (QUERY, reply_handler=self.reply_cb, error_handler=self.error_handler)
            self.resources.SparqlUpdate (UPDATE % (i),
                                         reply_handler=self.update_cb,
                                         error_handler=self.error_handler)
            
        # Safeguard of 50 seconds. The last reply should quit the loop
        gobject.timeout_add_seconds (60, self.timeout_cb)
        self.main_loop.run ()
        
    def reply_cb (self, results):
        self.finish_counter += 1
        assert len (results) == AMOUNT_OF_TEST_INSTANCES
        if (self.finish_counter >= AMOUNT_OF_QUERIES):
            self.timeout_cb ()

    def update_cb (self):
        assert True

    def error_handler (self):
        print "ERROR in DBus call"
        assert False

    def timeout_cb (self):
        self.mock_data_delete ()
        self.main_loop.quit ()
        return False

if __name__ == "__main__":
    unittest.main ()
