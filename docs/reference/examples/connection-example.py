#!/usr/bin/python3

import gi, sys
from gi.repository import GLib, Gio, Tsparql

try:
    connection = Tsparql.SparqlConnection.bus_new(
        'org.freedesktop.Tracker3.Miner.Files',
        None, None)

    stmt = connection.query_statement (
        'SELECT DISTINCT nie:url(?u) WHERE { ' +
        '  ?u a nfo:FileDataObject ; ' +
        '     nfo:fileName ~name ' +
        '}', None)

    stmt.bind_string('name', sys.argv[1])

    cursor = stmt.execute()
    i = 0;

    while cursor.next():
        i += 1
        print('Result {0}: {1}'.format(i, cursor.get_string(0)[0]))

    print('A total of {0} results were found\n'.format(i))

    cursor.close()
    connection.close()

except Exception as e:
    print('Error: {0}'.format(e))
    sys.exit(-1)
