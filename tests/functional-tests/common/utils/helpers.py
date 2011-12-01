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
import glib
import gobject
import commands
import os
import signal
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
    """

    BUS_NAME = None
    PROCESS_NAME = None

    def __init__ (self):
        self.loop = None
        self.bus = None
        self.bus_admin = None

    def _get_bus (self):
        if self.bus is not None:
            return

        self.loop = gobject.MainLoop ()

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

            return subprocess.Popen ([path] + flags, **kws)

    def _stop_process (self):
        if options.is_manual_start ():
            if self.available:
                print ("Kill %s manually" % self.PROCESS_NAME)
                self.loop.run ()
        else:
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

        raise Exception("%s exited with status: %i" % (self.PROCESS_NAME, status))

    def _timeout_on_idle_cb (self):
        log ("[%s] Timeout waiting... asumming idle." % self.PROCESS_NAME)
        self.loop.quit ()
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

        # Run the loop until the bus name appears, or the process dies.
        self.process_watch_timeout = glib.timeout_add (200, self._process_watch_cb)

        self.loop.run ()

        glib.source_remove (self.process_watch_timeout)

    def stop (self):
        if self.available:
            # It should step out of this loop when the miner disappear from the bus
            glib.idle_add (self._stop_process)
            self.timeout_id = glib.timeout_add_seconds (REASONABLE_TIMEOUT, self._timeout_on_idle_cb)
            self.loop.run ()

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
        except dbus.DBusException:
            self.connect ()
            result = self.resources.SparqlQuery (QUERY % (ontology_class))
            
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
        self.timeout_id = glib.timeout_add_seconds (REASONABLE_TIMEOUT, self._timeout_on_idle_cb)
        self.loop.run ()
        glib.source_remove (self.timeout_id)

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
        self.timeout_id = glib.timeout_add_seconds (REASONABLE_TIMEOUT, self._timeout_on_idle_cb)

        self.loop.run ()

        glib.source_remove (self.timeout_id)
        self.bus._clean_up_signal_match (self.status_match)


class ExtractorHelper (Helper):

    PROCESS_NAME = 'tracker-extract'
    BUS_NAME = cfg.TRACKER_EXTRACT_BUSNAME

    def start (self):
        Helper.start (self)

        bus_object = self.bus.get_object (cfg.TRACKER_EXTRACT_BUSNAME,
                                          cfg.TRACKER_EXTRACT_OBJ_PATH)
        self.extractor = dbus.Interface (bus_object,
                                         dbus_interface=cfg.TRACKER_EXTRACT_IFACE)

        # FIXME: interface does not appear straight away
        time.sleep (0.2)

    def get_metadata (self, filename, mime):
        """
        Calls the extractor a returns a dictionary of property, value.
        Example:
         { 'nie:filename': 'a.jpeg' ,
           'tracker:added': '2008-12-12T12:23:34Z'
         }
        """
        metadata = {}
        try:
            preupdate, postupdate, embedded, where = self.extractor.GetMetadata (filename, mime, "")
            extras = self.__process_where_part (where)
            for attribute_value in self.__process_lines (embedded):
                att, value = attribute_value.split (" ", 1)
                if value.startswith ("?") and extras.has_key (value):
                    value = extras[value]

                if metadata.has_key (att):
                    metadata [att].append (value)
                else:
                    metadata [att] = [value]

            return metadata
        except dbus.DBusException, e:
            raise NoMetadataException ()
            
    def __process_lines (self, embedded):
        """
        Translate each line in a "prop value" string, handling anonymous nodes.

        Example:
             nfo:width 699 ;  -> 'nfo:width 699'
        or
             nao:hasTag [ a nao:Tag ;
             nao:prefLabel "tracker"] ;  -> nao:hasTag:prefLabel 'tracker'

        Would be so cool to implement this with yield and generators... :)
        """
        grouped_lines = []
        current_line = ""
        anon_node_open = False
        for l in embedded.split ("\n\t"):
            if "[" in l:
                current_line = current_line + l
                anon_node_open = True
                continue

            if "]" in l:
                anon_node_open = False
                current_line += l
                final_lines = self.__handle_anon_nodes (current_line.strip ())
                grouped_lines = grouped_lines + final_lines
                current_line = ""
                continue

            if anon_node_open:
                current_line += l
            else:
                if (len (l.strip ()) == 0):
                    continue
                    
                final_lines = self.__handle_multivalues (l.strip ())
                grouped_lines = grouped_lines + final_lines

        return map (self.__clean_value, grouped_lines)

    def __process_where_part (self, where):
        gettags = re.compile ("(\?\w+)\ a\ nao:Tag\ ;\ nao:prefLabel\ \"([\w\ -]+)\"")
        tags = {}
        for l in where.split ("\n"):
            if len (l) == 0:
                continue
            match = gettags.search (l)
            if (match):
                tags [match.group(1)] = match.group (2)
            else:
                print "This line is not a tag:", l

        return tags

    def __handle_multivalues (self, line):
        """
        Split multivalues like:
        a nfo:Image, nmm:Photo ;
           -> a nfo:Image ;
           -> a nmm:Photo ;
        """
        hasEscapedComma = re.compile ("\".+,.+\"")

        if "," in line and not hasEscapedComma.search (line):
            prop, multival = line.split (" ", 1)
            results = []
            for value in multival.split (","):
                results.append ("%s %s" % (prop, value.strip ()))
            return results
        else:
            return [line]
       
    def __handle_anon_nodes (self, line):
        """
        Traslates anonymous nodes in 'flat' properties:

        nao:hasTag [a nao:Tag; nao:prefLabel "xxx"]
                 -> nao:hasTag:prefLabel "xxx"
                 
        slo:location [a slo:GeoLocation; slo:postalAddress <urn:uuid:1231-123> .]
                -> slo:location <urn:uuid:1231-123> 
                
        nfo:hasMediaFileListEntry [ a nfo:MediaFileListEntry ; nfo:entryUrl "file://x.mp3"; nfo:listPosition 1]
                -> nfo:hasMediaFileListEntry:entryUrl "file://x.mp3"

        """
        
        # hasTag case
        if line.startswith ("nao:hasTag"):
            getlabel = re.compile ("nao:prefLabel\ \"([\w\ -]+)\"")
            match = getlabel.search (line)
            if (match):
                line = 'nao:hasTag:prefLabel "%s" ;' % (match.group(1))
                return [line]
            else:
                print "Whats wrong on line", line, "?"
                return [line]

        # location case
        elif line.startswith ("slo:location"):
            results = []

            # Can have country AND/OR city
            getpa = re.compile ("slo:postalAddress\ \<([\w:-]+)\>")
            pa_match = getpa.search (line)
            
            if (pa_match):
                results.append ('slo:location:postalAddress "%s" ;' % (pa_match.group(1)))
            else:
                print "FIXME another location subproperty in ", line

            return results
        elif line.startswith ("nco:creator"):
            getcreator = re.compile ("nco:fullname\ \"([\w\ ]+)\"")
            creator_match = getcreator.search (line)

            if (creator_match):
                new_line = 'nco:creator:fullname "%s" ;' % (creator_match.group (1))
                return [new_line]
            else:
                print "Something special in this line '%s'" % (line)

        elif line.startswith ("nfo:hasMediaFileListEntry"):
            return self.__handle_playlist_entries (line)
        
        else:
            return [line]

    def __handle_playlist_entries (self, line):
        """
        Playlist entries come in one big line:
        nfo:hMFLE [ a nfo:MFLE; nfo:entryUrl '...'; nfo:listPosition X] , [ ... ], [ ... ]
          -> nfo:hMFLE:entryUrl '...'
          -> nfo:hMFLE:entryUrl '...'
          ...
        """
        geturl = re.compile ("nfo:entryUrl \"([\w\.\:\/]+)\"")
        entries = line.strip () [len ("nfo:hasMediaFileListEntry"):]
        results = []
        for entry in entries.split (","):
            url_match = geturl.search (entry)
            if (url_match):
                new_line = 'nfo:hasMediaFileListEntry:entryUrl "%s" ;' % (url_match.group (1))
                results.append (new_line)
            else:
                print " *** Something special in this line '%s'" % (entry)
        return results

    def __clean_value (self, value):
        """
        the value comes with a ';' or a '.' at the end
        """
        if (len (value) < 2):
            return value.strip ()
        
        clean = value.strip ()
        if value[-1] in [';', '.']:
            clean = value [:-1]

        clean = clean.replace ("\"", "")
            
        return clean.strip ()


class WritebackHelper (Helper):

    PROCESS_NAME = 'tracker-writeback'
    PROCESS_PATH = os.path.join (cfg.EXEC_PREFIX, 'tracker-writeback')
    BUS_NAME = cfg.WRITEBACK_BUSNAME
