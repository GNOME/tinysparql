#
# GtkSparql - Gtk UI to try SparQL queries against tracker.
# Copyright (C) 2009, Ivan Frade <ivan.frade@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

import dbus
import re

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

ALL_NAMESPACES = """
SELECT ?u ?prefix WHERE { ?u a tracker:Namespace; tracker:prefix ?prefix. }
"""

ALL_PROPERTIES = """
SELECT ?u ?comment
WHERE { ?u a rdf:Property. OPTIONAL { ?u rdfs:comment ?comment.} }
"""

ALL_CLASSES = """
SELECT ?u ?comment WHERE { ?u a rdfs:Class. OPTIONAL { ?u rdfs:comment ?comment.} }
"""

class TrackerCompletionEngine:

    def __init__ (self):
        self.connect_tracker ()

        self.namespaces = {}
        for row in self.resources.SparqlQuery (ALL_NAMESPACES):
            self.namespaces [row[0]] = row[1]

        self.properties = {}
        for (uri, comment) in self.resources.SparqlQuery (ALL_PROPERTIES):
            self.properties [str(self.qname_to_short (uri))] = str(comment)

        self.classes = {}
        for (uri, comment) in self.resources.SparqlQuery (ALL_CLASSES):
            self.classes [str(self.qname_to_short (uri))] = str(comment)

    def qname_to_short (self, uri):
        ns, classname = self.split_ns_uri (uri)
        prefix = self.namespaces [ns]
        return prefix + ":" + classname

    def split_ns_uri (self, uri):
        pieces = uri.split ("#")
        if ( len (pieces) > 1):
            classname = pieces [1]
            namespace = pieces [0] + "#"
        else:
            classname = uri[uri.rindex ('/')+1:]
            namespace = uri[:uri.rindex ('/')+1]
        return namespace, classname


    def connect_tracker (self):
        # TODO Check if the connection is valid
        bus = dbus.SessionBus ()
        tracker = bus.get_object (TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE);

    def complete_word (self, textbuffer):
        """
        Return a list of dictionaries with the following keys:
          completion: text to set in the buffer if selected the option
          abbr: text visible for the user in the list of options
          info: test description showed near to the option
        """
        completions = []
        uncomplete_word = self.get_last_word (textbuffer)
        for k in self.classes.iterkeys ():
            if (k.startswith (uncomplete_word)):
                c = {'completion':k[len (uncomplete_word):],
                     'abbr':k,
                     'info':self.classes [k]}
                completions.insert (0, c)
                
        for k in self.properties.iterkeys ():
            if (k.startswith (uncomplete_word)):
                c = {'completion':k[len (uncomplete_word):],
                     'abbr':k,
                     'info':self.properties [k]}
                completions.insert (0, c)

        return completions
        

    def get_last_word (self, textbuffer):
        insert = textbuffer.get_iter_at_mark(textbuffer.get_insert())
        lineno = insert.get_line ()
        linestart = textbuffer.get_iter_at_line (lineno)
        regex = re.compile (r"\S+$", re.MULTILINE)
        line = textbuffer.get_slice (linestart, insert, False)
        match = re.search (regex, line)
        if (not match):
            return []
        uncomplete_word = match.group (0)
        return uncomplete_word

if __name__ == "__main__":

    import gtk
    engine = TrackerCompletionEngine ()
    buf = gtk.TextBuffer ()
    buf.set_text ("SELECT ?u WHERE { ?u a nie:InformationElement;\n nie:t")
    print "Last word: ", engine.get_last_word (buf)
    print engine.complete_word (buf)
