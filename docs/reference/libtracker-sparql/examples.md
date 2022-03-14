---
title: Examples
short-description: Examples
...

# Examples

This chapters shows some real examples of usage of the Tracker
SPARQL Library.

## Querying a remote endpoint

All SPARQL queries happen on a [](TrackerSparqlConnection), often these
connections represent a remote endpoints maintained by another process or
server.

This example demonstrates the use of these connections on a remote
endpoint. Concretely creating a D-Bus [](TrackerSparqlConnection),
creating a prepared statement from a SPARQL query string, executing
the query, and obtaining the query results from the cursor.

The [](tracker_sparql_connection_query_statement) function can be used
to obtain a [](TrackerSparqlStatement) object holding a prepared SPARQL
query that can then be executed with [](tracker_sparql_statement_execute).
The query string can contain `~name` placeholders which can be replaced with
arbitrary values before query execution with
[](tracker_sparql_statement_bind_string) and similar functions.
This allows parsing the query string only once and to execute it multiple
times with different parameters with potentially significant performance gains.

Multiple functions offer asynchronous variants, so the application
main loop is not blocked while these operations are executed.

Once you end up with the query, remember to call [](tracker_sparql_cursor_close).
The same applies to [](tracker_sparql_connection_close) when no longer needed.

<div class="gi-lang-c">

{{ examples/connection-example.c }}

</div>
<div class="gi-lang-python">

{{ examples/connection-example.py }}

</div>
<div class="gi-lang-javascript">

{{ examples/connection-example.js }}

</div>

## Creating a private database

Applications may create private stores via the [](tracker_sparql_connection_new)
constructor.

This example demonstrates the creation of a private store, for simplicity the
example uses the builtin Nepomuk ontology, but the data structures may be defined
by the application, see the documentation on
[defining ontologies](defining-ontologies.md) for more information about this.

The example also demonstrates the use of [](TrackerResource) and [](TrackerBatch)
for insertion of RDF data. It is also possible the direct use of SPARQL update
strings via [](tracker_sparql_connection_update).

Multiple functions offer asynchronous variants, so the application
main loop is not blocked while these operations are executed.

Once you no longer need the connection, remember to call
[](tracker_sparql_connection_close) on the [](TrackerSparqlConnection).

<div class="gi-lang-c">

{{ examples/private-store-example.c }}

</div>
<div class="gi-lang-python">

{{ examples/private-store-example.py }}

</div>
<div class="gi-lang-javascript">

{{ examples/private-store-example.js }}

</div>

## Creating a SPARQL endpoint

For some applications and services, it might be desirable to export a
SPARQL store as an endpoint. Making it possible for other applications to
query the data they hold.

This example demonstrates the use of [](TrackerEndpoint) subclasses,
concretely the creation of a D-Bus endpoint, that other applications
may query e.g. through a connection created with
[](tracker_sparql_connection_bus_new).

<div class="gi-lang-c">

{{ examples/endpoint-example.c }}

</div>
<div class="gi-lang-python">

{{ examples/endpoint-example.py }}

</div>
<div class="gi-lang-javascript">

{{ examples/endpoint-example.js }}

</div>

## Receiving notification on changes

As an additional feature over SPARQL endpoints, Tracker allows for
users of private and D-Bus SPARQL connections to receive notifications
on changes of certain RDF classes (Those with the
[nrl:notify](nrl-ontology.md#nrl:notify) property, like
[nmm:MusicPiece](nmm-ontology.md#nmm:MusicPiece)).

This example demonstrates the use of [](TrackerNotifier) to receive
notifications on database updates.

<div class="gi-lang-c">

{{ examples/notifier-example.c }}

</div>
<div class="gi-lang-python">

{{ examples/notifier-example.py }}

</div>
<div class="gi-lang-javascript">

{{ examples/notifier-example.js }}

</div>
