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

class TestGroupConcat (unittest.TestCase):

    def setUp (self):
        bus = dbus.SessionBus ()
        tracker = bus.get_object (TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE);

    def test_group_concat (self):
        """
        1. Insert 3 capabilities for a test contact
        2. Retrieve contact/capabilites without group_contact (3 results)
        2. TEST: contact with group_concat capabilities (1 result)
        3. Remove the test contact inserted
        """
        
        uri = "contact://test_group_concat"
        
        insert = """
        INSERT { <%s> a nco:IMContact;
                      nco:fullname \"test_group_concat\";
                      nco:imContactCapability nco:im-capability-text-chat ;
                      nco:imContactCapability nco:im-capability-media-calls ;
                      nco:imContactCapability nco:im-capability-file-transfers .
         }
        """ % (uri)
        self.resources.SparqlUpdate (insert)

        query = """
        SELECT ?c ?capability WHERE {
           ?c a nco:IMContact ;
              nco:fullname \"test_group_concat\";
              nco:imContactCapability ?capability .
        }
        """ 
        results = self.resources.SparqlQuery (query)

        assert len (results) == 3
        group_concat_query = """
        SELECT ?c GROUP_CONCAT (?capability, '|') AS ?cap WHERE {
           ?c a nco:IMContact ;
              nco:fullname \"test_group_concat\";
              nco:imContactCapability ?capability .
        } GROUP BY (?c)
        """ 
        results = self.resources.SparqlQuery (group_concat_query)
        assert len (results) == 1
        
        instances = results[0][1].split ('|')
        assert len (instances) == 3
        
        TEXT_CHAT = "http://www.semanticdesktop.org/ontologies/2007/03/22/nco#im-capability-text-chat"
        MEDIA_CALLS = "http://www.semanticdesktop.org/ontologies/2007/03/22/nco#im-capability-media-calls"
        FILE_TRANSFERS = "http://www.semanticdesktop.org/ontologies/2007/03/22/nco#im-capability-file-transfers"
        assert TEXT_CHAT in instances
        assert MEDIA_CALLS in instances
        assert FILE_TRANSFERS in instances

        
        #self.assertEquals (str(results[0][0]), "test_insertion_1")

        delete = """
        DELETE { <%s> a rdfs:Resource. }
        """ % (uri)
        self.resources.SparqlUpdate (delete)
        

if __name__ == '__main__':
    unittest.main()
