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


    def test_fts_rank (self):
        """
        1. Insert a Contact1 with 'abcdefxyz' as fullname and nickname
        2. Insert a Contact2 with 'abcdefxyz' as fullname
        2. Insert a Contact3 with 'abcdefxyz' as fullname and twice in nickname
        3. Query sorting by fts:rank
           EXPECTED: The 3 contacts in order: 3, 1, 2
        4. Remove the created resources
        """
        insert_sparql = """
        INSERT {
        <contact://test/fts-function/rank/1> a nco:PersonContact ;
                       nco:fullname 'abcdefxyz' ;
                       nco:nickname 'abcdefxyz' .

        <contact://test/fts-function/rank/2> a nco:PersonContact ;
                       nco:fullname 'abcdefxyz' .

        <contact://test/fts-function/rank/3> a nco:PersonContact ;
                       nco:fullname 'abcdefxyz' ;
                       nco:nickname 'abcdefxyz abcdefxyz' .
        }
        """
        self.resources.SparqlUpdate (insert_sparql)

        query = """
        SELECT ?contact WHERE {
           ?contact a nco:PersonContact ;
                fts:match 'abcdefxyz' .
        } ORDER BY DESC (fts:rank(?contact))
        """
        results = self.resources.SparqlQuery (query)

        self.assertEquals (len(results), 3)
        self.assertEquals (results[0][0], "contact://test/fts-function/rank/3")
        self.assertEquals (results[1][0], "contact://test/fts-function/rank/1")
        self.assertEquals (results[2][0], "contact://test/fts-function/rank/2")

        delete_sparql = """
        DELETE {
        <contact://test/fts-function/rank/1> a rdf:Resource .
        <contact://test/fts-function/rank/2> a rdf:Resource .
        <contact://test/fts-function/rank/3> a rdf:Resource .
        }
        """

    def test_fts_offsets (self):
        """
        1. Insert a Contact1 with 'abcdefxyz' as fullname and nickname
        2. Insert a Contact2 with 'abcdefxyz' as fullname
        2. Insert a Contact3 with 'abcdefxyz' as fullname and twice in nickname
        3. Query fts:offsets for 'abcdefxyz'
           EXPECTED: The 3 contacts in insertion order, with 2, 1 and 3 pairs (prop, offset=1) each
        4. Remove the created resources
        """
        insert_sparql = """
        INSERT {
        <contact://test/fts-function/rank/1> a nco:PersonContact ;
                       nco:fullname 'abcdefxyz' ;
                       nco:nickname 'abcdefxyz' .

        <contact://test/fts-function/rank/2> a nco:PersonContact ;
                       nco:fullname 'abcdefxyz' .

        <contact://test/fts-function/rank/3> a nco:PersonContact ;
                       nco:fullname 'abcdefxyz' ;
                       nco:nickname 'abcdefxyz abcdefxyz' .
        }
        """
        self.resources.SparqlUpdate (insert_sparql)

        query = """
        SELECT fts:offsets (?contact) WHERE {
           ?contact a nco:PersonContact ;
                fts:match 'abcdefxyz' .
        } 
        """
        results = self.resources.SparqlQuery (query)
        self.assertEquals (len(results), 3)
        self.assertEquals (len (results[0][0].split(",")), 4) # (u'151,1,161,1')
        self.assertEquals (len (results[1][0].split(",")), 2) # (u'161,1')
        self.assertEquals (len (results[2][0].split(",")), 6) # (u'151,1,151,2,161,1')

        delete_sparql = """
        DELETE {
        <contact://test/fts-function/rank/1> a rdf:Resource .
        <contact://test/fts-function/rank/2> a rdf:Resource .
        <contact://test/fts-function/rank/3> a rdf:Resource .
        }
        """
        
        

if __name__ == '__main__':
    unittest.main()
