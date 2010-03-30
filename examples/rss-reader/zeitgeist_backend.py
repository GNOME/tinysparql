#
# Demo RSS client using tracker as backend
# Copyright (C) 2009 Nokia <ivan.frade@nokia.com>
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
import os
import zeitgeist
import time

import zeitgeist.dbusutils
from zeitgeist.datamodel import Manifestation, Interpretation, Event, Subject

APP_ID = u"/usr/local/share/tracker/examples/rss-python/rss-pseudo-miner.desktop"

class ZeitgeistBackend:

    def __init__ (self):
        self.iface = zeitgeist.dbusutils.DBusInterface ()

    def view_event (self, uri):
        ev = Event.new_for_values(timestamp=int(time.time()),
                                  interpretation=Interpretation.VISIT_EVENT.uri,
                                  manifestation=Manifestation.USER_ACTIVITY.uri,
                                  actor=APP_ID)
        subj = Subject.new_for_values(uri=uri,
                                      interpretation=Interpretation.DOCUMENT.uri,
                                      manifestation=Manifestation.FILE.uri)
        ev.append_subject(subj)

        self.iface.InsertEvents ([ev])
        print "VISIT event for <%s> " % (uri)


    def get_rss_by_usage (self):
        print "Events: ",
        event_template = Event.new_for_values(
            actor=APP_ID,
            interpretation=Interpretation.VISIT_EVENT.uri)

        results = []
        
        ids = self.iface.FindEventIds((0,0),
                                      [event_template],
                                      0, 0, 5)
        for event in self.iface.GetEvents (ids):
            for subject in Event(event).subjects:
                 results.append (str(subject.uri))
        return results

        
    def clean_events (self):
        event_template = Event.new_for_values(
            actor=APP_ID,
            interpretation=Interpretation.VISIT_EVENT.uri)
        ids = self.iface.FindEventIds((0,0),
                                 [event_template],
                                 0, 10, 0)
        print "Deleting", map (int, ids)
        self.iface.DeleteEvents (ids)
        

if __name__ == "__main__":
    zb = ZeitgeistBackend ()
    # 1 -> 1 occurrences
    # 2 -> 2 occurrence
    # 3 -> 3 occurrences
    zb.view_event ("rss://feed/3")
    zb.view_event ("rss://feed/2")
    time.sleep (1)
    zb.view_event ("rss://feed/1")
    zb.view_event ("rss://feed/3")
    time.sleep (1)
    zb.view_event ("rss://feed/2")
    zb.view_event ("rss://feed/3")

    zb.get_rss_by_usage ()
    zb.clean_events ()
