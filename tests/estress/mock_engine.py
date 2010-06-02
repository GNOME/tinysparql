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

import gobject
import getopt, sys
import options

options = options.parse_options (graphic_mode=False, period=1, msgsize=2, timeout=0)

if options['graphic_mode']:
    import gtk
    from abstract_engine import AbstractEngine
    superKlass = AbstractEngine
    mainloop = gtk.main
else:
    from abstract_text_engine import AbstractTextEngine
    superKlass = AbstractTextEngine
    mainloop = gobject.MainLoop().run

class MockEngine (superKlass):

    def __init__ (self, name, period, msgsize, timeout):
        """
        self.publicname
        self.msgsize
        self.period contains these values for the subclasses
        """
        superKlass.__init__ (self, name, period, msgsize, timeout)
        self.run ()
        
    def get_insert_sparql (self):
        """
        This method returns an string with the sparQL we will send
        to tracker.SparqlUpdate method
        """
        return "INSERT { put here your triplets }"

    def get_running_label (self):
        """
        This method returns the string showing the current status
        when the engine is running
        """
        return "%s sends %s items every %s sec." % (self.publicname,
                                                          self.msgsize,
                                                          self.period)


if __name__ == "__main__":

    gobject.set_application_name ("Here the title of the window")
    engine = MockEngine ("My mock stuff",
                         options['period'],
                         options['msgsize'],
                         options['timeout'])
    mainloop ()

