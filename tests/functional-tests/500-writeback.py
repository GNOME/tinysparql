#!/usr/bin/python

# Copyright (C) 2010, Nokia (ivan.frade@nokia.com)
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
"""
Write values in tracker and check the actual values are written
on the files. Note that these tests are highly platform dependant.
"""
import os, dbus
import time

from common.utils.helpers import StoreHelper, ExtractorHelper
from common.utils.writebacktest import CommonTrackerWritebackTest as CommonTrackerWritebackTest
from common.utils.writebacktest import uri as uri
import unittest2 as ut
from common.utils.expectedFailure import expectedFailureBug

REASONABLE_TIMEOUT = 5 # Seconds we wait for tracker-writeback to do the work

class WritebackBasicDataTest (CommonTrackerWritebackTest):
    """
    Write in tracker store the properties witih writeback support and check
    that the new values are actually in the file
    """
    def setUp (self):
        self.tracker = StoreHelper ()
        self.extractor = ExtractorHelper ()

    def tearDown (self):
        # Give it more time between tests to avoid random failures?
        pass

    def __clean_property (self, property_name, fileuri, expectFailure=True):
        """
        Remove the property for the fileuri (file://...)
        """
        CLEAN = """
           DELETE { ?u %s ?whatever }
           WHERE  {
               ?u nie:url '%s' ;
                  %s ?whatever .
           
           }
        """
        try:
            self.tracker.update (CLEAN % (property_name, fileuri, property_name))
        except Exception, e:
            print e
            assert expectFailure
                                

    def __writeback_test (self, filename, mimetype, prop, expectedKey=None):
        """
        Set a value in @prop for the @filename. Then ask tracker-extractor
        for metadata and check in the results dictionary if the property is there.

        Note: given the special translation of some property-names in the dictionary
        with extracted metadata, there is an optional parameter @expectedKey
        to specify what property to check in the dictionary. If None, then
        the @prop is used.
        """

        TEST_VALUE = prop.replace (":","") + "test"
        SPARQL_TMPL = """
           INSERT { ?u %s '%s' }
           WHERE  { ?u nie:url '%s' }
        """ 
        self.__clean_property (prop, uri(filename))
        self.tracker.update (SPARQL_TMPL % (prop, TEST_VALUE, uri(filename)))
        
        # There is no way to know when the operation is finished
        time.sleep (REASONABLE_TIMEOUT)
        
        results = self.extractor.get_metadata (uri (filename), mimetype)
        keyDict = expectedKey or prop
        self.assertIn (TEST_VALUE, results[keyDict])
        self.__clean_property (prop, uri(filename), False)


    def __writeback_hasTag_test (self, filename, mimetype):

        SPARQL_TMPL = """
            INSERT {
              <test://writeback-hasTag-test/1> a nao:Tag ;
                        nao:prefLabel "testTag" .

              ?u nao:hasTag <test://writeback-hasTag-test/1> .
            } WHERE {
              ?u nie:url '%s' .
            }
        """

        CLEAN_VALUE = """
           DELETE {
              <test://writeback-hasTag-test/1> a rdfs:Resource.
              ?u nao:hasTag <test://writeback-hasTag-test/1> .
           } WHERE {
              ?u nao:hasTag <test://writeback-hasTag-test/1> .
           }
        """

        self.tracker.update (SPARQL_TMPL % (uri (filename)))

        time.sleep (REASONABLE_TIMEOUT)

        results = self.extractor.get_metadata (uri (filename), mimetype)
        self.assertIn ("testTag", results ["nao:hasTag:prefLabel"])


    # JPEG test
    def test_001_jpeg_title (self):
        FILENAME = "test-writeback-monitored/writeback-test-1.jpeg"
        self.__writeback_test (FILENAME, "image/jpeg", "nie:title")

    def test_002_jpeg_description (self):
        FILENAME = "test-writeback-monitored/writeback-test-1.jpeg"
        self.__writeback_test (FILENAME, "image/jpeg", "nie:description")

    def test_003_jpeg_keyword (self):
        FILENAME = "test-writeback-monitored/writeback-test-1.jpeg"
        self.__writeback_test (FILENAME, "image/jpeg", "nie:keyword", "nao:hasTag:prefLabel")

    def test_004_jpeg_hasTag (self):
        FILENAME = "test-writeback-monitored/writeback-test-1.jpeg"
        self.__writeback_hasTag_test (FILENAME, "image/jpeg")

        
    # TIFF tests
    def test_011_tiff_title (self):
        FILENAME = "test-writeback-monitored/writeback-test-2.tif"
        self.__writeback_test (FILENAME, "image/tiff", "nie:title")

    def test_012_tiff_description (self):
        FILENAME = "test-writeback-monitored/writeback-test-2.tif"
        self.__writeback_test (FILENAME, "image/tiff", "nie:description")
        
    def test_013_tiff_keyword (self):
        FILENAME = "test-writeback-monitored/writeback-test-2.tif"
        self.__writeback_test (FILENAME, "image/tiff", "nie:keyword", "nao:hasTag:prefLabel")

    def test_014_tiff_hasTag (self):
        FILENAME = "test-writeback-monitored/writeback-test-2.tif"
        self.__writeback_hasTag_test (FILENAME, "image/tiff")
      
        

    # PNG tests
    @expectedFailureBug ("NB#185070")
    def test_021_png_title (self):
        FILENAME = "test-writeback-monitored/writeback-test-4.png"
        self.__writeback_test (FILENAME, "image/png", "nie:title")

    @expectedFailureBug ("NB#185070")
    def test_022_png_description (self):
        FILENAME = "test-writeback-monitored/writeback-test-4.png"
        self.__writeback_test (FILENAME, "image/png", "nie:description")
        
    @expectedFailureBug ("NB#185070")
    def test_023_png_keyword (self):
        FILENAME = "test-writeback-monitored/writeback-test-4.png"
        self.__writeback_test (FILENAME, "image/png", "nie:keyword", "nao:hasTag:prefLabel")

    @expectedFailureBug("NB#185070")
    def test_024_png_hasTag (self):
        FILENAME = "test-writeback-monitored/writeback-test-4.png"
        self.__writeback_hasTag_test (FILENAME, "image/png")

if __name__ == "__main__":
    ut.main ()
