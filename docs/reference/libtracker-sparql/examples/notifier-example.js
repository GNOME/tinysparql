#!/usr/bin/gjs

const { GLib, Gio, Tsparql } = imports.gi

try {
    let connection = Tsparql.SparqlConnection.bus_new(
        'org.freedesktop.Tracker3.Miner.Files',
        null, null);

    let notifier = connection.create_notifier();
    notifier.connect('events', (service, graph, events) => {
        for (let event in events)
            print (`Event ${event.get_event_type()} on ${event.get_urn()}`);
    });

    let loop = GLib.MainLoop.new(null, false);
    loop.run();

    connection.close();
} catch (e) {
    printerr(`Error: ${e.message}`)
}
