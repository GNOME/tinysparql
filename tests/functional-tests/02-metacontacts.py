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

class TestMetacontacts (unittest.TestCase):

    def setUp (self):
        bus = dbus.SessionBus ()
        tracker = bus.get_object (TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE);

    def test_metacontact_usage (self):
        """
        1. Insert Person and IM Contact with the same Metacontact
        2. Query by metacontact (there should be one)
        3. Add a new IM Contact and link it to the previous Metacontact
        4. Query by metacontact (there should be on)
        3. Remove the instances added
        """
        
        initial_data = """
        INSERT {
        <telephaty:///o/f/t/accounts/ivan_frade_gmail_com> a nco:IMAccount .
        
        nco:default-contact-me nco:hasIMAccount <telephaty:///o/f/t/accounts/ivan_frade_gmail_com>.
        nco:default-contact-me nco:hasIMAccount <telephaty:///o/f/t/accounts/ivan_jabber_org>.

        <urn:uuid:metacontact-ivan> a nco:MetaContact.

        <contact://test_metacontacts/person1> a nco:PersonContact ;
                         nco:metacontact <urn:uuid:metacontact-ivan> ;
                         nco:fullname 'Ivan in local addressbook'.
                         
        <contact://test_metacontacts/im1> a nco:IMContact ;
                         nco:metacontact <urn:uuid:metacontact-ivan> ;
                         nco:fromIMAccount <telephaty:///o/f/t/accounts/ivan_frade_gmail_com> ;
                         nco:fullname 'Ivan at gmail'.
                         }
        """
        self.resources.SparqlUpdate (initial_data)

        query = """
        SELECT ?c WHERE {
           ?c nco:metacontact <urn:uuid:metacontact-ivan> .
        }
        """ 
        results = self.resources.SparqlQuery (query)
        self.assertEquals (len(results), 2)
        
        new_account = """
        INSERT {
        <contact://test_metacontacts/im2> a nco:IMContact ;
                         nco:metacontact <urn:uuid:metacontact-ivan> ;
                         nco:fromIMAccount <telephaty:///o/f/t/accounts/ivan_jabber_org> ;
                         nco:fullname 'Ivan at gmail'.
        }
        """ 
        self.resources.SparqlUpdate (new_account)

        results = self.resources.SparqlQuery (query)
        self.assertEquals (len(results), 3)

        delete = """
        DELETE { 
        <telephaty:///o/f/t/accounts/ivan_frade_gmail_com> a rdfs:Resource .
        
        nco:default-contact-me nco:hasIMAccount <telephaty:///o/f/t/accounts/ivan_frade_gmail_com>.
        nco:default-contact-me nco:hasIMAccount <telephaty:///o/f/t/accounts/ivan_jabber_org>.

        <urn:uuid:metacontact-ivan> a rdfs:Resource.

        <contact://test_metacontacts/person1> a rdfs:Resource.
        <contact://test_metacontacts/im1> a rdfs:Resource .
        <contact://test_metacontacts/im2> a rdfs:Resource .
        }
        """ 
        self.resources.SparqlUpdate (delete)
        
    def test_metacontact_merge (self):
        """
        1. Insert Person  (metacontact A)
        2. Insert IM Contact (no metacontact)
        3. Merge IM Contact into PersonContact (sharing metacontact A)
        4. Remove 
        3. Remove the instances added
        """
        
        initial_data = """
        INSERT {
        <telephaty:///o/f/t/accounts/ivan_frade_gmail_com> a nco:IMAccount .
        
        nco:default-contact-me nco:hasIMAccount <telephaty:///o/f/t/accounts/ivan_frade_gmail_com>.

        <urn:uuid:metacontact-ivan> a nco:MetaContact.

        <contact://test_metacontacts/person1> a nco:PersonContact ;
                         nco:metacontact <urn:uuid:metacontact-ivan> ;
                         nco:fullname 'Ivan in local addressbook'.
                         
        <contact://test_metacontacts/im1> a nco:IMContact ;
                         nco:fromIMAccount <telephaty:///o/f/t/accounts/ivan_frade_gmail_com> ;
                         nco:fullname 'Ivan at gmail'.
                         }
        """
        self.resources.SparqlUpdate (initial_data)

        query = """
        SELECT ?c WHERE {
           ?c nco:metacontact <urn:uuid:metacontact-ivan> .
        }
        """ 
        results = self.resources.SparqlQuery (query)
        self.assertEquals (len(results), 1)
        
        merge = """
        INSERT {
        <contact://test_metacontacts/im1> nco:metacontact <urn:uuid:metacontact-ivan> .
        }
        """ 
        self.resources.SparqlUpdate (merge)

        results = self.resources.SparqlQuery (query)
        self.assertEquals (len(results), 2)

        delete = """
        DELETE { 
        <telephaty:///o/f/t/accounts/ivan_frade_gmail_com> a rdfs:Resource .
        
        nco:default-contact-me nco:hasIMAccount <telephaty:///o/f/t/accounts/ivan_frade_gmail_com>.

        <urn:uuid:metacontact-ivan> a rdfs:Resource.

        <contact://test_metacontacts/person1> a rdfs:Resource.
        <contact://test_metacontacts/im1> a rdfs:Resource .
        }
        """ 
        self.resources.SparqlUpdate (delete)

if __name__ == '__main__':
    unittest.main()
