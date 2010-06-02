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

class TestCoalesce (unittest.TestCase):

    def setUp (self):
        bus = dbus.SessionBus ()
        tracker = bus.get_object (TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE);

        self.resource_uri = "contact://test_group_concat"

        #
        # nco:nickname and nco:note are not set
        #
        insert = """
        INSERT { <%s> a nco:IMContact;
                      nco:fullname \"full name\" ;
                      nco:nameFamily \"family name\" .
         }
        """ % (self.resource_uri)
        self.resources.SparqlUpdate (insert)

    def tearDown (self):
        delete = """
        DELETE { <%s> a rdfs:Resource. }
        """ % (self.resource_uri)
        self.resources.SparqlUpdate (delete)


        
    def test_coalesce_first_fine (self):
        """
        setUp: Insert a contact with only some text properties set
        1. TEST: run a query with coalesce with the valid value in first position
        tearDown: Remove the test contact inserted
        """

        query = """
        SELECT tracker:coalesce (?full, ?family, ?nickname, ?note, 'test_coalesce') WHERE {
           ?c a nco:IMContact .
           OPTIONAL { ?c nco:fullname ?full }
           OPTIONAL { ?c nco:nameFamily ?family }
           OPTIONAL { ?c nco:nickname ?nickname }
           OPTIONAL { ?c nco:note ?note }
        }
        """ 
        results = self.resources.SparqlQuery (query)
        assert len (results) == 1
        assert results[0][0] == "full name"


    def test_coalesce_second_fine (self):
        """
        setUp: Insert a contact with only some text properties set
        1. TEST: run a query with coalesce. First property NULL, second fine
        tearDown: Remove the test contact inserted
        """

        query = """
        SELECT tracker:coalesce (?nickname, ?family, ?full, ?note, 'test_coalesce') WHERE {
           ?c a nco:IMContact .
           OPTIONAL { ?c nco:fullname ?full }
           OPTIONAL { ?c nco:nameFamily ?family }
           OPTIONAL { ?c nco:nickname ?nickname }
           OPTIONAL { ?c nco:note ?note }
        }
        """ 
        results = self.resources.SparqlQuery (query)
        assert len (results) == 1
        assert results[0][0] == "family name"


    def test_coalesce_none_fine_default (self):
        """
        setUp: Insert a contact with only some text properties set
        1. TEST: run a query with coalesce. all variables NULL, return default value
        tearDown: Remove the test contact inserted
        """

        query = """
        SELECT tracker:coalesce (?nickname, ?note, 'test_coalesce') WHERE {
           ?c a nco:IMContact .
           OPTIONAL { ?c nco:fullname ?full }
           OPTIONAL { ?c nco:nameFamily ?family }
           OPTIONAL { ?c nco:nickname ?nickname }
           OPTIONAL { ?c nco:note ?note }
        }
        """ 
        results = self.resources.SparqlQuery (query)
        assert len (results) == 1
        assert results[0][0] == "test_coalesce"
        

if __name__ == '__main__':
    unittest.main()
