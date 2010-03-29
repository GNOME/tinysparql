#!/usr/bin/env python
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

import random, sys
from internals.tools import print_namespaces, print_property
from internals.tools import getPseudoRandomDate, get_random_uuid_uri

DOMAINS = ["http://www.google.com",
           "http://www.nokia.com",
           "http://www.maemo.com",
           "http://slashdot.org",
           "http://www.youtube.com",
           "http://www.piratebay.org",
           "http://python.org"]

def get_random_url_in_domain (domain):
    return str.join('/', [domain, str(random.randint(0,100000))])

def gen_webhistory_ttl (entries):

    print_namespaces ()

    for i in range (0, entries):
        domain = DOMAINS [random.randint (0, len (DOMAINS)-1)]
        
        print "<%s> a nfo:WebHistory;" % (get_random_uuid_uri ())
        print_property ("nie:title", "%s %s" % (domain[7:], str(i)))
        print_property ("nie:contentCreated", getPseudoRandomDate ())
        print_property ("nfo:domain", domain)
        print_property ("nfo:uri", get_random_url_in_domain (domain), final=True)
        

if __name__ == "__main__":

    if (len(sys.argv) < 2):
        print "Usage: %s NO_ENTRIES (it must be an integer > 0" % (__name__)
        sys.exit(-1)

    try:
        entries = int (sys.argv[1])
    except ValueError:
        print "Usage: %s NO_ENTRIES (it must be an integer > 0)" % (__name__)
        sys.exit (-1)
        
    if (entries < 0):
        print >> sys.stderr, "Entries must be > 0"
        sys.exit(-1)
    
    gen_webhistory_ttl (entries)

