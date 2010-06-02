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

import dbus
import unittest
import random

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

class TestFTSFunctions (unittest.TestCase):

    def setUp (self):
        bus = dbus.SessionBus ()
        tracker = bus.get_object (TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE);

    def test_unique_insertion (self):
        """
        We actually can't test tracker-miner-fs, so we mimick its behavior in this test
        1. Insert one resource
        2. Update it like tracker-miner-fs does
        3. Check there isn't any duplicate
        4. Clean up
        """

        resource = 'graph://test/resource/1'

        insert_sparql = """
        DROP GRAPH <graph://test/resource/1>
        INSERT INTO <graph://test/resource/1> {
           _:resource a nie:DataObject ;
                      nie:url "%s" .
        }
        """ % resource

        select_sparql = """
        SELECT ?u { ?u nie:url "%s" }
        """ % resource

        delete_sparql = """
        DROP GRAPH <graph://test/resource/1>
        """

        ''' First insertion '''
        self.resources.SparqlUpdate (insert_sparql)

        results = self.resources.SparqlQuery (select_sparql)
        self.assertEquals (len(results), 1)

        ''' Second insertion / update '''
        self.resources.SparqlUpdate (insert_sparql)

        results = self.resources.SparqlQuery (select_sparql)
        self.assertEquals (len(results), 1)

        ''' Clean up '''
        self.resources.SparqlUpdate (delete_sparql)

        results = self.resources.SparqlQuery (select_sparql)
        self.assertEquals (len(results), 0)


if __name__ == '__main__':
    unittest.main()
