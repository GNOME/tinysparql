#!/usr/bin/gjs

const { GLib, Gio, Tracker } = imports.gi

try {
    let connection = Tracker.SparqlConnection.new(
        Tracker.SparqlConnectionFlags.NONE,
        null, // Database location, None creates it in-memory
        Tracker.sparql_get_ontology_nepomuk(), // Ontology location
        null);

    let bus = Gio.bus_get_sync(Gio.BusType.SESSION, null)

    let endpoint = Tracker.EndpointDBus.new(
        connection, bus, null, null);

    let loop = GLib.MainLoop.new(null, false);
    loop.run();

    connection.close();
} catch (e) {
    printerr(`Error: ${e.message}`)
}
