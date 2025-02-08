#!/usr/bin/python3

import gi, sys
from gi.repository import GLib, Gio, Tsparql

def callback(service, graph, events):
    for event in events:
        print('Event {0} on {1}\n'.format(
            event.get_event_type(), event.get_urn()))

try:
    connection = Tsparql.SparqlConnection.bus_new(
        'org.freedesktop.Tracker3.Miner.Files',
        None, None)

    notifier = connection.create_notifier()
    notifier.connect('events', callback)

    loop = GLib.MainLoop.new(None, False)
    loop.run()

    connection.close()

except Exception as e:
    print('Error: {0}'.format(e))
    sys.exit(-1)
