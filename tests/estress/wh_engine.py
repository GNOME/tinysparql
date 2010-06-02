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
import datetime, random
import options, sys

options = options.parse_options (graphic_mode=False, period=2, msgsize=1, timeout=0)

if options['graphic_mode']:
    import gtk
    from abstract_engine import AbstractEngine
    superKlass = AbstractEngine
    mainloop = gtk.main
else:
    from abstract_text_engine import AbstractTextEngine
    superKlass = AbstractTextEngine
    mainloop = gobject.MainLoop().run


class WebBrowserEngine (superKlass):

    def __init__ (self, name, period, msgsize, timeout):
        superKlass.__init__ (self, name, period, msgsize, timeout)
        self.run ()
        
    def get_insert_sparql (self):
        sparql = ""
        for i in range (0, self.msgsize):
            sparql += self.get_random_webhistory_click ()
        return "INSERT {" + sparql + "}"

    def get_running_label (self):
        """
        This method returns the string showing the current status
        when the engine is running
        """
        return "%s insert %s clicks every %s sec." % (self.publicname,
                                                      self.msgsize,
                                                      self.period)

    def get_random_webhistory_click (self):
        TEMPLATE = """<%s> a nfo:WebHistory;
            nie:title "This is one random title";
            nie:contentCreated "%s";
            nfo:domain "%s";
            nfo:uri "%s".
        """
        today = datetime.datetime.today ()
        date = today.isoformat () + "+00:00"
        click_no = str(random.randint (100, 1000000))
        resource = "urn:uuid:1234" + click_no
        uri = "http://www.maemo.org/" + click_no
        domain = "http://www.maemo.org"
        return TEMPLATE % (resource, date, domain, uri)

if __name__ == "__main__":

    gobject.set_application_name ("Web Browser saving clicks (inefficiently)")
    engine = WebBrowserEngine ("Naughty Web Browser",
                               options['period'],
                               options['msgsize'],
                               options['timeout'])
    mainloop ()

