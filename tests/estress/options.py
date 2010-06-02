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

import getopt
import sys

def usage ():
    print "Usage:"
    print "  xxx.py [OPTION...] - Input data into tracker"
    print ""
    print "Help Options:"
    print "  -h, --help             Show help options"
    print ""
    print "Application Options:"
    print "  -g, --graphics         Enable GTK interface"
    print "  -p, --period=NUM       Time (in sec) between insertion message"
    print "  -s, --size=NUM         Amount of instances in the message"
    print "  -t, --timeout=NUM      Switch off the program after NUM seconds"
    print ""


def parse_options (graphic_mode=False, period=1, msgsize=1, timeout=0):
    try:
        opts, args = getopt.getopt(sys.argv[1:],
                                   "gp:s:t:h",
                                   ["graphics", "period", "size", "timeout", "help"])
    except getopt.GetoptError, err:
        # print help information and exit:
        print str (err) # will print something like "option -a not recognized"
        usage ()
        sys.exit (2)

    options = { "graphic_mode" : graphic_mode ,
                "period"  : period,
                "msgsize" : msgsize,
                "timeout" : timeout }


    for o, a in opts:
        if o in ["-g", "--graphics"]:
            options ['graphic_mode'] = True
            
        elif o in ["-p", "--period"]:
            options ['period'] = int(a)
            
        elif o in ["-s", "--size"]:
            options ['msgsize'] = int (a)
            
        elif o in ["-t", "--timeout"]:
            options ['timeout'] = int (a)
        elif o in ["-h", "--help"]:
            usage ()
            sys.exit (-1)
        else:
            usage ()
            assert False, "unhandled option"

    return options

