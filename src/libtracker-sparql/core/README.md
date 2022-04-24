# SPARQL engine implementation

This document explains some internals of Tracker's SPARQL implementation.

## Database format

TrackerDataManager is the central piece to create an instance of a
triple store, either at a given directory or in memory.

When a TrackerDataManager is created, the ontology files
are parsed into internal TrackerOntology data structures, and
databases are created/updated with a set of
tables and indexes that match the ontology definition.

By default one "meta.db" database file is created, holding the
anonymous graph and other internal tables.

Named graphs are created as separate databases with a
mirrored schema as created from the ontology. These are all
attached via the main database connection, with a handle name
that matches the expanded graph name.

The ontology definition is translated into a database schema with the
following rules:

### The "Resource" table

Every resource (defined by either a blank node or a URI) that is inserted
to a Tracker database gets a distinct ROWID. The "Resource" table contains
a mapping from these ROWIDs to the URI or (persistent) blank node strings
that identify the resource.

The ROWID is persistent as long as the resource exists, and is used
across all tables. Internally, queries across multiple tables are most
often done matching ROWIDs, with resources being translated to their
corresponding strings only in the topmost layers of the query.

This table is not defined by the ontology, it is internally created in
the main "meta.db" database on all triple store instances. It is the
canonical source of ROWIDs for all ontology tables in all attached
databases.

### The "Graph" table

Graphs are also defined by URIs, and these get a distinct ROWID. The
"Graph" table maintains those URIs that were defined as graphs in the
database, e.g.:

```
CREATE GRAPH tracker:FileSystem
```

This is used by Tracker to introspect the available/known graphs. The
"Graph" table is created in all Tracker triple store instances, and
only exists in the main "meta.db" database created.

### The "Refcount" table

This table can be deemed an implementation detail. Since ontologies may
be vast (our stock one at least is), and there may be countless properties
pointing across resources, we need to track all these locations to know
when did a resource stop being used, and can thus be disposed.

The typical approach would involve foreign keys, so referential integrity
in the database is automatically maintained when a resource goes away
from all tables and columns. This approach is however impractical with
too many tables and cross-referencing columns (again, Nepomuk is vast).

Instead, this accounting is performed as a "refcount" on the resource,
maintained in code (every individual insertion and deletion is already
tracked). After the refcount reaches 0, the ROWID can be deleted from
the "Resource" table. This approach is many times faster than classic
referential integrity.

Refcounts are accounted per-graph, each graph database has a "Refcount"
table.

### The "metadata" table

This is a simple key/value table holding internal accounting data, e.g.
the full-text search settings that apply on the database. It only
exists in the main "meta.db" database.

### Class tables

Every rdf:Class definition, e.g.:

```
nmm:MusicPiece a rdfs:Class .
```

Gets a matching table named like the class itself, e.g.

```
CREATE TABLE "nmm:MusicPiece" (ROWID INTEGER, ...)
```

### Single valued properties

All single-valued properties, e.g.:

```
nmm:musicAlbum a rdf:Property ;
  nrl:maxCardinality 1 ;
  rdfs:domain nmm:MusicPiece ;
```

Are made to be columns in the class table they are in domain of:

```
CREATE TABLE "nmm:MusicPiece" (ROWID INTEGER, "nmm:musicAlbum" INTEGER, ...)
```

### Multi-valued property tables

All multi-valued properties (i.e. no `nrl:maxCardinality`):

```
nmm:performer a rdf:Property ;
  rdfs:domain nmm:MusicPiece .
```

Get their own table, named after the class, and the property name itself:

```
CREATE TABLE "nmm:MusicPiece_nmm:performer" (ID INTEGER, "nmm:performer" ...)
```

These tables have always 2 columns, a non-unique "ID" and a column
to hold the property value.

### Domain indexes

Domain indexes are indexes made from a subclass on a property of
a superclass.

```
ex:A a rdfs:Class .

ex:propA a rdf:Property
  nrl:maxCardinality 1 ;
  rdfs:domain ex:A ;

ex:B a rdfs:Class ;
    nrl:domainIndex ex:propA ;
    rdfs:subClassOf ex:A .
```

This is typically desirable to avoid setting an index on a very
generic/populated property, while it is only relevant on elements of
this subclass.

This is implemented in Tracker by replicating the property (and content)
in the subclass table:

```
CREATE TABLE "ex:A" (ID INTEGER, "ex:propA" ...)
CREATE TABLE "ex:B" (ID INTEGER, "ex:propA" ...)
```

A database index is then created on the inherited property. Insertions
and deletions in these properties will be likewise replicated for all
tables where the property exists. As such, there is a higher (but
probably negligible) cost in updating these properties.

### Full-text search

Properties defined as full-text search, e.g.:

```
nie:title a rdf:Property ;
        nrl:maxCardinality 1 ;
        rdfs:range xsd:string ;
        nrl:fulltextIndexed true .
```

All FTS properties are gathered by ROWID in a "fts_view" view,
the "fts5" table is a contentless FTS5 table made on top of
"fts_view", thus textual data is only stored once (in the
class/property table they belong), and the "fts5" table only
stores the tokenization info necessary for full-text search.

### Virtual tables

To put all of SPARQL together, Tracker implements some virtual
tables:

#### The "tracker_triples" table

This is a virtual table implementation made to answer queries
like:

```
SELECT * { ?s ?p ?o }
SELECT * { GRAPH ?g { ?s ?p ?o } }
SELECT * { ?s ?p 42 }
SELECT * { <a> ?p 42 }
```

These would boil down to roughly:

```
SELECT * FROM *;
```

In SQL, which is not even real syntax. The virtual table has
graph/subject/predicate/object columns, and serializes access
to all properties so they are given in this format.

### The "tracker_services" virtual table

This table implements access to external services (in SPARQL,
the SERVICE {} graph pattern), e.g.:

```
SELECT * {
  SERVICE <http:...> {
    ?u a rdfs:Resource ;
       rdfs:label ?l .
  }
  ...
}
```

It does so by querying on a remote TrackerSparqlConnection (whose
URI is determined by the "service" column in the virtual table),
performing a query (as specified in the "query" column in the virtual
table), and mapping the resulting cursor/resultset as the content
of the virtual table. These service/query columns can be considered
input-only parameters to the virtual table.

The remote SPARQL query is generalized so all involved variables are
queried, e.g.:

```
SELECT ?u ?l {
  ?u a rdfs:Resource ;
     rdfs:label ?l .
}
```

All returned variables are mapped to columns in the virtual
table, this virtual table can the be used further along in the
query as any other regular table.

## Indexes

Implicitly, ROWID columns are indexed. This works in Tracker's
advantage since matching by ROWID across different tables is
a central part of how SPARQL translates to our database format.

Additionally, the ontologies may define other indexes on
properties.

### Primary indexes

These indexes are defined on a property, and allow faster
range/specific matches, and ordering.

```
nie:mimeType a rdf:Property ;
  rdfs:domain nie:InformationElement ;
  rdfs:range xsd:string ;
  nrl:indexed true .
```

Results in:

```
CREATE INDEX "nie:InformationElement_nie:mimeType" ON "nie:InformationElement" ("nie:mimeType");
```

### Secondary indexes

These indexes are defined on one property, pointing to another
property:

```
ex:propA a rdf:Property ;
  nrl:maxCardinality 1 ;
  rdfs:domain ex:Class .

ex:propB a rdf:Property ;
  nrl:maxCardinality 1 ;
  rdfs:domain ex:Class ;
  nrl:secondaryIndex ex:propA .
```

And effectively result in a compound index on both properties:

```
CREATE INDEX "ex:Class_ex:propB" ON "ex:Class" ("ex:propB", "ex:propA");
```

Secondary indexes can only apply on single-valued properties
(i.e. `nrl:maxCardinality 1`).

### Domain indexes

In addition to the extra column in the table defining the domain index
(see above), this extra column gets an index, similar to primary indexes:

```
CREATE INDEX "ex:B_ex:propA" ON "ex:B" ("ex:propA");
```

Domain indexes can only apply on single-valued properties
(i.e. `nrl:maxCardinality 1`).

## Supported data types

Tracker does the following mapping of data types:

- `xsd:integer` is `INTEGER`.
- `xsd:boolean` is `INTEGER`.
- `xsd:double` is `REAL`.
- `xsd:dateTime` and `xsd:date` are either `INTEGER` or `TEXT`
   depending on whether a Unix timestamp is lossless at encoding
   the value (e.g. timezone information is specified, granularity
   is larger than milliseconds).
- `rdf:langString` are `BLOB` with `$str\0$lang` format.

## SPARQL query translation

### Queries

The SPARQL parser is very heavily based on the BNF syntax
description at https://www.w3.org/TR/sparql11-query/#grammar.
When parsing a SPARQL query, an expression tree is created
representing the query as per this grammar.

Once the parsing tree is constructed, the query is interpreted
node by node, by a series of functions that are named like
the individual BNF rules. In these functions conversion to
SQL happens.

For the most part, this SQL conversion is a "serialization" with
jumps, query building uses TrackerStringBuilder, which allows adding
"placeholders", empty substrings at a fixed position that can be
filled in at a later point during query translation. The final result
is a SQL string that:

- Has all literals replaced with SQL variables (e.g `$1`), this
  is done to maximize queries being reusable at the `sqlite3_stmt`
  level.

- On the outside behaves exactly the same as the SPARQL query,
  i.e. does the equivalent algebra, and returns the expected values
  in the expected order.

### Updates

Database updates are done by decomposing update queries into a
series of simple delete/insert commands over graph/subject/predicate/object
quads. E.g., the most complete SPARQL update form:

```
DELETE {
  ...
} INSERT {
  ...
} WHERE {
  ...
}
```

Works by:

- Resolving the WHERE{} pattern, collecting all variables.
- For each row in the resultset:
   - The quad data at the DELETE{} clause is processed, substituting
     variables with the values in the resultset.
   - The quad data at the INSERT{} clause is processed, substituting
     variables with the values in the resultset.

There are no "updates": deletions and insertions are performed in
this decomposed manner, atomicity is achieved via transactions.
See `tracker_data_insert_statement` and `tracker_data_delete_statement`.

Note there is a `tracker_data_update_statement` for `INSERT OR REPLACE`
non-standard SPARQL syntax, but this is more of a façade for delete/insert
underneath.

## Object glossary

- `TrackerDBInterface`: Database interface, each of them holds a
  connection to the main "meta.db" SQLite database, with all graph
  databases attached.
- `TrackerDBManager`: Manager of database interfaces, it is the
  gatekeeper to the unique read/write database connection, and
  manages the pool of readonly connections that can run in parallel.
- `TrackerDataManager`: Manages the database structure, creates
  all tables from the ontology definition and all other internal
  management tables.
   - tracker-data-update.c: Contains all the code that performs
     insertion and deletion of triple data.
- `TrackerOntology`, `TrackerNamespace`, `TrackerClass`, `TrackerProperty`:
  in-memory representation of the ontology and the ontology changes
  being applied.
- `TrackerSparqlParser`: SPARQL syntax parser.
- `TrackerSparql`: Conversion from SPARQL into SQL.
- tracker-vtab-*.c: Implementation files for SQLite virtual tables.
