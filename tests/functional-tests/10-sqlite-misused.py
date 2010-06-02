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

class TestSqliteMisused (unittest.TestCase):
    """
    Send queries while importing files (in .ttl directory)
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
        self.files_counter = 0
        
    def test_queries_while_import (self):
        for root, dirs, files in os.walk('ttl'):
            for ttl_file in filter (lambda f: f.endswith (".ttl"), files):
                full_path = os.path.abspath(os.path.join (root, ttl_file))
                self.files_counter += 1
                self.resources.Load ("file://" + full_path,
                                     timeout=30000,
                                     reply_handler=self.loaded_success_cb,
                                     error_handler=self.loaded_failed_cb)
        
        gobject.timeout_add_seconds (2, self.run_a_query)
        # Safeguard of 60 seconds. The last reply should quit the loop
        gobject.timeout_add_seconds (60, self.timeout_cb)
        self.main_loop.run ()

    def run_a_query (self):
        QUERY = "SELECT ?u ?title WHERE { ?u a nie:InformationElement; nie:title ?title. }"
        self.resources.SparqlQuery (QUERY, timeout=20000,
                                    reply_handler=self.reply_cb, error_handler=self.error_handler)
        return True
        
    def reply_cb (self, results):
        print "Query replied correctly"

    def error_handler (self, error_msg):
        print "ERROR in DBus call", error_msg

    def loaded_success_cb (self):
        self.files_counter -= 1
        if (self.files_counter == 0):
            print "Last file loaded"
            self.timeout_cb ()
        print "Success loading a file"

    def loaded_failed_cb (self, error):
        print "Failed loading a file"
        assert False

    def timeout_cb (self):
        print "Forced timeout after 60 sec."
        self.main_loop.quit ()
        return False

if __name__ == "__main__":
    unittest.main ()
