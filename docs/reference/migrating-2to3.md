Title: Migrating from 2.x to 3.0
Slug: migrating-2-to-3

Tracker 3.0 is a new major version, containing some large
syntax and conceptual changes.

## Graph semantics

One of the big features in 3.0 is full SPARQL 1.1 syntax
support. Besides the missing syntax additions, one conceptually
big change is the handling of graphs in the database.

In 2.x, there was a minimum concept of graphs, but it didn't
represent what is defined in the standard. You could attribute
a graph to a data triple, but a given triple could only reside
in one graph at a time. In other words, this yields the wrong
result:

```SPARQL
INSERT { GRAPH <http://example.com/A> { <foo> nie:title 'Hello' } }
INSERT { GRAPH <http://example.com/B> { <foo> nie:title 'Hola' } }

# We expect 2 rows, 2.x returns 1.
SELECT ?g ?t { GRAPH ?g { <foo> nie:title ?t } }
```

3.0 implements the graph semantics as defined in the SPARQL 1.1
documents. So the SELECT query would return both graphs.

3.0 also honors properly the concept of «Unnamed graph». This
graph is always used whenever no graph is specified, and always
skipped if a GRAPH is requested or defined, e.g.:

```SPARQL
# Inserts element into the unnamed graph
INSERT { <http://example.com/foo> a nfo:FileDataObject }

# Inserts element into named graph A
INSERT { GRAPH <A> { <http://example.com/bar> a nfo:FileDataObject } }

# Queries from all named graphs, A in this case
SELECT ?g ?s { GRAPH ?g { ?s a nfo:FileDataObject } }

# Queries the default graph, which includes the unnamed graph
SELECT ?s { ?s a nfo:FileDataObject }
```

3.0 defines the default graph to be the union of the unnamed
graph plus all known named graphs. So the last query in the
example above would return results from both the unnamed graph
and graph A. This behavior can be influenced with FROM/FROM NAMED
syntax (also newly handled in 3.0)

In contrast, 2.x does not distinguish between named and unnamed
graphs. The first SELECT query would return a row for the unnamed
graph, with ?g being NULL.

## No libtracker-control

This library offered fully generic control method to Tracker
miners. This genericity is not widely useful, so the feature is
no longer exposed as a library. Users are recommended to perform
direct DBus calls to the well-known name of the miner(s) of
interest.

## No libtracker-miner

This library offered a too shallow collection of abstract objects
whose sole role is inserting data to Tracker data store. There is
no provided migration path, use TrackerSparqlConnection directly.

## No `tracker_sparql_connection_load()/load_async()/load_finish()`

This is superseded by the "LOAD <url>" SPARQL syntax.

## No `tracker_sparql_connection_statistics()/statistics_async()/statistics_finish()`

It is possible to query statistics through SPARQL. This query would
provide a similar return value than the statistics API:

```SPARQL
SELECT
  ?class
  (COUNT(?resource) AS ?count)
{
  ?class a rdfs:Class .
  ?resource a ?class
}
GROUP BY ?class
ORDER BY DESC ?count
```

But of course, other more detailed statistics may be obtained.

## No `TRACKER_NOTIFIER_FLAG_NOTIFY_UNEXTRACTED`

There is no replacement. If metadata about resources is added in several steps.
TrackerNotifier will inform of each.

## No `TRACKER_NOTIFIER_FLAG_QUERY_LOCATION`

If using the Nepomuk ontology, It is possible to query for the "nie:url" of
those elements in place. Other ontologies might have similar concepts.

## No `tracker_notifier_new()`

Notifiers are now created through tracker_sparql_connection_create_notifier().

## Different signature of [signal@Notifier::events] signal

A TrackerNotifier may hint changes across multiple endpoints (local or remote),
in consequence the signal additionally contains 2 string arguments, notifying
about the SPARQL endpoint the changes came from, and the SPARQL graph the changes
apply to.

## Return value change in [method@SparqlConnection.update_array_async]

This function changed to handle all changes within a single transaction. Returning
an array of errors for each individual update is no longer necessary, so it now
simply returns a boolean return value and a single error for the whole transaction.

## No `tracker_sparql_connection_get()/get_async()`

There is no longer a singleton SPARQL connection. If you are only interested in
tracker-miner-fs data, you can create a dedicated DBus connection to it through:

```c
conn = tracker_sparql_connection_bus_new ("org.freedesktop.Tracker3.Miner.Files", …);
```

If you are interested in storing your own data, you can do it through:

```c
conn = tracker_sparql_connection_new (…);
```

Note that you still may access other endpoints in SELECT queries, eg. for
tracker-miner-fs:

```SPARQL
SELECT ?url ?tag {
  SERVICE <dbus:org.freedesktop.Tracker3.Miner.Files> {
    ?a a rdfs:Resource;
       nie:url ?url .
  }
  ?b nie:url ?url;
     nao:hasTag ?tag
}
```

This SELECT query merges together data from the tracker-miner-fs endpoint
with data from the local endpoint.

## No tracker_sparql_connection_set_domain

Use a dedicated DBus TrackerSparqlConnection to the specific domain.

## No priority argument on SPARQL updates

All TrackerSparqlConnection updates API has been dropped the priority
argument. Updates are processed sequentially, and asynchronous tasks
are handled by the main loop with the default priority.
