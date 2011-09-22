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

"""
Test that resource removal does not leave debris or clobber too much,
especially in the case where nie:InformationElement != nie:DataObject
"""

from common.utils import configuration as cfg
from common.utils.dconf import DConfClient
from common.utils.helpers import MinerFsHelper, StoreHelper, ExtractorHelper, log
from common.utils.system import TrackerSystemAbstraction

import dbus
import glib
import os
import shutil
import unittest2 as ut

MINER_TMP_DIR = cfg.TEST_MONITORED_TMP_DIR

def get_test_path (filename):
    return os.path.join (MINER_TMP_DIR, filename)

def get_test_uri (filename):
    return "file://" + os.path.join (MINER_TMP_DIR, filename)


CONF_OPTIONS = [
    (cfg.DCONF_MINER_SCHEMA, "enable-writeback", "false"),
    (cfg.DCONF_MINER_SCHEMA, "index-recursive-directories", [MINER_TMP_DIR]),
    (cfg.DCONF_MINER_SCHEMA, "index-single-directories", "[]"),
    (cfg.DCONF_MINER_SCHEMA, "index-optical-discs", "true"),
    (cfg.DCONF_MINER_SCHEMA, "index-removable-devices", "false"),
    (cfg.DCONF_MINER_SCHEMA, "throttle", 5)
    ]

REASONABLE_TIMEOUT = 30

class MinerResourceRemovalTest (ut.TestCase):
    graph_updated_handler_id = 0

    # Use the same instances of store and miner-fs for the whole test suite,
    # because they take so long to do first-time init.
    @classmethod
    def setUpClass (self):
        log ("Using %s as temp dir\n" % MINER_TMP_DIR)
        if os.path.exists (MINER_TMP_DIR):
            shutil.rmtree (MINER_TMP_DIR)
        os.makedirs (MINER_TMP_DIR)

        self.system = TrackerSystemAbstraction ()
        self.system.set_up_environment (CONF_OPTIONS, None)
        self.store = StoreHelper ()
        self.store.start ()
        self.miner_fs = MinerFsHelper ()
        self.miner_fs.start ()

        # GraphUpdated seems to not be emitted if the extractor isn't running
        # even though the file resource still gets inserted - maybe because
        # INSERT SILENT is used in the FS miner?
        self.extractor = ExtractorHelper ()
        self.extractor.start ()

    @classmethod
    def tearDownClass (self):
        self.store.bus._clean_up_signal_match (self.graph_updated_handler_id)
        self.extractor.stop ()
        self.miner_fs.stop ()
        self.store.stop ()

    def setUp (self):
        self.inserts_list = []
        self.deletes_list = []
        self.inserts_match_function = None
        self.deletes_match_function = None
        self.match_timed_out = False

        self.graph_updated_handler_id = self.store.bus.add_signal_receiver (self._graph_updated_cb,
                                                                            signal_name = "GraphUpdated",
                                                                            path = "/org/freedesktop/Tracker1/Resources",
                                                                            dbus_interface = "org.freedesktop.Tracker1.Resources")

    def tearDown (self):
        self.system.unset_up_environment ()

    # A system to follow GraphUpdated and make sure all changes are tracked.
    # This code saves every change notification received, and exposes methods
    # to await insertion or deletion of a certain resource which first check
    # the list of events already received and wait for more if the event has
    # not yet happened.
    #
    # FIXME: put this stuff in StoreHelper
    def _timeout_cb (self):
        self.match_timed_out = True
        self.store.loop.quit ()
        # Don't fail here, exceptions don't get propagated correctly
        # from the GMainLoop

    def _graph_updated_cb (self, class_name, deletes_list, inserts_list):
        """
        Process notifications from tracker-store on resource changes.
        """
        matched = False
        if inserts_list is not None:
            if self.inserts_match_function is not None:
                # The match function will remove matched entries from the list
                (matched, inserts_list) = self.inserts_match_function (inserts_list)
            self.inserts_list += inserts_list

        if deletes_list is not None:
            if self.deletes_match_function is not None:
                (matched, deletes_list) = self.deletes_match_function (deletes_list)
            self.deletes_list += deletes_list

    def await_resource_inserted (self, rdf_class, url = None, title = None):
        """
        Block until a resource matching the parameters becomes available
        """
        assert (self.inserts_match_function == None)

        def match_cb (inserts_list, in_main_loop = True):
            matched = False
            filtered_list = []
            known_subjects = set ()

            #print "Got inserts: ", inserts_list, "\n"

            # FIXME: this could be done in an easier way: build one query that filters
            # based on every subject id in inserts_list, and returns the id of the one
            # that matched :)
            for insert in inserts_list:
                id = insert[1]

                if not matched and id not in known_subjects:
                    known_subjects.add (id)

                    where = "  ?urn a %s " % rdf_class

                    if url is not None:
                        where += "; nie:url \"%s\"" % url

                    if title is not None:
                        where += "; nie:title \"%s\"" % title

                    query = "SELECT ?urn WHERE { %s FILTER (tracker:id(?urn) = %s)}" % (where, insert[1])
                    #print "%s\n" % query
                    result_set = self.store.query (query)
                    #print result_set, "\n\n"

                    if len (result_set) > 0:
                        matched = True
                        self.matched_resource_urn = result_set[0][0]
                        self.matched_resource_id = insert[1]

                if not matched or id != self.matched_resource_id:
                    filtered_list += [insert]

            if matched and in_main_loop:
                glib.source_remove (self.graph_updated_timeout_id)
                self.graph_updated_timeout_id = 0
                self.inserts_match_function = None
                self.store.loop.quit ()

            return (matched, filtered_list)


        self.matched_resource_urn = None
        self.matched_resource_id = None

        log ("Await new %s (%i existing inserts)" % (rdf_class, len (self.inserts_list)))

        # Check the list of previously received events for matches
        (existing_match, self.inserts_list) = match_cb (self.inserts_list, False)

        if not existing_match:
            self.graph_updated_timeout_id = glib.timeout_add_seconds (REASONABLE_TIMEOUT, self._timeout_cb)
            self.inserts_match_function = match_cb

            # Run the event loop until the correct notification arrives
            self.store.loop.run ()

        if self.match_timed_out:
            self.fail ("Timeout waiting for resource: class %s, URL %s, title %s" % (rdf_class, url, title))

        return (self.matched_resource_id, self.matched_resource_urn)


    def await_resource_deleted (self, id, fail_message = None):
        """
        Block until we are notified of a resources deletion
        """
        assert (self.deletes_match_function == None)

        def match_cb (deletes_list, in_main_loop = True):
            matched = False
            filtered_list = []

            #print "Looking for %i in " % id, deletes_list, "\n"

            for delete in deletes_list:
                if delete[1] == id:
                    matched = True
                else:
                    filtered_list += [delete]

            if matched and in_main_loop:
                glib.source_remove (self.graph_updated_timeout_id)
                self.graph_updated_timeout_id = 0
                self.deletes_match_function = None

            self.store.loop.quit ()

            return (matched, filtered_list)

        log ("Await deletion of %i (%i existing)" % (id, len (self.deletes_list)))

        (existing_match, self.deletes_list) = match_cb (self.deletes_list, False)

        if not existing_match:
            self.graph_updated_timeout_id = glib.timeout_add_seconds (REASONABLE_TIMEOUT, self._timeout_cb)
            self.deletes_match_function = match_cb

            # Run the event loop until the correct notification arrives
            self.store.loop.run ()

        if self.match_timed_out:
            if fail_message is not None:
                self.fail (fail_message)
            else:
                self.fail ("Resource %i has not been deleted." % id)

        return


    def create_test_content (self, file_urn, title):
        sparql = "INSERT { \
                    _:ie a nmm:MusicPiece ; \
                         nie:title \"%s\" ; \
                         nie:isStoredAs <%s> \
                  } " % (title, file_urn)

        self.store.update (sparql)

        return self.await_resource_inserted (rdf_class = 'nmm:MusicPiece',
                                             title = title)

    def create_test_file (self, file_name):
        file_path = get_test_path (file_name)

        file = open (file_path, 'w')
        file.write ("Test")
        file.close ()

        return self.await_resource_inserted (rdf_class = 'nfo:Document',
                                             url = get_test_uri (file_name));

    def assertResourceExists (self, urn):
        if self.store.ask ("ASK { <%s> a rdfs:Resource }" % urn) == False:
            self.fail ("Resource <%s> does not exist" % urn)

    def assertResourceMissing (self, urn):
        if self.store.ask ("ASK { <%s> a rdfs:Resource }" % urn) == True:
            self.fail ("Resource <%s> should not exist" % urn)


    def test_01_file_deletion (self):
        """
        Ensure every logical resource (nie:InformationElement) contained with
        in a file is deleted when the file is deleted.
        """

        (file_1_id, file_1_urn) = self.create_test_file ("test_1.txt")
        (file_2_id, file_2_urn) = self.create_test_file ("test_2.txt")
        (ie_1_id, ie_1_urn) = self.create_test_content (file_1_urn, "Test resource 1")
        (ie_2_id, ie_2_urn) = self.create_test_content (file_2_urn, "Test resource 2")

        os.unlink (get_test_path ("test_1.txt"))

        self.await_resource_deleted (file_1_id)
        self.await_resource_deleted (ie_1_id,
                                     "Associated logical resource failed to be deleted " \
                                     "when its containing file was removed.")

        self.assertResourceMissing (file_1_urn)
        self.assertResourceMissing (ie_1_urn)
        self.assertResourceExists (file_2_urn)
        self.assertResourceExists (ie_2_urn)

    def test_02_removable_device_data (self):
        """
        Tracker does periodic cleanups of data on removable volumes that haven't
        been seen since 'removable-days-threshold', and will also remove all data
        from removable volumes if 'index-removable-devices' is disabled.

        FIXME: not yet possible to test this - we need some way of mounting
        a fake removable volume: https://bugzilla.gnome.org/show_bug.cgi?id=659739
        """

        #dconf = DConfClient ()
        #dconf.write (cfg.DCONF_MINER_SCHEMA, 'index-removable-devices', 'true')

        #self.mount_test_removable_volume ()

        #self.add_test_resource ("urn:test:1", test_volume_urn)
        #self.add_test_resource ("urn:test:2", None)

        # Trigger removal of all resources from removable devices
        #dconf.write (cfg.DCONF_MINER_SCHEMA, 'index-removable-devices', 'false')

        # Check that only the data on the removable volume was deleted
        #self.await_updates (2)


if __name__ == "__main__":
    ut.main()
