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
import gobject
import commands
import os
import signal
from dbus.mainloop.glib import DBusGMainLoop
import configuration as cfg
import re

class NoMetadataException (Exception):
    pass

class StoreHelper:
    """
    Wrapper for the Store API

    Every method tries to reconnect once if there is a dbus exception
    (some tests kill the daemon and make the connection useless)
    """

    def __init__ (self):
        self.connect ()
        
    def connect (self):
        dbus_loop = DBusGMainLoop(set_as_default=True)
        bus = dbus.SessionBus (mainloop=dbus_loop)
        tracker = bus.get_object (cfg.TRACKER_BUSNAME, cfg.TRACKER_OBJ_PATH)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=cfg.RESOURCES_IFACE)
        tracker_backup = bus.get_object (cfg.TRACKER_BUSNAME, cfg.TRACKER_BACKUP_OBJ_PATH)
        self.backup_iface = dbus.Interface (tracker_backup, dbus_interface=cfg.BACKUP_IFACE)

        tracker_stats = bus.get_object (cfg.TRACKER_BUSNAME, cfg.TRACKER_STATS_OBJ_PATH)
        self.stats_iface = dbus.Interface (tracker_stats, dbus_interface=cfg.STATS_IFACE)

        tracker_status = bus.get_object (cfg.TRACKER_BUSNAME, cfg.TRACKER_STATUS_OBJ_PATH)
        self.status_iface = dbus.Interface (tracker_status, dbus_interface=cfg.STATUS_IFACE)

    def query (self, query, timeout=5000):
        try:
            return self.resources.SparqlQuery (query, timeout=timeout)
        except dbus.DBusException:
            self.connect ()
            return self.resources.SparqlQuery (query, timeout=timeout)

    def update (self, update_sparql, timeout=5000):
        try:
            return self.resources.SparqlUpdate (update_sparql, timeout=timeout)
        except dbus.DBusException:
            self.connect ()
            return self.resources.SparqlUpdate (update_sparql, timeout=timeout)

    def batch_update (self, update_sparql):
        try:
            return self.resources.BatchSparqlUpdate (update_sparql)
        except dbus.DBusException:
            self.connect ()
            return self.resources.BatchSparqlUpdate (update_sparql)

    def batch_commit (self):
        return self.resources.BatchCommit ()

    def backup (self, backup_file):
        try:
            self.backup_iface.Save (backup_file)
        except dbus.DBusException:
            self.connect ()
            self.backup_iface.Save (backup_file)
            
    def restore (self, backup_file):
        try:
            return self.backup_iface.Restore (backup_file)
        except dbus.DBusException:
            self.connect ()
            return self.backup_iface.Restore (backup_file)

    def get_stats (self):
        try:
            return self.stats_iface.Get ()
        except dbus.DBusException:
            self.connect ()
            return self.stats_iface.Get ()

    def wait (self):
        try:
            return self.status_iface.Wait ()
        except dbus.DBusException:
            self.connect ()
            return self.status_iface.Wait ()


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
            
        

        
class MinerHelper ():

    def __init__ (self):
        self.connect ()

    def connect (self):
        dbus_loop = DBusGMainLoop(set_as_default=True)
        bus = dbus.SessionBus (mainloop=dbus_loop)
        minerfs = bus.get_object (cfg.MINERFS_BUSNAME, cfg.MINERFS_OBJ_PATH)
        self.miner_fs = dbus.Interface (minerfs,
                                        dbus_interface=cfg.MINER_IFACE)
        
    def ignore (self, filelist):
        self.miner_fs.IgnoreNextUpdate (filelist)

        
class ExtractorHelper ():

    def __init__ (self):
        self.connect ()
    
    def connect (self):
        dbus_loop = DBusGMainLoop(set_as_default=True)
        bus = dbus.SessionBus (mainloop=dbus_loop)
        tracker = bus.get_object (cfg.TRACKER_EXTRACT_BUSNAME, cfg.TRACKER_EXTRACT_OBJ_PATH)
        self.extractor = dbus.Interface (tracker,
                                         dbus_interface=cfg.TRACKER_EXTRACT_IFACE)


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
            preupdate, embedded = self.extractor.GetMetadata (filename, mime)
            for attribute_value in self.__process_lines (embedded):
                att, value = attribute_value.split (" ", 1)
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

        pieces = embedded.split ("\n} } WHERE { {\n")
        if len (pieces) == 1:
            main = pieces [0]
            tags = []
        elif len (pieces) == 2:
            main = pieces [0]
            tags = pieces [1]
        else:
            print "Warning: something fishy splitting", embedded
 
        
        for l in main.split ("\n\t"):
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

        if len (tags) > 0:
            grouped_lines += self.__handle_tags (tags)


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

        IGNORE hasTag
        """
        hasEscapedComma = re.compile ("\".+,.+\"")

        if "," in line and not hasEscapedComma.search (line):
            prop, multival = line.split (" ", 1)
            results = []
            for value in multival.split (","):
                results.append ("%s %s" % (prop, value.strip ()))
            return results
        else:
            if "nao:hasTag" in line:
                return []
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
        
        # location case
        if line.startswith ("slo:location"):
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


    def __handle_tags (self, sparql):
        results = []
        gettag = re.compile ('a nao:Tag\ ;\ nao:prefLabel "([\w:-]+)"')
        for l in sparql.split ("\n"):
            if len (l) < 1:
                continue
            tag_match = gettag.search (l)
            if (tag_match):
                results.append ("nao:hasTag:prefLabel %s ;" % tag_match.group (1))
            else:
                print " *** Something special in this tag line '%s'" % (l)
            
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
        
