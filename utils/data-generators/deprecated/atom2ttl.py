#!/usr/bin/env python2.5
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

import feedparser
import re, sys
from internals.tools import print_namespaces, print_property

def get_self_link (d):
    rel_link = filter (lambda e: "rel" in e and e["rel"] == "self", d.feed.links)
    if (rel_link and len (rel_link)):
        return rel_link[0]["href"]
    raise Exception ("Invalid feed channel (no 'self' link")


def parse_author_contact (author_line):
    """ Ivan Frade <ivan.frade@nokia.com> --> ('Ivan Frade', 'ivan.frade@nokia.com')
    """
    if (not author_line or author_line == ''):
        return (author_line, '')
    
    AUTHOR_PATTERN = "(.*)<(\S+@(\w+\.)+\w+)>"
    m = re.search (AUTHOR_PATTERN, author_line)
    if (m):
        return (m.group(1).strip(), m.group(2).strip())
    else:
        AUTHOR_PATTERN_2 = "(.*)\((\S+@(\w+\.)+\w+)\)"
        m = re.search (AUTHOR_PATTERN_2, author_line)
        if (m):
            return (m.group(1).strip(), m.group(2).strip())
        else:
            print >> sys.stderr, "Assuming '%s' as author name" % (author_line)
            return (author_line, '')
            
def process_atom_feed (d):
    atom_uri =  get_self_link (d)
    print '<%s> a nmo:FeedChannel;' % atom_uri
    print_property ("nie:title", d.feed.title)
    print_property ("nie:description", d.feed.get('subtitle', None))
    print_property ("nie:contentLastModified", d.feed.updated.replace('Z', "+00:00"), final=True)
    print ""
    
    for entry in d.entries:
        process_atom_entries (entry, atom_uri)

def process_atom_entries (entry, atom_uri):
    print '<%s> a nmo:FeedMessage;' % entry.id
    print_property ("nie:title", entry.title)
    date =  entry.updated or entry.published
    if (date):
        print_property ("nie:contentLastModified", date.replace('Z', "+00:00"))
    print_property ("nmo:communicationChannel", atom_uri, "uri")
    #print_property ("nmo:htmlMessageContent", entry.summary.encode ('utf8'))
    author, email = parse_author_contact (entry.get('author', None))
    if (email and not email == ''):
        print '\tnco:contributor [a nco:Contact; nco:fullname "%s"; nco:hasEmailAddress <mailto:%s>];' % (author.encode ('utf8'),
                                                                                                           email.encode ('utf8')) 
        print ""
    else:
        if (author):
            print '\tnco:contributor [a nco:Contact; nco:fullname "%s"];' % (author.encode ('utf8'))
            print ""
        
    print_property ("nie:links", entry.link, "uri", final=True)

if __name__ == "__main__":

    if (len(sys.argv) < 2):
        print "USAGE: %s ATOM_FEED"
    
    PLANET_MAEMO_ATOM = "http://maemo.org/news/planet-maemo/atom.xml"
    PLANET_MAEMO_ATOM_LOCAL = "./planet-maemo.atom"
    PLANET_GNOME_ATOM_LOCAL = "./planet-gnome.atom"
    #sys.stderr = open ('planetmaemo2ttl.log', 'w')
    d = feedparser.parse (sys.argv[1])
    print_namespaces ()
    process_atom_feed (d)
    
