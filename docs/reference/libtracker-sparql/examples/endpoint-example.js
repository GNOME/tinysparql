#!/usr/bin/gjs

const { GLib, Gio, Tsparql } = imports.gi

try {
    let connection = Tsparql.SparqlConnection.new(
        Tsparql.SparqlConnectionFlags.NONE,
        null, // Database location, None creates it in-memory
        Tsparql.sparql_get_ontology_nepomuk(), // Ontology location
        null);

    let bus = Gio.bus_get_sync(Gio.BusType.SESSION, null)

    let endpoint = Tsparql.EndpointDBus.new(
        connection, bus, null, null);

    let loop = GLib.MainLoop.new(null, false);
    loop.run();

    connection.close();
} catch (e) {
    printerr(`Error: ${e.message}`)
}
