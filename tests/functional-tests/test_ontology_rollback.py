# Copyright (C) 2021, Abanoub Ghadban <abanoub.gdb@gmail.com>
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
Test how the database is kept in consistent state when errors occur.
"""

import gi

gi.require_version("Tracker", "3.0")
from gi.repository import GLib
from gi.repository import Gio
from gi.repository import Tracker

import os
import pathlib
import shutil
import re
import tempfile
import time
import glob
import unittest as ut

import storehelper

import configuration as cfg
import fixtures


RDFS_RANGE = "http://www.w3.org/2000/01/rdf-schema#range"
XSD_DATETIME = "http://www.w3.org/2001/XMLSchema#dateTime"
XSD_STRING = "http://www.w3.org/2001/XMLSchema#string"
XSD_INTEGER = "http://www.w3.org/2001/XMLSchema#integer"

TEST_PREFIX = "http://example.org/ns#"
TEST2_PREFIX = "http://example2.org/ns#"


class OntologyRollbackTestTemplate(ut.TestCase):
    """
    Template class for the ontology rollback tests. It ensures that the db
    is left in a consistent state when building or updating the ontology fails.
    The tests are subclasses of this, implementing these methods:

       * set_ontology_dirs
       * insert_data_into_first_ontology
       * validate_first_ontology_status
       * insert_data_into_second_ontology
       * validate_second_ontology_status

    and adding a method 'test_x_y_z' to be invoked by unittest.

    Check doc in those methods for the specific details.
    """

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="tracker-test-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def ontology_path(self, param):
        return pathlib.Path(__file__).parent.joinpath("data", "ontologies", param)

    def template_test_ontology_rollback(self):
        self.set_ontology_dirs()

        self.__assert_different_ontology_dates(
            self.FIRST_ONTOLOGY_DIR, self.SECOND_ONTOLOGY_DIR
        )
        self.__assert_same_ontology_dates_if_exist(
            self.FIRST_ONTOLOGY_DIR, self.FIRST_MALFORMED_ONTOLOGY_DIR
        )
        self.__assert_same_ontology_dates_if_exist(
            self.SECOND_ONTOLOGY_DIR, self.SECOND_MALFORMED_ONTOLOGY_DIR
        )

        # Make sure that the connection fails when the malformed ontology is used
        with self.assertRaises(GLib.GError):
            Tracker.SparqlConnection.new(
                Tracker.SparqlConnectionFlags.NONE,
                Gio.File.new_for_path(self.tmpdir),
                Gio.File.new_for_path(
                    str(self.ontology_path(self.FIRST_MALFORMED_ONTOLOGY_DIR))
                ),
                None,
            )

        # Use the error-free first ontology. It should work now
        conn1 = Tracker.SparqlConnection.new(
            Tracker.SparqlConnectionFlags.NONE,
            Gio.File.new_for_path(self.tmpdir),
            Gio.File.new_for_path(str(self.ontology_path(self.FIRST_ONTOLOGY_DIR))),
            None,
        )

        self.tracker = storehelper.StoreHelper(conn1)
        self.insert_data_into_first_ontology()
        self.validate_first_ontology_status()

        conn1.close()

        # Reopen the local store with the second malformed set of ontologies.
        # The connection should fail
        with self.assertRaises(GLib.GError):
            Tracker.SparqlConnection.new(
                Tracker.SparqlConnectionFlags.NONE,
                Gio.File.new_for_path(self.tmpdir),
                Gio.File.new_for_path(
                    str(self.ontology_path(self.SECOND_MALFORMED_ONTOLOGY_DIR))
                ),
                None,
            )

        conn2 = Tracker.SparqlConnection.new(
            Tracker.SparqlConnectionFlags.NONE,
            Gio.File.new_for_path(self.tmpdir),
            Gio.File.new_for_path(str(self.ontology_path(self.SECOND_ONTOLOGY_DIR))),
            None,
        )
        self.tracker = storehelper.StoreHelper(conn2)

        self.insert_data_into_second_ontology()
        self.validate_second_ontology_status()

        conn2.close()

    def set_ontology_dirs(self):
        """
        Implement this method in the subclass setting values for:
        self.FIRST_MALFORMED_ONTOLOGY_DIR,
        self.FIRST_ONTOLOGY_DIR,
        self.SECOND_MALFORMED_ONTOLOGY_DIR and
        self.SECOND_ONTOLOGY_DIR
        """
        raise Exception("Subclasses must implement 'set_ontology_dir'")

    def insert_data_into_first_ontology(self):
        """
        Store some data with the FIRST ontology
        Make sure that it can't insert data into properties and classes that
        exist in the malformed ontology only
        """
        raise Exception("Subclasses must implement 'insert_data_into_first_ontology'")

    def validate_first_ontology_status(self):
        """
        This is called after inserting the data into the first ontology
        Check that the data is inserted successfully and the database schema
        matches that of the correct ontology and the schema of the malformed
        ontology is completely rolled back
        """
        raise Exception("Subclasses must implement 'validate_first_ontology_status'")

    def insert_data_into_second_ontology(slef):
        """
        Store some data with the SECOND ontology
        Make sure that it can't insert data into properties and classes that
        exist in the malformed second ontology only
        """
        raise Exception("Subclasses must implement 'insert_data_into_second_ontology'")

    def validate_second_ontology_status(self):
        """
        This is called after inserting the data into the second ontology
        Check that the data is inserted successfully and the database schema
        matches that of the correct second ontology and the schema of the malformed
        second ontology is completely rolled back
        """
        raise Exception("Subclasses must implement 'validate_second_ontology_status'")

    def assertInDbusResult(self, member, dbus_result, column=0):
        """
        Convenience assertion used in these tests
        """
        for row in dbus_result:
            if member == row[column]:
                return
        # This is going to fail with pretty printing
        self.assertIn(member, dbus_result)

    def assertNotInDbusResult(self, member, dbus_result, column=0):
        """
        Convenience assertion used in these tests
        """
        for row in dbus_result:
            if member == str(row[column]):
                # This is going to fail with pretty printing
                self.fail("'%s' wasn't supposed to be in '%s'" % (member, dbus_result))
        return

    def __get_ontology_date(self, ontology_path):
        """
        Returns the value of nrl:lastModified in the ontology file
        """
        ISO9601_REGEX = "(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)"

        with open(ontology_path, "r") as f:
            for line in f:
                if "nrl:lastModified" in line:
                    getmodtime = re.compile('nrl:lastModified\ "' + ISO9601_REGEX + '"')
                    modtime_match = getmodtime.search(line)

                    if modtime_match:
                        nao_date = modtime_match.group(1)
                        return time.strptime(nao_date, "%Y-%m-%dT%H:%M:%SZ")
                    else:
                        return None

    def __assert_all_ontology_dates(self, first_dir, second_dir, assertion_fn):
        """
        Compare between the nrl:lastModified of all ontology files in first_dir and second_dir
        assertion_fn is used to compare between the two dates and make sure they are valid
        """
        ontology_files = glob.glob(
            str(self.ontology_path(first_dir).joinpath("*.ontology"))
        )

        for first_ontology in ontology_files:
            ontology_fname = os.path.basename(first_ontology)
            second_ontology = self.ontology_path(second_dir).joinpath(ontology_fname)

            first_date = self.__get_ontology_date(first_ontology)
            second_date = self.__get_ontology_date(second_ontology)

            try:
                assertion_fn(first_date, second_date)
            except AssertionError as e:
                self.fail("%s: %s" % (first_ontology, e.msg))

    def __assert_different_ontology_dates(self, first_dir, second_dir):
        """
        Asserts that nrl:lastModified of ontologies in second_dir are more recent
        than that in first_dir
        """

        def assert_different_dates(first_date, second_date):
            if first_date >= second_date:
                self.fail("nrl:lastModified is not more recent in the second ontology")

        self.__assert_all_ontology_dates(first_dir, second_dir, assert_different_dates)

    def __assert_same_ontology_dates_if_exist(self, first_dir, second_dir):
        """
        Asserts that nrl:lastModified of ontologies in first_dir are the same as
        that in second_dir if they exist and are valid
        """

        def assert_different_dates(first_date, second_date):
            if second_date is not None and first_date != second_date:
                self.fail("nrl:lastModified is not the same as in the second ontology")

        self.__assert_all_ontology_dates(first_dir, second_dir, assert_different_dates)


class SimpleOntologyRollback(OntologyRollbackTestTemplate):
    def test_simple_ontology_rollback(self):
        self.template_test_ontology_rollback()

    def set_ontology_dirs(self):
        self.FIRST_MALFORMED_ONTOLOGY_DIR = "simple-with-errors"
        self.FIRST_ONTOLOGY_DIR = "simple"
        self.SECOND_MALFORMED_ONTOLOGY_DIR = "simple-updated-with-errors"
        self.SECOND_ONTOLOGY_DIR = "simple-updated"

    def insert_data_into_first_ontology(self):
        # test:a_tmp_prop only appeared in the malformed ontology
        with self.assertRaises(GLib.GError):
            self.tracker.update(
                "INSERT { <http://example/t1.1> a test:A ; test:a_tmp_prop 5. }"
            )

        # The domain of test:b_a_domain should be test:A no test:B
        with self.assertRaises(GLib.GError):
            self.tracker.update(
                "INSERT { <http://example/t1.2> a test:B ; test:b_a_domain 5. }"
            )

        # The domain should be test:B and range be test:A
        with self.assertRaises(GLib.GError):
            self.tracker.update(
                "INSERT { <http://example/t1.3> a test:B . <t1.4> a test:A ; test:a_b_domain_range <http://example/t1.3>. }"
            )

        # test2:C should be subclass of test:B not test:A
        with self.assertRaises(GLib.GError):
            self.tracker.update(
                "INSERT { <http://example/t1.5> a test2:C ; test:b_a_domain 5. }"
            )

        self.tracker.update(
            "INSERT { <http://example/t1.6> a test:A ; test:b_a_domain 5. }"
        )

        self.tracker.update(
            "INSERT { <http://example/t1.7> a test:B ; test:a_b_domain_range <http://example/t1.6>. }"
        )

        self.tracker.update(
            'INSERT { <http://example/t1.8> a test2:C ; test:b_range_boolean_string "String". }'
        )

    def validate_first_ontology_status(self):
        result = self.tracker.query("SELECT ?p WHERE { ?p a rdf:Property. }")
        self.assertNotInDbusResult(TEST_PREFIX + "a_tmp_prop", result)

        result = self.tracker.query(
            "SELECT ?d ?r WHERE { test:a_b_domain_range rdfs:domain ?d ; rdfs:range ?r }"
        )
        self.assertEqual(result[0][0], TEST_PREFIX + "B")
        self.assertEqual(result[0][1], TEST_PREFIX + "A")

        self.assertFalse(
            self.tracker.ask("ASK { <%s> a rdfs:Class}" % (TEST2_PREFIX + "D")),
            "test2:D class is not rolled back on failure",
        )

    def insert_data_into_second_ontology(self):
        # Domain was test:B in the malformed ontology
        # and became test:A in the error-free ontology
        with self.assertRaises(GLib.GError):
            self.tracker.update(
                "INSERT { <http://example/t2.1> a test:B ; test:a_b_domain 5. }"
            )

        self.tracker.update(
            "INSERT { <http://example/t2.2> a test:A ; test:a_b_domain 5. }"
        )

    def validate_second_ontology_status(self):
        result = self.tracker.query(
            "SELECT ?d ?r WHERE { test:a_b_domain rdfs:domain ?d ; rdfs:range ?r }"
        )
        self.assertEqual(result[0][0], TEST_PREFIX + "A")
        self.assertEqual(result[0][1], XSD_INTEGER)

        result = self.tracker.query(
            "SELECT ?v WHERE { <http://example/t2.2> test:a_b_domain ?v }"
        )
        self.assertEqual(result[0][0], "5")


if __name__ == "__main__":
    fixtures.tracker_test_main()
