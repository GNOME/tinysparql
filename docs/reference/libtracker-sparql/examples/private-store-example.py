#!/usr/bin/python3

import gi, sys
from gi.repository import GLib, Gio, Tracker

try:
    connection = Tracker.SparqlConnection.new(
        Tracker.SparqlConnectionFlags.NONE,
        None, # Database location, None creates it in-memory
        Tracker.sparql_get_ontology_nepomuk(), # Ontology location
        None)

    # Create a resource containing RDF data
    resource = Tracker.Resource.new(None)
    resource.set_uri('rdf:type', 'nmm:MusicPiece')

    # Create a batch, and add the resource to it
    batch = connection.create_batch()
    batch.add_resource(None, resource)

    # Execute the batch to insert the data
    batch.execute()

    connection.close()

except Exception as e:
    print('Error: {0}'.format(e))
    sys.exit(-1)
