#!/usr/bin/gjs

const { GLib, Gio, Tsparql } = imports.gi

try {
    let connection = Tsparql.SparqlConnection.new(
        Tsparql.SparqlConnectionFlags.NONE,
        null, // Database location, None creates it in-memory
        Tsparql.sparql_get_ontology_nepomuk(), // Ontology location
        null);

    // Create a resource containing RDF data
    let resource = Tsparql.Resource.new(null)
    resource.set_uri('rdf:type', 'nmm:MusicPiece')

    // Create a batch, and add the resource to it
    let batch = connection.create_batch()
    batch.add_resource(null, resource)

    // Execute the batch to insert the data
    batch.execute(null)

    connection.close();
} catch (e) {
    printerr(`Error: ${e.message}`)
}
