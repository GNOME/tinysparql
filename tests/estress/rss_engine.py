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
import sys
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

class RSSEngine (superKlass):

    def __init__ (self, name, period, msgsize, timeout=0):
        superKlass.__init__ (self, name, period, msgsize, timeout)
        self.run ()
        
    def get_insert_sparql (self):
        triplets = ""
        for i in range (0, self.msgsize):
            triplets += self.gen_new_post ()

        query = "INSERT {" + triplets + "}"
        return query

    def get_running_label (self):
        return "%s sends %s rss entries every %s sec." % (self.publicname,
                                                          self.msgsize,
                                                          self.period)

    def gen_new_post (self):
        SINGLE_POST = """
        <%s> a nmo:FeedMessage ;
        nie:contentLastModified "%s" ;
        nmo:communicationChannel <http://maemo.org/news/planet-maemo/atom.xml>;
        nie:title "%s".
        """
        today = datetime.datetime.today ()
        date = today.isoformat () + "+00:00"
        post_no = str(random.randint (100, 1000000))
        uri = "http://test.maemo.org/feed/" + post_no
        title = "Title %s" % (post_no)
        
        return SINGLE_POST % (uri, date, title)
        

if __name__ == "__main__":

    gobject.set_application_name ("Feeds engine/signals simulator")
    engine = RSSEngine ("RSS",
                        options['period'],
                        options['msgsize'],
                        options['timeout'])
    mainloop ()

