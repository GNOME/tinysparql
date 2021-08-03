---
title: Overview
short-description: Library Overview
...

# Overview

Tracker SPARQL allows creating and connecting to one or more
triplestore databases. It is used by the
[Tracker Miners filesystem indexer](https://gitlab.gnome.org/GNOME/tracker-miners/),
and can also store and publish any kind of app data.

Querying data is done using the SPARQL graph query language. See the
[examples](examples.html) to find out how this works.
Storing data can also be done using SPARQL, or using the [](TrackerResource)
API.

You can share a database over D-Bus using the [](TrackerEndpoint) API,
allowing other libtracker-sparql users to query from it, either
by referencing it in a `SELECT { SERVICE ... }` query, or by connecting
directly with [](tracker_sparql_connection_bus_new).

Tracker SPARQL partitions the database into multiple graphs.
You can implementing access control restrictions based on
graphs, and we provide a Flatpak portal that does so.
The number of graphs is [limited](limits.html).

## Connection methods

You can create and access a private store using
[](tracker_sparql_connection_new). This is useful to store
app-specific data.

To connect to another database on the same local machine, such as the
one exposed by Tracker Miner FS, use [](tracker_sparql_connection_bus_new).

To connect to another a database on a remote machine, use
[](tracker_sparql_connection_remote_new). This can be used to query online
databases that provide a SPARQL endpoint, such as [DBpedia](https://wiki.dbpedia.org/about).
        .
## Connecting from Flatpak

Tracker SPARQL provides a portal for the [Flatpak](https://flatpak.org/)
application sandboxing system. This lets you control which parts of a
database each app can query.

The app's Flatpak manifest needs to specify which graph(s) the app will
access. See the [example app](https://gitlab.gnome.org/GNOME/tracker/-/blob/master/examples/flatpak/org.example.TrackerSandbox.json)
and the [portal documentation](https://gnome.pages.gitlab.gnome.org/tracker/docs/commandline/#tracker-xdg-portal-3) to see how.

No code changes are needed in the app, as [](tracker_sparql_connection_bus_new)
will automatically try connect via the portal if it can't talk to the
given D-Bus name directly.

Tracker SPARQL is included in the
[GNOME Flatpak SDK and runtime](https://docs.flatpak.org/en/latest/available-runtimes.html#gnome).
If the app uses a different runtime, you may need to build Tracker
SPARQL in the app manifest.

The app might use the Tracker Miner FS search index. Ideally this is done via the portal,
using the search index maintained on the host. Since not all OSes ship tracker-miner-fs-3,
we recommend that apps bundle it inside the Flatpak and connect to the bundled version as
a fallback. See [GNOME Music](https://gitlab.gnome.org/GNOME/gnome-music/)
for an example of how to do this.
