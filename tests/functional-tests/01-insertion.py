#!/usr/bin/env python2.5

# Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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


import dbus
import unittest
import random

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

class TestInsertion (unittest.TestCase):

    def setUp (self):
        bus = dbus.SessionBus ()
        tracker = bus.get_object (TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE);

    def test_simple_insertion (self):
        """
        1. Insert a InformationElement with title.
        2. TEST: Query the title of that information element
        3. Remove the InformationElement to keep everything as it was before
        """
        
        uri = "tracker://test_insertion_1/" + str(random.randint (0, 100))
        
        insert = """
        INSERT { <%s> a nie:InformationElement;
                      nie:title \"test_insertion_1\". }
        """ % (uri)
        self.resources.SparqlUpdate (insert)

        query = """
        SELECT ?t WHERE {
           <%s> a nie:InformationElement ;
               nie:title ?t .
        }
        """ % (uri)
        results = self.resources.SparqlQuery (query)

        self.assertEquals (str(results[0][0]), "test_insertion_1")

        delete = """
        DELETE { <%s> a nie:InformationElement. }
        """ % (uri)
        self.resources.SparqlUpdate (delete)
        

if __name__ == '__main__':
    unittest.main()
