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
For a collection of files, call the extractor and check that the expected
metadata is extracted. Load dynamically the test information from a data
directory (containing xxx.expected files)
"""

from common.utils import configuration as cfg
from common.utils.extractor import get_tracker_extract_output
import unittest2 as ut
import os
import sys

import ConfigParser


class ExtractionTestCase (ut.TestCase):
    """
    Test checks if the tracker extractor is able to retrieve metadata
    """
    def __init__ (self, methodName='runTest', descfile=None):
        """
        Descfile is the description file in a relative path
        """
        ut.TestCase.__init__ (self, methodName)

        # Load the description file
        assert descfile
        self.rel_description = descfile
        self.configParser = self.__load_description_file (self.rel_description)

        # Add a method to the class called after the description file
        methodName = self.rel_description.lower()[:-len(".expected")].replace (" ", "_")[-60:]

        if (self.__is_expected_failure ()):
            setattr (self,
                     methodName,
                     self.expected_failure_test_extraction)
        else:
            setattr (self,
                     methodName,
                     self.generic_test_extraction)

        # unittest framework will run the test called "self._testMethodName"
        # So we set that variable to our new name
        self._testMethodName = methodName

    def runTest (self):
        """
        Empty function pointer, that should NEVER be called. It is required to exist by unittest.
        """
        assert False

    def __load_description_file (self, descfile):
        configParser = ConfigParser.RawConfigParser ()
        # Make it case sensitive:
        configParser.optionxform = str

        abs_description = os.path.abspath (descfile)
        loaded_files = configParser.read (abs_description)
        if not abs_description in loaded_files:
            raise Exception("Unable to load %s" % (abs_description))

        return configParser

    def __is_expected_failure (self):
        assert self.configParser
        return self.configParser.has_option ("TestFile", "ExpectedFailure")

    def __get_bugnumber (self):
        assert self.configParser
        if self.configParser.has_option ("TestFile", "Bugzilla"):
            return "'" + self.configParser.get ("TestFile", "Bugzilla") + "'"
        else:
            return None

    def expected_failure_test_extraction (self):
        try:
            self.generic_test_extraction ()
        except Exception:
            raise ut.case._ExpectedFailure(sys.exc_info())

        if self.__get_bugnumber ():
            raise Exception ("Unexpected success. Maybe bug: " + self.__get_bugnumber () + " has been fixed?")
        else:
            raise Exception ("Unexpected success. Check " + self.rel_description)

    def generic_test_extraction (self):
        abs_description = os.path.abspath (self.rel_description)

        # Filename contains the file to extract, in a relative path to the description file
        desc_root, desc_file = os.path.split (abs_description)

        filename_to_extract = self.configParser.get ("TestFile", "Filename")
        self.file_to_extract = os.path.join (desc_root, filename_to_extract)

        result = get_tracker_extract_output(self.file_to_extract)
        self.__assert_extraction_ok (result)

    def assertDictHasKey (self, d, key, msg=None):
        if not d.has_key (key):
            standardMsg = "Missing: %s\n" % (key)
            self.fail (self._formatMessage (msg, standardMsg))
        else:
            return

    def assertIsURN (self, supposed_uuid, msg=None):
        import uuid

        try:
            if (supposed_uuid.startswith ("<") and supposed_uuid.endswith (">")):
                supposed_uuid = supposed_uuid[1:-1]

            uuid.UUID (supposed_uuid)
        except ValueError:
            standardMsg = "'%s' is not a valid UUID" % (supposed_uuid)
            self.fail (self._formatMessage (msg, standardMsg))

    def __assert_extraction_ok (self, result):
        self.__check_section ("Metadata", result)

        if (cfg.haveMaemo and self.configParser.has_section ("Meego")):
            self.__check_section ("Meego", result)


    def __check_section (self, section, result):
        error_missing_prop = "Property '%s' hasn't been extracted from file \n'%s'\n (requested on '%s' [%s])"
        error_wrong_value = "on property '%s' from file %s\n (requested on: '%s' [%s])"
        error_extra_prop = "Property '%s' was explicitely banned for file \n'%s'\n (requested on '%s' [%s])"
        error_extra_prop_v = "Property '%s' with value '%s' was explicitely banned for file \n'%s'\n (requested on %s' [%s])"

        expected_pairs = [] # List of expected (key, value)
        unexpected_pairs = []  # List of unexpected (key, value)
        expected_keys = []  # List of expected keys (the key must be there, value doesnt matter)

        for k, v in self.configParser.items (section):
            if k.startswith ("!"):
                unexpected_pairs.append ( (k[1:].replace ("_", ":"), v) )
            elif k.startswith ("@"):
                expected_keys.append ( k[1:].replace ("_", ":") )
            else:
                expected_pairs.append ( (k.replace ("_", ":"), v) )


        for (prop, value) in expected_pairs:
            self.assertDictHasKey (result, prop,
                                   error_missing_prop % (prop,
                                                         self.file_to_extract,
                                                         self.rel_description,
                                                         section))
            if value == "@URNUUID@":
                # Watch out! We take only the FIRST element. Incompatible with multiple-valued props.
                self.assertIsURN (result [prop][0],
                                  error_wrong_value % (prop,
                                                       self.file_to_extract,
                                                       self.rel_description,
                                                       section))
            else:
                self.assertIn (value, result [prop],
                               error_wrong_value % (prop,
                                                    self.file_to_extract,
                                                    self.rel_description,
                                                    section))

        for (prop, value) in unexpected_pairs:
            # There is no prop, or it is but not with that value
            if (value == ""):
                self.assertFalse (result.has_key (prop), error_extra_prop % (prop,
                                                                             self.file_to_extract,
                                                                             self.rel_description,
                                                                             section))
            else:
                if (value == "@URNUUID@"):
                    self.assertIsURN (result [prop][0], error_extra_prop % (prop,
                                                                            self.file_to_extract,
                                                                            self.rel_description,
                                                                            section))
                else:
                    self.assertNotIn (value, result [prop], error_extra_prop_v % (prop,
                                                                                  value,
                                                                                  self.file_to_extract,
                                                                                  self.rel_description,
                                                                                  section))

        for prop in expected_keys:
             self.assertDictHasKey (result, prop,
                                    error_missing_prop % (prop,
                                                          self.file_to_extract,
                                                          self.rel_description,
                                                          section))


def run_all ():
    ##
    # Traverse the TEST_DATA_PATH directory looking for .description files
    # Add a new TestCase to the suite per .description file and run the suite.
    #
    # Is we do this inside a single TestCase an error in one test would stop the whole
    # testing.
    ##
    if (os.path.exists (os.getcwd() + "/test-extraction-data")):
        # Use local directory if available
        TEST_DATA_PATH = os.getcwd() + "/test-extraction-data"
    else:
        TEST_DATA_PATH = os.path.join (cfg.DATADIR, "tracker-tests",
                                       "test-extraction-data")
    print "Loading test descriptions from", TEST_DATA_PATH
    extractionTestSuite = ut.TestSuite ()
    for root, dirs, files in os.walk (TEST_DATA_PATH):
         descriptions = [os.path.join (root, f) for f in files if f.endswith ("expected")]
         for descfile in descriptions:
             tc = ExtractionTestCase(descfile=descfile)
             extractionTestSuite.addTest(tc)
    result = ut.TextTestRunner (verbosity=1).run (extractionTestSuite)
    sys.exit(not result.wasSuccessful())

def run_one (filename):
    ##
    # Run just one .description file
    ##
    description = os.path.join (os.getcwd (), filename) 

    extractionTestSuite = ut.TestSuite ()
    tc = ExtractionTestCase(descfile=description)
    extractionTestSuite.addTest(tc)

    result = ut.TextTestRunner (verbosity=2).run (extractionTestSuite)
    sys.exit(not result.wasSuccessful())


if __name__ == "__main__":
    if (len (sys.argv) == 1):
        run_all ()
    else:
        if os.path.exists (sys.argv[1]) and sys.argv[1].endswith (".expected"):
            run_one (sys.argv[1])
        # FIXME: for the case when invoked by testrunner (see create-tests-xml.py)
        elif sys.argv[1] == "ExtractionTestCase":
            run_all ()
        else:
            print "Usage: %s [FILE.expected]" % (sys.argv[0])
        
