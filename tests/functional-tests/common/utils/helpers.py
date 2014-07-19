#!/usr/bin/python
#
# Copyright (C) 2010, Nokia <jean-luc.lamadon@nokia.com>
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
from gi.repository import GLib
from gi.repository import GObject
import os
import sys
import subprocess
import time
from dbus.mainloop.glib import DBusGMainLoop
import re

import configuration as cfg
import options

class NoMetadataException (Exception):
    pass

REASONABLE_TIMEOUT = 30

def log (message):
    if options.is_verbose ():
        print (message)

class Helper:
    """
    Abstract helper for Tracker processes. Launches the process manually
    and waits for it to appear on the session bus.

    The helper will fail if the process is already running. Use
    test-runner.sh to ensure the processes run inside a separate DBus
    session bus.

    The process is watched using a timed GLib main loop source. If the process
    exits with an error code, the test will abort the next time the main loop
    is entered (or straight away if currently running the main loop). Tests
    that block waiting for results in time.sleep() won't benefit from this, but
    it works for those that use await_resource_inserted()/deleted() and others.
    """

    BUS_NAME = None
    PROCESS_NAME = None

    def __init__ (self):
        self.loop = None
        self.bus = None
        self.bus_admin = None

    def install_glib_excepthook(self, loop):
        """
        Handler to abort test if an exception occurs inside the GLib main loop.
        """
        old_hook = sys.excepthook
        def new_hook(etype, evalue, etb):
            old_hook(etype, evalue, etb)
            GLib.MainLoop.quit(loop)
            sys.exit()
        sys.excepthook = new_hook

    def _get_bus (self):
        if self.bus is not None:
            return

        self.loop = GObject.MainLoop ()

        self.install_glib_excepthook(self.loop)

        dbus_loop = DBusGMainLoop (set_as_default=True)
        self.bus = dbus.SessionBus (dbus_loop)

        obj = self.bus.get_object ("org.freedesktop.DBus",
                                   "/org/freedesktop/DBus")
        self.bus_admin = dbus.Interface (obj, dbus_interface = "org.freedesktop.DBus")

    def _start_process (self):
        path = getattr (self,
                        "PROCESS_PATH",
                        os.path.join (cfg.EXEC_PREFIX, self.PROCESS_NAME))
        flags = getattr (self,
                         "FLAGS",
                         [])

        if options.is_manual_start ():
            print ("Start %s manually" % self.PROCESS_NAME)
        else:
            kws = {}

            if not options.is_verbose ():
                FNULL = open ('/dev/null', 'w')
                kws = { 'stdout': FNULL, 'stderr': FNULL }

            command = [path] + flags
            log ("Starting %s" % ' '.join(command))
            return subprocess.Popen ([path] + flags, **kws)

    def _stop_process (self):
        if options.is_manual_start ():
            if self.available:
                print ("Kill %s manually" % self.PROCESS_NAME)
                self.loop.run ()
        else:
            # FIXME: this can hang if the we try to terminate the
            # process too early in its lifetime ... if you see:
            #
            #   GLib-CRITICAL **: g_main_loop_quit: assertion 'loop != NULL' failed
            #
            # This means that the signal_handler() function was called
            # before the main loop was started and the process failed to
            # terminate.
            self.process.terminate ()
            self.process.wait ()
        return False

    def _name_owner_changed_cb (self, name, old_owner, new_owner):
        if name == self.BUS_NAME:
            if old_owner == '' and new_owner != '':
                log ("[%s] appeared in the bus" % self.PROCESS_NAME)
                self.available = True
            elif old_owner != ''  and new_owner == '':
                log ("[%s] disappeared from the bus" % self.PROCESS_NAME)
                self.available = False
            else:
                log ("[%s] name change %s -> %s" % (self.PROCESS_NAME, old_owner, new_owner))

            self.loop.quit ()

    def _process_watch_cb (self):
        status = self.process.poll ()

        if status is None:
            return True

        if status == 0 and not self.abort_if_process_exits_with_status_0:
            return True

        raise Exception("%s exited with status: %i" % (self.PROCESS_NAME, status))

    def _timeout_on_idle_cb (self):
        log ("[%s] Timeout waiting... asumming idle." % self.PROCESS_NAME)
        self.loop.quit ()
        self.timeout_id = None
        return False


    def start (self):
        """
        Start an instance of process and wait for it to appear on the bus.
        """

        self._get_bus ()

        if (self.bus_admin.NameHasOwner (self.BUS_NAME)):
            raise Exception ("Unable to start test instance of %s: already running" % self.PROCESS_NAME)

        self.name_owner_match = self.bus.add_signal_receiver (self._name_owner_changed_cb,
                                                              signal_name="NameOwnerChanged",
                                                              path="/org/freedesktop/DBus",
                                                              dbus_interface="org.freedesktop.DBus")

        self.process = self._start_process ()
        log ('[%s] Started process %i' % (self.PROCESS_NAME, self.process.pid))

        self.process_watch_timeout = GLib.timeout_add (200, self._process_watch_cb)

        self.abort_if_process_exits_with_status_0 = True

        # Run the loop until the bus name appears, or the process dies.
        self.loop.run ()

        self.abort_if_process_exits_with_status_0 = False

    def stop (self):
        if self.available:
            # It should step out of this loop when the miner disappear from the bus
            GLib.source_remove(self.process_watch_timeout)

            GLib.idle_add (self._stop_process)
            self.timeout_id = GLib.timeout_add_seconds (REASONABLE_TIMEOUT, self._timeout_on_idle_cb)
            self.loop.run ()
            if self.timeout_id is not None:
                GLib.source_remove(self.timeout_id)

        log ("[%s] stop." % self.PROCESS_NAME)
        # Disconnect the signals of the next start we get duplicated messages
        self.bus._clean_up_signal_match (self.name_owner_match)

    def kill (self):
        self.process.kill ()

        # Name owner changed callback should take us out from this loop
        self.loop.run ()

        log ("[%s] killed." % self.PROCESS_NAME)
        self.bus._clean_up_signal_match (self.name_owner_match)


class StoreHelper (Helper):
    """
    Wrapper for the Store API

    Every method tries to reconnect once if there is a dbus exception
    (some tests kill the daemon and make the connection useless)
    """

    PROCESS_NAME = "tracker-store"
    BUS_NAME = cfg.TRACKER_BUSNAME

    graph_updated_handler_id = 0

    def start (self):
        Helper.start (self)

        tracker = self.bus.get_object (cfg.TRACKER_BUSNAME,
                                       cfg.TRACKER_OBJ_PATH)

        self.resources = dbus.Interface (tracker,
                                         dbus_interface=cfg.RESOURCES_IFACE)

        tracker_backup = self.bus.get_object (cfg.TRACKER_BUSNAME, cfg.TRACKER_BACKUP_OBJ_PATH)
        self.backup_iface = dbus.Interface (tracker_backup, dbus_interface=cfg.BACKUP_IFACE)

        tracker_stats = self.bus.get_object (cfg.TRACKER_BUSNAME, cfg.TRACKER_STATS_OBJ_PATH)

        self.stats_iface = dbus.Interface (tracker_stats, dbus_interface=cfg.STATS_IFACE)

        tracker_status = self.bus.get_object (cfg.TRACKER_BUSNAME,
                                              cfg.TRACKER_STATUS_OBJ_PATH)
        self.status_iface = dbus.Interface (tracker_status, dbus_interface=cfg.STATUS_IFACE)

        log ("[%s] booting..." % self.PROCESS_NAME)
        self.status_iface.Wait ()
        log ("[%s] ready." % self.PROCESS_NAME)

        self.reset_graph_updates_tracking ()
        self.graph_updated_handler_id = self.bus.add_signal_receiver (self._graph_updated_cb,
                                                                      signal_name = "GraphUpdated",
                                                                      path = cfg.TRACKER_OBJ_PATH,
                                                                      dbus_interface = cfg.RESOURCES_IFACE)

    def stop (self):
        Helper.stop (self)

        self.bus._clean_up_signal_match (self.graph_updated_handler_id)

    # A system to follow GraphUpdated and make sure all changes are tracked.
    # This code saves every change notification received, and exposes methods
    # to await insertion or deletion of a certain resource which first check
    # the list of events already received and wait for more if the event has
    # not yet happened.

    def reset_graph_updates_tracking (self):
        self.inserts_list = []
        self.deletes_list = []
        self.inserts_match_function = None
        self.deletes_match_function = None
        self.graph_updated_timed_out = False

    def _graph_updated_timeout_cb (self):
        # Don't fail here, exceptions don't get propagated correctly
        # from the GMainLoop
        self.graph_updated_timed_out = True
        self.loop.quit ()

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
                    result_set = self.query (query)
                    #print result_set, "\n\n"

                    if len (result_set) > 0:
                        matched = True
                        self.matched_resource_urn = result_set[0][0]
                        self.matched_resource_id = insert[1]

                if not matched or id != self.matched_resource_id:
                    filtered_list += [insert]

            if matched and in_main_loop:
                GLib.source_remove (self.graph_updated_timeout_id)
                self.graph_updated_timeout_id = 0
                self.inserts_match_function = None
                self.loop.quit ()

            return (matched, filtered_list)


        self.matched_resource_urn = None
        self.matched_resource_id = None

        log ("Await new %s (%i existing inserts)" % (rdf_class, len (self.inserts_list)))

        # Check the list of previously received events for matches
        (existing_match, self.inserts_list) = match_cb (self.inserts_list, False)

        if not existing_match:
            self.graph_updated_timeout_id = GLib.timeout_add_seconds (REASONABLE_TIMEOUT,
                                                                      self._graph_updated_timeout_cb)
            self.inserts_match_function = match_cb

            # Run the event loop until the correct notification arrives
            self.loop.run ()

        if self.graph_updated_timed_out:
            raise Exception ("Timeout waiting for resource: class %s, URL %s, title %s" % (rdf_class, url, title))

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
                GLib.source_remove (self.graph_updated_timeout_id)
                self.graph_updated_timeout_id = 0
                self.deletes_match_function = None

            self.loop.quit ()

            return (matched, filtered_list)

        log ("Await deletion of %i (%i existing)" % (id, len (self.deletes_list)))

        (existing_match, self.deletes_list) = match_cb (self.deletes_list, False)

        if not existing_match:
            self.graph_updated_timeout_id = GLib.timeout_add_seconds (REASONABLE_TIMEOUT,
                                                                      self._graph_updated_timeout_cb)
            self.deletes_match_function = match_cb

            # Run the event loop until the correct notification arrives
            self.loop.run ()

        if self.graph_updated_timed_out:
            if fail_message is not None:
                raise Exception (fail_message)
            else:
                raise Exception ("Resource %i has not been deleted." % id)

        return

    def query (self, query, timeout=5000):
        try:
            return self.resources.SparqlQuery (query, timeout=timeout)
        except dbus.DBusException as (e):
            if (e.get_dbus_name().startswith ("org.freedesktop.DBus")):
                self.start ()
                return self.resources.SparqlQuery (query, timeout=timeout)
            raise (e)

    def update (self, update_sparql, timeout=5000):
        try:
            return self.resources.SparqlUpdate (update_sparql, timeout=timeout)
        except dbus.DBusException as (e):
            if (e.get_dbus_name().startswith ("org.freedesktop.DBus")):
                self.start ()
                return self.resources.SparqlUpdate (update_sparql, timeout=timeout)
            raise (e)

    def batch_update (self, update_sparql):
        try:
            return self.resources.BatchSparqlUpdate (update_sparql)
        except dbus.DBusException as (e):
            if (e.get_dbus_name().startswith ("org.freedesktop.DBus")):
                self.start ()
                return self.resources.BatchSparqlUpdate (update_sparql)
            raise (e)

    def batch_commit (self):
        return self.resources.BatchCommit ()

    def backup (self, backup_file):
        try:
            self.backup_iface.Save (backup_file)
        except dbus.DBusException as (e):
            if (e.get_dbus_name().startswith ("org.freedesktop.DBus")):
                self.start ()
                return self.backup_iface.Save (backup_file)
            raise (e)
            
    def restore (self, backup_file):
        try:
            return self.backup_iface.Restore (backup_file)
        except dbus.DBusException as (e):
            if (e.get_dbus_name().startswith ("org.freedesktop.DBus")):
                self.start ()
                return self.backup_iface.Restore (backup_file)
            raise (e)

    def get_stats (self):
        try:
            return self.stats_iface.Get ()
        except dbus.DBusException as (e):
            if (e.get_dbus_name().startswith ("org.freedesktop.DBus")):
                self.start ()
                return self.stats_iface.Get ()
            raise (e)


    def get_tracker_iface (self):
        return self.resources

    def count_instances (self, ontology_class):
        QUERY = """
        SELECT COUNT(?u) WHERE {
            ?u a %s .
        }
        """
        try:
            result = self.resources.SparqlQuery (QUERY % (ontology_class))
        except dbus.DBusException as (e):
            if (e.get_dbus_name().startswith ("org.freedesktop.DBus")):
                self.start ()
                result = self.resources.SparqlQuery (QUERY % (ontology_class))
            else:
                raise (e)
            
        if (len (result) == 1):
            return int (result [0][0])
        else:
            return -1


    def ask (self, ask_query):
        assert ask_query.strip ().startswith ("ASK")
        result = self.query (ask_query)
        assert len (result) == 1
        if result[0][0] == "true":
            return True
        elif result[0][0] == "false":
            return False
        else:
            raise Exception ("Something fishy is going on")


class MinerFsHelper (Helper):

    PROCESS_NAME = 'tracker-miner-fs'
    PROCESS_PATH = os.path.join (cfg.EXEC_PREFIX, "tracker-miner-fs")
    BUS_NAME = cfg.MINERFS_BUSNAME

    FLAGS = ['--initial-sleep=0']
    if cfg.haveMaemo:
        FLAGS.append ('--disable-miner=userguides')

    def _stop_process (self):
        if options.is_manual_start ():
            if self.available:
                log ("Kill %s manually" % self.PROCESS_NAME)
                self.loop.run ()
        else:
            control_binary = os.path.join (cfg.BINDIR, "tracker-control")

            kws = {}

            if not options.is_verbose ():
                FNULL = open ('/dev/null', 'w')
                kws = { 'stdout': FNULL }

            subprocess.call ([control_binary, "--kill=miners"], **kws)

        return False

    def _minerfs_status_cb (self, status, progress, remaining_time):
        if (status == "Idle"):
            self.loop.quit ()

    def start (self):
        Helper.start (self)

        self.status_match = self.bus.add_signal_receiver (self._minerfs_status_cb,
                                                          signal_name="Progress",
                                                          path=cfg.MINERFS_OBJ_PATH,
                                                          dbus_interface=cfg.MINER_IFACE)

        # It should step out of this loop after progress changes to "Idle"
        self.timeout_id = GLib.timeout_add_seconds (REASONABLE_TIMEOUT, self._timeout_on_idle_cb)
        self.loop.run ()
        if self.timeout_id is not None:
            GLib.source_remove (self.timeout_id)

        bus_object = self.bus.get_object (cfg.MINERFS_BUSNAME,
                                          cfg.MINERFS_OBJ_PATH)
        self.miner_fs = dbus.Interface (bus_object,
                                        dbus_interface = cfg.MINER_IFACE)

    def stop (self):
        Helper.stop (self)

        self.bus._clean_up_signal_match (self.status_match)

    def ignore (self, filelist):
        self.miner_fs.IgnoreNextUpdate (filelist)

    def wait_for_idle (self, timeout=REASONABLE_TIMEOUT):
        """
        Block until the miner has finished crawling and its status becomes "Idle"
        """

        self.status_match = self.bus.add_signal_receiver (self._minerfs_status_cb,
                                                          signal_name="Progress",
                                                          path=cfg.MINERFS_OBJ_PATH,
                                                          dbus_interface=cfg.MINER_IFACE)
        self.timeout_id = GLib.timeout_add_seconds (REASONABLE_TIMEOUT, self._timeout_on_idle_cb)

        self.loop.run ()

        if self.timeout_id is not None:
            GLib.source_remove (self.timeout_id)
        self.bus._clean_up_signal_match (self.status_match)


class ExtractorHelper (Helper):

    PROCESS_NAME = 'tracker-extract'
    BUS_NAME = cfg.TRACKER_EXTRACT_BUSNAME

class WritebackHelper (Helper):

    PROCESS_NAME = 'tracker-writeback'
    PROCESS_PATH = os.path.join (cfg.EXEC_PREFIX, 'tracker-writeback')
    BUS_NAME = cfg.WRITEBACK_BUSNAME
