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

from common.utils import configuration as cfg
from common.utils.helpers import log
import os
import re
import subprocess


class ExtractorParser(object):
    def parse_tracker_extract_output(self, text):
        """
        Parse stdout of `tracker-extract --file` to get SPARQL statements.

        Calls the extractor a returns a dictionary of property, value.

        Example:
         { 'nie:filename': 'a.jpeg' ,
           'tracker:added': '2008-12-12T12:23:34Z'
         }
        """

        metadata = {}
        parts = self.get_statements_from_stdout_output(text)
        extras = self.__process_where_part(parts['where'])
        for attribute_value in self.__process_lines(parts['item']):
            att, value = attribute_value.split(" ", 1)
            if value.startswith("?") and extras.has_key(value):
                value = extras[value]

            if metadata.has_key(att):
                metadata [att].append(value)
            else:
                metadata [att] = [value]

        return metadata

    def get_statements_from_stdout_output(self, text):
        lines = text.split('\n')
        parts = {}

        current_part = None
        part_start = None

        i = 0
        for i in range(0, len(lines)):
            if lines[i] == 'SPARQL pre-update:':
                current_part = 'preupdate'
            elif lines[i] == 'SPARQL item:':
                current_part = 'item'
            elif lines[i] == 'SPARQL where clause:':
                current_part = 'where'
            elif lines[i] == 'SPARQL post-update:':
                current_part = 'postupdate'

            if lines[i] == '--':
                if part_start is None:
                    part_start = i + 1
                else:
                    part_lines = lines[part_start:i]
                    parts[current_part] = '\n'.join(part_lines)
                    current_part = None
                    part_start = None

        if current_part is not None:
            raise Exception("End of text while parsing %s in tracker-extract "
                            "output" % current_part)

        if len(parts) == 0:
            raise Exception("No metadata was found by tracker-extract")

        return parts

    def __process_lines(self, embedded):
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

    def __process_where_part(self, where):
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

    def __handle_multivalues(self, line):
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
       
    def __handle_anon_nodes(self, line):
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

    def __handle_playlist_entries(self, line):
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

    def __clean_value(self, value):
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


def get_tracker_extract_output(filename):
    """
    Runs `tracker-extract --file` to extract metadata from a file.
    """

    tracker_extract = os.path.join (cfg.EXEC_PREFIX, 'tracker-extract')
    command = [tracker_extract, '--file', filename]

    try:
        log ('Running: %s' % ' '.join(command))
        output = subprocess.check_output (command)
    except subprocess.CalledProcessError as e:
        raise Exception("Error %i from tracker-extract, output: %s" %
                        (e.returncode, e.output))

    parser = ExtractorParser()
    return parser.parse_tracker_extract_output(output)
