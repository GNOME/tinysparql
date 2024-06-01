#!/usr/bin/gjs

const { GLib, Gio, Tsparql } = imports.gi

try {
    let connection = Tsparql.SparqlConnection.bus_new(
        'org.freedesktop.Tracker3.Miner.Files',
        null, null);

    let stmt = connection.query_statement (
        'SELECT DISTINCT nie:url(?u) WHERE { ' +
        '  ?u a nfo:FileDataObject ; ' +
        '     nfo:fileName ~name ' +
        '}', null);

    stmt.bind_string('name', ARGV[0]);

    let cursor = stmt.execute(null);
    let i = 0;

    while (cursor.next(null)) {
        i++;
        print(`Result ${i}: ${cursor.get_string(0)[0]}`);
    }

    print(`A total of ${i} results were found`);

    cursor.close();
    connection.close();
} catch (e) {
    printerr(`Error: ${e.message}`)
}
