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
Stand-alone tests cases for the store, booting it with different ontology
changes and checking if the data is still there.
"""
import time

import os
from common.utils import configuration as cfg
import unittest2 as ut
#import unittest as ut
from common.utils.system import TrackerSystemAbstraction as TrackerSystemAbstraction
from common.utils.system import UnableToBootException as UnableToBootException
from common.utils.helpers import StoreHelper as StoreHelper
from common.utils.expectedFailure import expectedFailureBug


RDFS_RANGE = "http://www.w3.org/2000/01/rdf-schema#range"
XSD_DATETIME = "http://www.w3.org/2001/XMLSchema#dateTime"
XSD_STRING = "http://www.w3.org/2001/XMLSchema#string"
XSD_INTEGER = "http://www.w3.org/2001/XMLSchema#integer"

class OntologyChangeTestTemplate (ut.TestCase):

    def tearDown (self):
        print "*******"
        
    def get_ontology_dir (self, param):
        local = os.path.join (os.getcwd (), "test-ontologies", param)
        if (os.path.exists (local)):
            # Use local directory if available
            return local
        else:
            return os.path.join (cfg.DATADIR, "tracker-tests",
                                    "test-ontologies", param)

    def setUp (self):
        self.system = TrackerSystemAbstraction ()

    def tearDown (self):
        self.system.tracker_store_testing_stop ()

    def template_test_ontology_change (self):

        self.set_ontology_dirs ()
        
        basic_ontologies = self.get_ontology_dir (self.FIRST_ONTOLOGY_DIR)
        self.system.tracker_store_testing_start (ontodir=basic_ontologies)
        self.tracker = StoreHelper ()
        self.tracker.wait () #Safe guard. Returns when the store is ready

        self.insert_data ()

        try:
            # Boot the second set of ontologies
            modified_ontologies = self.get_ontology_dir (self.SECOND_ONTOLOGY_DIR)
            self.system.tracker_store_restart_with_new_ontologies (modified_ontologies)
        except UnableToBootException, e:
            self.fail (str(self.__class__) + " " + str(e))

        self.validate_status ()

    def set_ontology_dirs (self):
        """
        Implement this method in the subclass setting values for:
        self.FIRST_ONTOLOGY_DIR and
        self.SECOND_ONTOLOGY_DIR
        """
        raise Exception ("Subclasses must implement 'set_ontology_dir'")

    def insert_data (self):
        """
        Put in the store some data with the FIRST ontology
        """
        raise Exception ("Subclasses must implement 'insert_data'")

    def validate_status (self):
        """
        This is called after restarting the store with the SECOND ontology
        Check that the inserted data was handled correctly and the ontology
        is up to date
        """
        raise Exception ("Subclasses must implement 'validate_status'")


class PropertyRangeStringToDate (OntologyChangeTestTemplate):
    """
    Change the range of a property from string to date. There shouldn't be any data loss.
    """

    @expectedFailureBug ("New journal is gonna work it out")
    def test_property_range_string_to_date (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "property-range-string-to-date"

    def insert_data (self):
        self.instance = "test://ontology-change/property-range/string-to-date"
        self.tracker.update ("INSERT { <%s> a test:A ; test:a_string '2010-10-12T13:30:00Z' }"
                             % (self.instance))

    def validate_status (self):
        print "validating"
        # Query the ontology itself
        result = self.tracker.query ("SELECT ?o WHERE { test:a_string rdfs:range ?o }")
        self.assertEquals (result[0][0], XSD_DATETIME)

        # Check the value is there
        result = self.tracker.query ("SELECT ?o WHERE { <%s> test:a_string ?o . }" % (self.instance))
        self.assertEquals (result[0][0], "2010-10-12T13:30:00Z")


class PropertyRangeDateToString (OntologyChangeTestTemplate):
    """
    Change the range of a property from date to string. There shouldn't be any data loss.
    """

    @expectedFailureBug ("New journal is gonna work it out")
    def test_property_range_date_to_string (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "property-range-string-to-date"
        self.SECOND_ONTOLOGY_DIR = "basic"
        
    def insert_data (self):
        self.instance = "test://ontology-change/property-range/date-to-string"
        self.tracker.update ("INSERT { <%s> a test:classA ; test:propertyString '2010-10-12T13:30:00Z' }"
                             % (self.instance))

    def validate_status (self):
        # Query the ontology itself
        result = self.tracker.query ("SELECT ?o WHERE { test:propertyString rdfs:range ?o }")
        self.assertEquals (result[0][0], XSD_STRING)

        # Check the value is there
        result = self.tracker.query ("SELECT ?o WHERE { <%s> test:propertyString ?o . }" % (self.instance))
        self.assertEquals (result[0][0], "2010-10-12T13:30:00Z")

class PropertyRangeIntToString (OntologyChangeTestTemplate):
    """
    Change the range of a property from int to string. There shouldn't be any data loss.
    """
    def test_property_range_int_to_str (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "property-range-int-to-string"

    def insert_data (self):
        self.instance = "test://ontology-change/property-range/int-to-string"
        self.tracker.update ("INSERT { <%s> a test:A; test:a_int 12. }" % (self.instance))

    def validate_status (self):
        result = self.tracker.query ("SELECT ?o WHERE { test:a_int rdfs:range ?o. }")
        self.assertEquals (str(result[0][0]), XSD_STRING)

        # Check the value is there
        result = self.tracker.query ("SELECT ?o WHERE { <%s> test:a_int ?o .}" % (self.instance))
        self.assertEquals (result[0][0], "12")

class PropertyRangeStringToInt (OntologyChangeTestTemplate):
    """
    Change the range of a property from string to int. There shouldn't be any data loss.
    """

    def test_property_range_str_to_int (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "property-range-int-to-string"
        self.SECOND_ONTOLOGY_DIR = "basic"

    def insert_data (self):
        self.instance = "test://ontology-change/property-range/string-to-int"
        self.tracker.update ("INSERT { <%s> a test:A; test:a_int '12'. }" % (self.instance))

    def validate_status (self):
        result = self.tracker.query ("SELECT ?o WHERE { test:a_int rdfs:range ?o. }")
        self.assertEquals (str(result[0][0]), XSD_INTEGER)

        # Check the value is there
        result = self.tracker.query ("SELECT ?o WHERE { <%s> test:a_int ?o .}" % (self.instance))
        self.assertEquals (result[0][0], "12")
        
class PropertyMaxCardinality1toN (OntologyChangeTestTemplate):
    """
    Change cardinality of a property from 1 to N. There shouldn't be any data loss
    """

    @expectedFailureBug ("New journal is gonna work it out")
    def test_property_cardinality_1_to_n (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        #self.FIRST_ONTOLOGY_DIR = "basic"
        #self.SECOND_ONTOLOGY_DIR = "cardinality"

        self.FIRST_ONTOLOGY_DIR = "cardinality"
        self.SECOND_ONTOLOGY_DIR = "basic"

    def insert_data (self):
        self.instance = "test://ontology-change/cardinality/1-to-n"
        self.tracker.update ("INSERT { <%s> a test:A; test:a_n_cardinality 'some text'. }" % (self.instance))

        result = self.tracker.query ("SELECT ?o WHERE { test:a_n_cardinality nrl:maxCardinality ?o}")
        self.assertEquals (int (result[0][0]), 1)

                
    def validate_status (self):
        result = self.tracker.query ("SELECT ?o WHERE { test:a_n_cardinality nrl:maxCardinality ?o}")
        self.assertEquals (len (result), 0, "Cardinality should be 0")
        
        # Check the value is there
        result = self.tracker.query ("SELECT ?o WHERE { <%s> test:a_n_cardinality ?o .}" % (self.instance))
        self.assertEquals (str(result[0][0]), "some text")

class PropertyMaxCardinalityNto1 (OntologyChangeTestTemplate):
    """
    Change the cardinality of a property for N to 1.
    """

    @expectedFailureBug ("New journal is gonna work it out")
    def test_property_cardinality_n_to_1 (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "cardinality"

    def insert_data (self):
        self.instance = "test://ontology-change/cardinality/1-to-n"
        self.tracker.update ("INSERT { <%s> a test:A; test:a_n_cardinality 'some text'. }" % (self.instance))

        result = self.tracker.query ("SELECT ?o WHERE { test:a_n_cardinality nrl:maxCardinality ?o}")
        self.assertEquals (len (result), 0, "Cardinality should be 0")

                
    def validate_status (self):
        result = self.tracker.query ("SELECT ?o WHERE { test:a_n_cardinality nrl:maxCardinality ?o}")
        self.assertEquals (int (result[0][0]), 1, "Cardinality should be 1")
        
        # Check the value is there
        result = self.tracker.query ("SELECT ?o WHERE { <%s> test:a_n_cardinality ?o .}" % (self.instance))
        self.assertEquals (str(result[0][0]), "some text")

class PropertyNotifySet (OntologyChangeTestTemplate):
    """
    Set tracker:notify to true in a class and check there is no data loss
    """
    def test_property_notify_set (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "notify"

    def insert_data (self):
        self.instance = "test://ontology-change/notify/true"
        self.tracker.update ("INSERT { <%s> a test:A; test:a_string 'some text'. }" % (self.instance))


    def validate_status (self):
        result = self.tracker.query ("SELECT ?notify WHERE { test:A tracker:notify ?notify}")
        self.assertEquals (str(result[0][0]), "true")
        
        result = self.tracker.query ("SELECT ?u WHERE { ?u a test:A. }")
        self.assertEquals (str(result[0][0]), self.instance)

class PropertyNotifyUnset (OntologyChangeTestTemplate):
    """
    Set tracker:notify to true in a class and check there is no data loss
    """
    def test_property_notify_set (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "notify"
        self.SECOND_ONTOLOGY_DIR = "basic"

    def insert_data (self):
        self.instance = "test://ontology-change/notify/true"
        self.tracker.update ("INSERT { <%s> a test:A; test:a_string 'some text'. }" % (self.instance))


    def validate_status (self):
        result = self.tracker.query ("SELECT ?notify WHERE { test:A tracker:notify ?notify}")
        if (len (result) == 1):
            # Usually is (none) but it was "true" before so now has value.
            self.assertEquals (result[0][0], "false")
        else:
            self.assertEquals (len (result), 0)
        
        result = self.tracker.query ("SELECT ?u WHERE { ?u a test:A. }")
        self.assertEquals (str(result[0][0]), self.instance)
                
    

if __name__ == "__main__":
    ut.main ()

    
