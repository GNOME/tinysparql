#!/usr/bin/python
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

"""
Peculiar Sparql behavour reported in bugs
"""
import sys,os,dbus
import unittest
import time
import random
import string
import datetime

from common.utils import configuration as cfg
import unittest2 as ut
#import unittest as ut
from common.utils.storetest import CommonTrackerStoreTest as CommonTrackerStoreTest
from common.utils.expectedFailure import expectedFailureBug


class TrackerStoreSparqlBugsTests (CommonTrackerStoreTest):

        @expectedFailureBug ("NB#217566")
        def test_01_NB217566_union_exists_filter (self):
                """
                NB217566: Use of UNION in EXISTS in a FILTER breaks filtering 
                """
                content = """
                INSERT {
                    <contact:affiliation> a nco:Affiliation ;
                             nco:hasPhoneNumber
                                  [ a nco:PhoneNumber ; maemo:localPhoneNumber "98653" ] .
                    <contact:test> a nco:PersonContact ;
                             nco:hasAffiliation <contact:affiliation> .
                }
                """
                self.tracker.update (content)

                """ Check that these 3 queries return the same results """
                query1 = """
                SELECT  ?_contact ?n WHERE {
                   ?_contact a nco:PersonContact .
                   {
                     ?_contact nco:hasAffiliation ?a .
                     ?a nco:hasPhoneNumber ?p1 .
                     ?p1 maemo:localPhoneNumber ?n
                   } UNION {
                     ?_contact nco:hasPhoneNumber ?p2 .
                     ?p2 maemo:localPhoneNumber ?n
                   } .
                  FILTER (
                    EXISTS {
                        {
                          ?_contact nco:hasPhoneNumber ?auto81 .
                          ?auto81 maemo:localPhoneNumber ?auto80
                        } UNION {
                          ?_contact nco:hasAffiliation ?auto83 .
                          ?auto83 nco:hasPhoneNumber ?auto84 .
                          ?auto84 maemo:localPhoneNumber ?auto80
                        }
                        FILTER (?auto80 = '98653')
                     }
                  )
                }
                """

                query2 = """
                SELECT ?_contact ?n WHERE {
                    ?_contact a nco:PersonContact .
                    {
                        ?_contact nco:hasAffiliation ?a .
                        ?a nco:hasPhoneNumber ?p1 .
                        ?p1 maemo:localPhoneNumber ?n
                    } UNION {
                        ?_contact nco:hasPhoneNumber ?p2 .
                        ?p2 maemo:localPhoneNumber ?n
                    } .
                    FILTER(?n = '98653')
                }
                """

                query3 = """
                SELECT ?_contact ?n WHERE {
                    ?_contact a nco:PersonContact .
                    {
                        ?_contact nco:hasAffiliation ?a .
                        ?a nco:hasPhoneNumber ?p1 .
                        ?p1 maemo:localPhoneNumber ?n
                    } UNION {
                        ?_contact nco:hasPhoneNumber ?p2 .
                        ?p2 maemo:localPhoneNumber ?n
                    } .
                    FILTER(
                        EXISTS {
                            ?_contact nco:hasAffiliation ?auto83 .
                            ?auto83 nco:hasPhoneNumber ?auto84 .
                            ?auto84 maemo:localPhoneNumber ?auto80 
                            FILTER(?auto80 = "98653")
                        }
                    )
                }
                """

                results1 = self.tracker.query (query1)
                print "1", results1
                self.assertEquals (len (results1), 1)
                self.assertEquals (len (results1[0]), 2)
                self.assertEquals (results1[0][0], "contact:test")
                self.assertEquals (results1[0][1], "98653")

                results2 = self.tracker.query (query2)
                print "2", results2
                self.assertEquals (len (results2), 1)
                self.assertEquals (len (results2[0]), 2)
                self.assertEquals (results2[0][0], "contact:test")
                self.assertEquals (results2[0][1], "98653")
                

                results3 = self.tracker.query (query3)
                print "3", results3
                self.assertEquals (len (results3), 1)
                self.assertEquals (len (results3[0]), 2)
                self.assertEquals (results3[0][0], "contact:test")
                self.assertEquals (results3[0][1], "98653")

                """ Clean the DB """
                delete = """
                DELETE { <contact:affiliation> a rdfs:Resource .
                <contact:test> a rdfs:Resource .
                }
                """ 
                

if __name__ == "__main__":
	ut.main()
