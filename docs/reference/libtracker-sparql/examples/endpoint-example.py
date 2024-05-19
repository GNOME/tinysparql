#!/usr/bin/python3

import gi, sys
from gi.repository import GLib, Gio, Tsparql

try:
    connection = Tsparql.SparqlConnection.new(
        Tsparql.SparqlConnectionFlags.NONE,
        None, # Database location, None creates it in-memory
        Tsparql.sparql_get_ontology_nepomuk(), # Ontology location
        None)

    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)

    endpoint = Tsparql.EndpointDBus.new(
        connection, bus, None, None)

    loop = GLib.MainLoop.new(None, False)
    loop.run()

    connection.close()

except Exception as e:
    print('Error: {0}'.format(e))
    sys.exit(-1)
