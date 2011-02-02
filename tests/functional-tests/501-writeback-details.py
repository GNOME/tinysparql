#!/usr/bin/python

# Copyright (C) 2011, Nokia (ivan.frade@nokia.com)
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
from common.utils.writebacktest import CommonTrackerWritebackTest as CommonTrackerWritebackTest
from common.utils.helpers import StoreHelper, ExtractorHelper
import unittest2 as ut
from common.utils.expectedFailure import expectedFailureBug
import time

REASONABLE_TIMEOUT = 5 # Seconds we wait for tracker-writeback to do the work

class WritebackKeepDateTest (CommonTrackerWritebackTest):

    def setUp (self):
        self.tracker = StoreHelper ()
        self.extractor = ExtractorHelper ()
        self.favorite = self.__prepare_favorite_tag ()

    def __prepare_favorite_tag (self):
        # Check here if favorite has tag... to make sure writeback is actually writing
        results = self.tracker.query ("""
             SELECT ?label WHERE { nao:predefined-tag-favorite nao:prefLabel ?label }""")

        if len (results) == 0:
            self.tracker.update ("""
             INSERT { nao:predefined-tag-favorite nao:prefLabel 'favorite'}
             WHERE { nao:predefined-tag-favorite a nao:Tag }
             """)
            return "favorite"
        else:
            return str(results[0][0])
                       

    def test_01_NB217627_content_created_date (self):
        """
        NB#217627 - Order if results is different when an image is marked as favorite.
        """
        query_images = """
          SELECT nie:url (?u) ?contentCreated WHERE {
              ?u a nfo:Visual ;
                 nie:contentCreated ?contentCreated .
          } ORDER BY ?contentCreated
          """
        results = self.tracker.query (query_images)
        self.assertEquals (len (results), 3, results)

        print "Waiting 2 seconds to ensure there is a noticiable difference in the timestamp"
        time.sleep (2)
    
        # This triggers the writeback
        mark_as_favorite = """
         INSERT {
           ?u nao:hasTag nao:predefined-tag-favorite .
         } WHERE {
           ?u nie:url <%s> .
         }
        """ % (self.get_test_filename_jpeg ())
        self.tracker.update (mark_as_favorite)
        print "Setting favorite in <%s>" % (self.get_test_filename_jpeg ())
        time.sleep (REASONABLE_TIMEOUT)

        # Check the value is written in the file
        metadata = self.extractor.get_metadata (self.get_test_filename_jpeg (), "")
        self.assertIn (self.favorite, metadata ["nao:hasTag:prefLabel"],
                       "Tag hasn't been written in the file")
        
        # Now check the modification date of the files and it should be the same :)
        new_results = self.tracker.query (query_images)
        ## for (uri, date) in new_results:
        ##     print "Checking dates of <%s>" % uri
        ##     previous_date = convenience_dict[uri]
        ##     print "Before: %s \nAfter : %s" % (previous_date, date)
        ##     self.assertEquals (date, previous_date, "File <%s> has change its contentCreated date!" % uri)

        # Indeed the order of the results should be the same
        for i in range (0, len (results)):
            self.assertEquals (results[i][0], new_results[i][0], "Order of the files is different")
            self.assertEquals (results[i][1], new_results[i][1], "Date has change in file <%s>" % results[i][0])
        

if __name__ == "__main__":
    ut.main ()
