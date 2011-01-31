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
from common.utils.helpers import StoreHelper
import unittest2 as ut
from common.utils.expectedFailure import expectedFailureBug


class WritebackKeepDateTest (CommonTrackerWritebackTest):

    def setUp (self):
        self.tracker = StoreHelper ()


    def __prepare_favorite_tag (self):
        # Check here if favorite has tag... to make sure writeback is actually writing
        pass


    def test_01_NB217627_content_created_date (self):
        """
        NB#217627 - Order if results is different when an image is marked as favorite.
        """
        query_images = """
          SELECT ?u nie:url(?u) WHERE {
              ?u a nfo:Visual ;
                 nie:contentCreated ?contentCreated .
          } ORDER BY ?contentCreated
          """
        results = self.tracker.query (query_images)
        results_unpacked = [ r[1] for r in results ]
        print results_unpacked
        self.assertEquals ( len (results), 3, results_unpacked)

        # This triggers the writeback
        mark_as_favorite = """
         INSERT {
          <%s> nao:hasTag <http://www.semanticdesktop.org/ontologies/2007/08/15/nao#predefined-tag-favorite>
         }
        """

        # Now check the modification date of the files and it should be the same :)
        pass

        

if __name__ == "__main__":
    ut.main ()
