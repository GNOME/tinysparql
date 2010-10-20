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

TEST_PREFIX = "http://example.org/ns#"

import re
import time

class OntologyChangeTestTemplate (ut.TestCase):
    """
    Template class for the ontology changes tests. The tests are subclasses
    of this, implementing these methods:
    
       * set_ontology_dirs
       * insert_data
       * validate_status
       
    and adding a method 'test_x_y_z' to be invoked by unittest.
   
    Check doc in those methods for the specific details.
    """
        
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
        modified_ontologies = self.get_ontology_dir (self.SECOND_ONTOLOGY_DIR)

        self.__assert_ontology_dates (basic_ontologies, modified_ontologies)


        self.system.tracker_store_testing_start (ontodir=basic_ontologies)
        self.tracker = StoreHelper ()
        self.tracker.wait () #Safe guard. Returns when the store is ready

        self.insert_data ()

        try:
            # Boot the second set of ontologies
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


    def assertInDbusResult (self, member, dbus_result, column=0):
        """
        Convenience assertion used in these tests
        """
        for row in dbus_result:
            if member == row[column]:
                return
        # This is going to fail with pretty printing
        self.assertIn (member, dbus_result) 

    def assertNotInDbusResult (self, member, dbus_result, column=0):
        """
        Convenience assertion used in these tests
        """
        for row in dbus_result:
            if member == str(row[column]):
                # This is going to fail with pretty printing
                self.fail ("'%s' wasn't supposed to be in '%s'" % (member, dbus_result))
        return

    def __assert_ontology_dates (self, first_dir, second_dir):
        """
        Asserts that 91-test.ontology in second_dir has a more recent
        modification time than in first_dir
        """
        ISO9601_REGEX = "(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)"

        def get_ontology_date (ontology):
            for line in open (ontology, 'r'):
                if "nao:lastModified" in line:
                    getmodtime = re.compile ('nao:lastModified\ \"' + ISO9601_REGEX + '\"')
                    modtime_match = getmodtime.search (line)

                    if (modtime_match):
                        nao_date = modtime_match.group (1)
                        return time.strptime(nao_date, "%Y-%m-%dT%H:%M:%SZ")  
                    else:
                        print "something funky in", line
                    break

        first_date = get_ontology_date (os.path.join (first_dir, "91-test.ontology"))
        second_date = get_ontology_date (os.path.join (second_dir, "91-test.ontology"))
        if first_date >= second_date:
            self.fail ("nao:modifiedTime in '%s' is not more recent in the second ontology" % ("91-test.ontology"))
        

        

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
        self.SECOND_ONTOLOGY_DIR = "basic-future"
        
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
        self.SECOND_ONTOLOGY_DIR = "basic-future"

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
        self.SECOND_ONTOLOGY_DIR = "basic-future"

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

class ClassNotifySet (OntologyChangeTestTemplate):
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

class ClassNotifyUnset (OntologyChangeTestTemplate):
    """
    Set tracker:notify to true in a class and check there is no data loss
    """
    def test_property_notify_set (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "notify"
        self.SECOND_ONTOLOGY_DIR = "basic-future"

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


class PropertyIndexedSet (OntologyChangeTestTemplate):
    """
    Set tracker:indexed true to single and multiple valued properties.
    Check that instances and content of the property are still in the DB
    """
    def test_indexed_set (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "indexed"

    def insert_data (self):
        # Instance with value in the single valued property
        self.instance_single_valued = "test://ontology-change/indexed/single/true"
        self.tracker.update ("INSERT { <%s> a test:A ; test:a_string 'anything 1'. }"
                             % (self.instance_single_valued))

        # Instance with value in the n valued property
        self.instance_n_valued = "test://ontology-change/indexed/multiple/true"
        self.tracker.update ("INSERT { <%s> a test:A ; test:a_n_cardinality 'anything n'. }"
                             % (self.instance_n_valued))

    def validate_status (self):
        # Check ontology and instance for the single valued property
        result = self.tracker.query ("SELECT ?indexed WHERE { test:a_string tracker:indexed ?indexed}")
        self.assertEquals (str(result[0][0]), "true")

        result = self.tracker.query ("SELECT ?content WHERE { <%s> a test:A; test:a_string ?content. }"
                                     % (self.instance_single_valued))
        self.assertEquals (str(result[0][0]), "anything 1")

        # Check ontology and instance for the multiple valued property
        result = self.tracker.query ("SELECT ?indexed WHERE { test:a_n_cardinality tracker:indexed ?indexed}")
        self.assertEquals (str(result[0][0]), "true")

        result = self.tracker.query ("SELECT ?content WHERE { <%s> a test:A; test:a_n_cardinality ?content. }"
                                     % (self.instance_n_valued))
        self.assertEquals (str(result[0][0]), "anything n")

class PropertyIndexedUnset (OntologyChangeTestTemplate):
    """
    tracker:indexed property from true to false in single and multiple valued properties.
    Check that instances and content of the property are still in the DB.
    """
    def test_indexed_unset (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "indexed"
        self.SECOND_ONTOLOGY_DIR = "basic-future"

    def insert_data (self):
        # Instance with value in the single valued property
        self.instance_single_valued = "test://ontology-change/indexed/single/true"
        self.tracker.update ("INSERT { <%s> a test:A ; test:a_string 'anything 1'. }"
                             % (self.instance_single_valued))

        # Instance with value in the n valued property
        self.instance_n_valued = "test://ontology-change/indexed/multiple/true"
        self.tracker.update ("INSERT { <%s> a test:A ; test:a_n_cardinality 'anything n'. }"
                             % (self.instance_n_valued))

    def validate_status (self):
        #
        # NOTE: tracker:indexed can be 'false' or None. In both cases is fine.
        # 
        
        # Check ontology and instance for the single valued property
        result = self.tracker.query ("SELECT ?indexed WHERE { test:a_string tracker:indexed ?indexed}")
        self.assertEquals (str(result[0][0]), "false")

        result = self.tracker.query ("SELECT ?content WHERE { <%s> a test:A; test:a_string ?content. }"
                                     % (self.instance_single_valued))
        self.assertEquals (str(result[0][0]), "anything 1")

        # Check ontology and instance for the multiple valued property
        result = self.tracker.query ("SELECT ?indexed WHERE { test:a_n_cardinality tracker:indexed ?indexed}")
        self.assertEquals (str(result[0][0]), "false")

        result = self.tracker.query ("SELECT ?content WHERE { <%s> a test:A; test:a_n_cardinality ?content. }"
                                     % (self.instance_n_valued))
        self.assertEquals (str(result[0][0]), "anything n")

class OntologyAddClassTest (OntologyChangeTestTemplate):
    """
    Add a class in the ontology.
    """
    def test_ontology_add_class (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "add-class"

    def insert_data (self):
        # No need, adding a class
        pass

    def validate_status (self):
        # check the class is there
        result = self.tracker.query ("SELECT ?k WHERE { ?k a rdfs:Class. }")
        self.assertInDbusResult (TEST_PREFIX + "D", result)

        result = self.tracker.query ("SELECT ?k WHERE { ?k a rdfs:Class. }")
        self.assertInDbusResult (TEST_PREFIX + "E", result)


class OntologyRemoveClassTest (OntologyChangeTestTemplate):
    """
    Remove a class from the ontology. With and without superclasses.
    """
    def test_ontology_remove_class (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "add-class"
        self.SECOND_ONTOLOGY_DIR = "basic-future"

    def insert_data (self):
        self.instance_e = "test://ontology-change/removal/class/1"
        self.tracker.update ("INSERT { <%s> a test:E. }" % self.instance_e)

        self.instance_d = "test://ontology-change/removal/class/2"
        self.tracker.update ("INSERT { <%s> a test:D. }" % self.instance_d)

    def validate_status (self):
        #
        # The classes are not actually removed... so this assertions are not valid (yet?)
        #
        
        #result = self.tracker.query ("SELECT ?k WHERE { ?k a rdfs:Class. }")
        #self.assertNotInDbusResult (TEST_PREFIX + "E", result)
        #self.assertNotInDbusResult (TEST_PREFIX + "D", result)

        # D is a subclass of A, removing D should keep the A instances
        result = self.tracker.query ("SELECT ?i WHERE { ?i a test:A. }")
        self.assertEquals (result[0][0], self.instance_e)

class OntologyAddPropertyTest (OntologyChangeTestTemplate):
    """
    Add new properties in the ontology, with/without super prop and different ranges and cardinalities
    """
    def test_ontology_add_property (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "add-prop"

    def insert_data (self):
        # No need, adding new properties
        pass

    def validate_status (self):
        result = self.tracker.query ("SELECT ?k WHERE { ?k a rdf:Property}")
        self.assertInDbusResult (TEST_PREFIX + "new_prop_int", result)
        self.assertInDbusResult (TEST_PREFIX + "new_prop_int_n", result)

        self.assertInDbusResult (TEST_PREFIX + "new_prop_string", result)
        self.assertInDbusResult (TEST_PREFIX + "new_prop_string_n", result)

        self.assertInDbusResult (TEST_PREFIX + "new_subprop_string", result)
        self.assertInDbusResult (TEST_PREFIX + "new_subprop_string_n", result)

class OntologyRemovePropertyTest (OntologyChangeTestTemplate):
    """
    Remove properties from the ontology, with and without super props and different ranges and cardinalities
    """
    def test_ontology_remove_property (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "add-prop"
        self.SECOND_ONTOLOGY_DIR = "basic-future"

    def insert_data (self):
        self.instance_a = "test://ontology-change/remove/properties/1"
        self.tracker.update ("""
            INSERT { <%s> a   test:A;
                          test:a_string 'This is fine' ;
                          test:new_prop_int 7;
                          test:new_prop_int_n 3;
                          test:new_prop_string 'this is going to disappear' ;
                          test:new_prop_string_n 'same with this' .
                   }
           """ % (self.instance_a))

        self.instance_b = "test://ontology-change/remove/properties/2"
        self.tracker.update ("""
            INSERT { <%s> a   test:B;
                          test:new_subprop_string 'super-prop keeps this value';
                          test:new_subprop_string_n 'super-prop also keeps this value'.
                   }
        """ % (self.instance_b))
        self.assertTrue (self.tracker.ask ("ASK { <%s> a test:A}" % (self.instance_a)), "The instance is not there")

    def validate_status (self):
        #
        # Note: on removal basically nothing happens. The property and values are still in the DB
        #
        # Maybe we should test there forcing a db reconstruction and journal replay
        
        # First the ontology
        ## result = self.tracker.query ("SELECT ?k WHERE { ?k a rdf:Property}")
        ## self.assertNotInDbusResult (TEST_PREFIX + "new_prop_int", result)
        ## self.assertNotInDbusResult (TEST_PREFIX + "new_prop_int_n", result)

        ## self.assertNotInDbusResult (TEST_PREFIX + "new_prop_string", result)
        ## self.assertNotInDbusResult (TEST_PREFIX + "new_prop_string_n", result)

        ## self.assertNotInDbusResult (TEST_PREFIX + "new_subprop_string", result)
        ## self.assertNotInDbusResult (TEST_PREFIX + "new_subprop_string_n", result)

        # The instances are still there
        self.assertTrue (self.tracker.ask ("ASK { <%s> a test:A}" % (self.instance_a)))
        self.assertTrue (self.tracker.ask ("ASK { <%s> a test:B}" % (self.instance_b)))

        check = self.tracker.ask ("ASK { <%s> test:a_superprop 'super-prop keeps this value' }" % (self.instance_b))
        self.assertTrue (check, "This property and value should exist")
        
        check = self.tracker.ask ("ASK { <%s> test:a_superprop_n 'super-prop also keeps this value' }" % (self.instance_b))
        self.assertTrue (check, "This property and value should exist")

class DomainIndexAddTest (OntologyChangeTestTemplate):
    """
    Add tracker:domainIndex in properties
    """
    def test_DomainIndexAddTest (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "add-domainIndex"

    def insert_data (self):
        self.instance_a = "test://ontology-changes/properties/add-domain-index/a"
        self.tracker.update ("""
            INSERT { <%s> a test:B ;
                          test:a_string 'test-value' ;
                          test:a_n_cardinality 'another-test-value'. }""" % (self.instance_a))

        self.instance_b = "test://ontology-changes/properties/add-domain-index/b"
        self.tracker.update ("""
            INSERT { <%s> a test:C ;
                          test:a_string 'test-value' ;
                          test:a_n_cardinality 'another-test-value'. }""" % (self.instance_b))

    def validate_status (self):
        # Check the ontology
        has_domainIndex = self.tracker.ask ("ASK { test:B tracker:domainIndex test:a_string }")
        self.assertTrue (has_domainIndex)

        has_domainIndex = self.tracker.ask ("ASK { test:C tracker:domainIndex test:a_n_cardinality }")
        self.assertTrue (has_domainIndex)

        # Check the data
        dataok = self.tracker.ask ("ASK { <%s> test:a_string 'test-value' }" % (self.instance_a))
        self.assertTrue (dataok)

        dataok = self.tracker.ask ("ASK { <%s> test:a_n_cardinality 'another-test-value' }" % (self.instance_b))
        self.assertTrue (dataok)

class DomainIndexAddTest (OntologyChangeTestTemplate):
    """
    Add tracker:domainIndex to a class and check there is no data loss.
    """
    def test_domain_index_add (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "basic"
        self.SECOND_ONTOLOGY_DIR = "add-domainIndex"

    def insert_data (self):
        self.instance_a = "test://ontology-changes/properties/add-domain-index/a"
        self.tracker.update ("""
            INSERT { <%s> a test:B ;
                          test:a_string 'test-value' ;
                          test:a_n_cardinality 'another-test-value'. }""" % (self.instance_a))

        self.instance_b = "test://ontology-changes/properties/add-domain-index/b"
        self.tracker.update ("""
            INSERT { <%s> a test:C ;
                          test:a_string 'test-value' ;
                          test:a_n_cardinality 'another-test-value'. }""" % (self.instance_b))

    def validate_status (self):
        # Check the ontology
        has_domainIndex = self.tracker.ask ("ASK { test:B tracker:domainIndex test:a_string }")
        self.assertTrue (has_domainIndex)

        has_domainIndex = self.tracker.ask ("ASK { test:C tracker:domainIndex test:a_n_cardinality }")
        self.assertTrue (has_domainIndex)

        # Check the data
        dataok = self.tracker.ask ("ASK { <%s> test:a_string 'test-value' }" % (self.instance_a))
        self.assertTrue (dataok)

        dataok = self.tracker.ask ("ASK { <%s> test:a_n_cardinality 'another-test-value' }" % (self.instance_b))
        self.assertTrue (dataok)


class DomainIndexRemoveTest (OntologyChangeTestTemplate):
    """
    Remove tracker:domainIndex to a class and check there is no data loss.
    """
    def test_domain_index_remove (self):
        self.template_test_ontology_change ()

    def set_ontology_dirs (self):
        self.FIRST_ONTOLOGY_DIR = "add-domainIndex"
        self.SECOND_ONTOLOGY_DIR = "basic-future"

    def insert_data (self):
        self.instance_a = "test://ontology-changes/properties/add-domain-index/a"
        self.tracker.update ("""
            INSERT { <%s> a test:B ;
                          test:a_string 'test-value' ;
                          test:a_n_cardinality 'another-test-value'. }""" % (self.instance_a))

        self.instance_b = "test://ontology-changes/properties/add-domain-index/b"
        self.tracker.update ("""
            INSERT { <%s> a test:C ;
                          test:a_string 'test-value' ;
                          test:a_n_cardinality 'another-test-value'. }""" % (self.instance_b))

    def validate_status (self):
        # Check the ontology
        has_domainIndex = self.tracker.ask ("ASK { test:B tracker:domainIndex test:a_string }")
        self.assertFalse (has_domainIndex)

        has_domainIndex = self.tracker.ask ("ASK { test:C tracker:domainIndex test:a_n_cardinality }")
        self.assertFalse (has_domainIndex)

        # Check the data
        dataok = self.tracker.ask ("ASK { <%s> test:a_string 'test-value' }" % (self.instance_a))
        self.assertTrue (dataok)

        dataok = self.tracker.ask ("ASK { <%s> test:a_n_cardinality 'another-test-value' }" % (self.instance_b))
        self.assertTrue (dataok)


if __name__ == "__main__":
    ut.main ()

    
