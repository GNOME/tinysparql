Title: Overview

Tracker SPARQL allows creating and connecting to one or more
triplestore databases. It is used by the
[Tracker Miners filesystem indexer](https://gitlab.gnome.org/GNOME/tracker-miners/),
and can also store and publish any kind of app data.

Querying data is done using the SPARQL graph query language. See the
[examples](examples.html) to find out how this works.
Storing data can also be done using SPARQL, or using the [class@Tracker.Resource]
API.

You can share a database over D-Bus using the [class@Tracker.Endpoint] API,
allowing other libtracker-sparql users to query from it, either
by referencing it in a `SELECT { SERVICE ... }` query, or by connecting
directly with [ctor@Tracker.SparqlConnection.bus_new].

Tracker SPARQL partitions the database into multiple graphs.
You can implementing access control restrictions based on
graphs, and we provide a Flatpak portal that does so.
The number of graphs is [limited](limits.html).

## Connection methods

You can create and access a private store using
[ctor@Tracker.SparqlConnection.new]. This is useful to store
app-specific data.

To connect to another database on the same local machine, such as the
one exposed by Tracker Miner FS, use [ctor@Tracker.SparqlConnection.bus_new].

To connect to another a database on a remote machine, use
[ctor@Tracker.SparqlConnection.remote_new]. This can be used to query online
databases that provide a SPARQL endpoint, such as [DBpedia](https://wiki.dbpedia.org/about).
