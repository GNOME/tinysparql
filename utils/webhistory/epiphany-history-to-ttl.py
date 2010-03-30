#
# Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

import xml.dom.minidom
from xml.dom.minidom import Node
import time
import sys, os

PROPERTIES = {2: ("nie:title", str),
              3: ("nfo:uri", str),
              4: ("nie:usageCounter", int),
              6: ("nie:lastRefreshed", time.struct_time)}
# Use time.struct_time as type for dates, even when the format is not that!

def get_text (node):
    text = ""
    for subnode in node.childNodes:
      if subnode.nodeType == Node.TEXT_NODE:
        text += subnode.data
    return text.encode ('utf8').replace ('"', '') # Use a safer method!

def process_file (filename):
    doc = xml.dom.minidom.parse(filename)
    
    for node in doc.getElementsByTagName ("node"):
        print "<uri:uuid:epiphany-webhistory-%s> a nfo:WebHistory" % (node.getAttribute ("id")),
        
        for prop in node.getElementsByTagName ("property"):
            prop_id = int(prop.getAttribute ("id"))

            if (PROPERTIES.has_key (prop_id)):
                prop_name, prop_type = PROPERTIES [prop_id]

                if (prop_type == str):
                    print  ';\n\t%s "%s"' % (prop_name, get_text (prop)),

                elif (prop_type == int):
                    print ';\n\t%s %s' % (prop_name, get_text (prop)),

                elif (prop_type == time.struct_time):
                    print ';\n\t%s "%s"' % (prop_name, time.strftime ("%Y%m%dT%H:%m:%S",time.localtime (int(get_text (prop))))),
        print ".\n"
        

def print_headers ():
    print "@prefix nfo: <http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>."
    print "@prefix nie: <http://www.semanticdesktop.org/ontologies/2007/01/19/nie#> ."

if __name__ == "__main__":

    epi_history = os.path.join (os.getenv ("HOME"), ".gnome2", "epiphany", "ephy-history.xml")
    print >> sys.stderr, "Scanning", epi_history

    print_headers ()
    if (os.path.exists (epi_history)):
        process_file (epi_history)
