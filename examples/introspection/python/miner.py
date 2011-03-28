#!/usr/bin/env python
import gi
from gi.repository import TrackerMiner, GLib, GObject, Gio


class MyMiner (TrackerMiner.Miner):
    __gtype_name__ = 'MyMiner'

    def __init__ (self):
        TrackerMiner.Miner.__init__ (self,
                                     name="MyMiner",
                                     progress=0,
                                     status="fine")
        # This shouldn't be needed, but at the moment the
        # overrided methods are not called
        self.connect ("started", self.started_cb)

        # Say to initable that we are ok
        self.init (None)

    def started (self, x):
        print "override started"

    def started_cb (self, x):
        print "started as callback"

    def stopped (self):
        print "override stopped"

    def resumed (self):
        print "override resumed"

    def paused (self):
        print "override paused"

    def progress (self):
        print "override progress"

    def ignore_next_update (self):
        print "override ignore next updated"


if __name__ == "__main__":
    m = MyMiner ()
    m.start ()

    GObject.MainLoop().run ()
