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


    def test_graph_filter (self):
        """
        1. Insert a contact with different phone numbers from different sources
        2. Query phone numbers of a single graph
           EXPECTED: Only return the phone number from the specified source
        3. Remove the created resources
        """
        insert_sparql = """
        INSERT {
            <tel:+1234567890> a nco:PhoneNumber ;
                nco:phoneNumber '+1234567890' .
            <tel:+1234567891> a nco:PhoneNumber ;
                nco:phoneNumber '+1234567891' .
            <tel:+1234567892> a nco:PhoneNumber ;
                nco:phoneNumber '+1234567892' .
            <contact://test/graph/1> a nco:PersonContact .
            GRAPH <graph://test/graph/0> {
                <contact://test/graph/1> nco:hasPhoneNumber <tel:+1234567890>
            }
            GRAPH <graph://test/graph/1> {
                <contact://test/graph/1> nco:hasPhoneNumber <tel:+1234567891>
            }
            GRAPH <graph://test/graph/2> {
                <contact://test/graph/1> nco:hasPhoneNumber <tel:+1234567892>
            }
        }
        """
        self.resources.SparqlUpdate (insert_sparql)

        query = """
        SELECT ?contact ?number WHERE {
            ?contact a nco:PersonContact
            GRAPH <graph://test/graph/1> {
                ?contact nco:hasPhoneNumber ?number
            }
        } ORDER BY DESC (fts:rank(?contact))
        """
        results = self.resources.SparqlQuery (query)

        self.assertEquals (len(results), 1)
        self.assertEquals (results[0][0], "contact://test/graph/1")
        self.assertEquals (results[0][1], "tel:+1234567891")

        delete_sparql = """
        DELETE {
            <tel:+1234567890> a rdf:Resource .
            <tel:+1234567891> a rdf:Resource .
            <tel:+1234567892> a rdf:Resource .
            <contact://test/graph/1> a rdf:Resource .
        }
        """

    def test_graph_insert_multiple (self):
        """
        1. Insert a contact with the same phone number from different sources
        2. Query graph uri of hasPhoneNumber statement
           EXPECTED: The uri of the first graph that inserted the phone number
        3. Remove the created resources
        """
        insert_sparql = """
        INSERT {
            <tel:+1234567890> a nco:PhoneNumber ;
                nco:phoneNumber '+1234567890' .
            <contact://test/graph/1> a nco:PersonContact .
            GRAPH <graph://test/graph/0> {
                <contact://test/graph/1> nco:hasPhoneNumber <tel:+1234567890>
            }
            GRAPH <graph://test/graph/1> {
                <contact://test/graph/1> nco:hasPhoneNumber <tel:+1234567890>
            }
            GRAPH <graph://test/graph/2> {
                <contact://test/graph/1> nco:hasPhoneNumber <tel:+1234567890>
            }
        }
        """
        self.resources.SparqlUpdate (insert_sparql)

        query = """
        SELECT ?contact ?g WHERE {
            ?contact a nco:PersonContact
            GRAPH ?g {
                ?contact nco:hasPhoneNumber <tel:+1234567890>
            }
        }
        """
        results = self.resources.SparqlQuery (query)
        self.assertEquals (len(results), 1)
        self.assertEquals (results[0][0], "contact://test/graph/1")
        self.assertEquals (results[0][1], "graph://test/graph/0")

        delete_sparql = """
        DELETE {
            <tel:+1234567890> a rdf:Resource .
            <contact://test/graph/1> a rdf:Resource .
        }
        """



if __name__ == '__main__':
    unittest.main()
