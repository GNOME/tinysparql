Title: SPARQL as understood by TinySPARQL
slug: sparql-and-tracker

This document describes the choices made by TinySPARQL in its interpretation
of the SPARQL documents, as well as the ways it diverges or extends on the
specifications.

## The default graph sets

The [SPARQL documentation](https://www.w3.org/TR/sparql11-update/#graphStore)
says:

```
Operations may specify graphs to be modified, or they may rely on a
default graph for that operation. […] The unnamed graph for the store
will be the default graph for any operations on that store. Depending
on implementation, the unnamed graph may refer to a separate graph, a
graph describing the named graphs, a representation of a union of
other graphs, etc.
```

By default, TinySPARQL declares a named graph called `nrl:DefaultGraph`, this
graph is implicitly part of the default graph used for solution matching, but
not to the pool of named graphs. Every other graph created by user data will be
added both to the default graph and the pool of named graphs.

The default graph is defined this way as the RDF union of the `nrl:DefaultGraph`
graph with every other known named graph. The default pool of named graphs
comprises all known graphs, with the exception of the default `nrl:DefaultGraph`
graph.

```SPARQL
SELECT *
WHERE {
  # Implicitly accesses the RDF union of all graphs
  ?a a rdfs:Resource
  GRAPH ?g {
    # Implicitly accesses every individual graph except nrl:DefaultGraph
    ?b a rdfs:Resource
  }
}
```

This behavior can be altered with `FROM` / `USING` and
`FROM NAMED` / `USING NAMED` syntax. The default graph is to all effects
anonymous, unless it is explicitly brought to the set of named graphs.

```SPARQL
# Access the default graph as a named graph
SELECT *
FROM NAMED nrl:DefaultGraph
WHERE {
  GRAPH ?g {
    ?u a rdfs:Resource .
  }
}
```

## Blank nodes

The [RDF documentation](https://www.w3.org/TR/2004/REC-rdf-concepts-20040210/#section-URI-Vocabulary)
says:

```
A blank node is a node that is not a URI reference or a literal. In
the RDF abstract syntax, a blank node is just a unique node that can
be used in one or more RDF statements, but has no intrinsic name.
```

By default, TinySPARQL does instead treat blank nodes as an URI generator. The
string referencing a blank node (e.g. as returned by cursors) permanently
identifies that blank node and can be used as an URI reference in
future queries.

The blank node behavior defined in the RDF/SPARQL specifications can
be enabled with the #TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES
flag.

## Property functions

The [SPARQL documentation](https://www.w3.org/TR/sparql11-query/#expressions)
says:

```
In addition, SPARQL provides the ability to invoke arbitrary functions
[…]. These functions are invoked by name (an IRI) within a SPARQL query.
```

TinySPARQL allows using all defined `rdf:Property`
instances as functions. If the property has multiple values, it will
propagate to the cursor as the `GROUP_CONCAT`
(with comma separator) of the multiple values.

## Entailment regime

An entailment regime is a set of rules defining what is valid RDF data, and what
other axiomatic RDF triples may be extrapolated from a RDF dataset. An example is
[RDF Schema entailment](https://en.wikipedia.org/wiki/RDF_Schema#RDFS_entailment).

For SPARQL queries, TinySPARQL implements a subset of
[RDF Schema entailment rules](https://www.w3.org/TR/rdf-mt/#RDFSRules).
Rules *rdfs1*, *rdfs5*, *rdfs6*, *rdfs10*, *rdfs11*, *rdfs12* and *rdfs13* are
not implemented.

The [SPARQL documentation](https://www.w3.org/TR/sparql11-entailment/#Updates)
says:

```
SPARQL endpoints that use an entailment regime other than simple entailment may
support update queries, but the exact behavior of the system for such queries
is not covered by this specification. SPARQL endpoints that use an entailment
regime other than simple entailment and that do support update queries should
describe the system behavior in the system's documentation.
```

TinySPARQL implements the propagation of `rdfs:subPropertyOf` (*rdfs7*) and
`rdfs:subClassOf`(*rdfs8*, *rdfs9*) at SPARQL update time. Other than this
detail, SPARQL updates can be considered to follow simple entailment in
practical effects. Rules *rdfs2*, *rdfs3*, *rdfs4a* and *rdfs4b* do not apply
to SPARQL updates. The inserted RDF data must still conform to the ontology.

Notably, resources must be given `rdf:type` properties explicitly in order to
be able to receive properties belonging to these classes. The effect of
`rdf:type` properties is local to the graph where it was defined.

## Syntax extensions

TinySPARQL offers some SPARQL syntax extensions. These predate the
existence of SPARQL 1.1 and stay for legacy reasons. These
extensions should be used sparingly, if at all.

### GROUP_CONCAT

The SPARQL specifications define the following syntax to use
a specific separator for the GROUP_CONCAT operation:

```SPARQL
GROUP_CONCAT (?var, separator=;)
```

TinySPARQL additionally accepts a simplified syntax:

```SPARQL
GROUP_CONCAT (?u, ';')
```

### BOUND

The BOUND function, as defined in the SPARQL specification,
only accepts variables as its single argument. TinySPARQL additionally
allows this function to deal with expressions, mainly allowing the
nesting of other functions, e.g. functional properties:

```SPARQL
SELECT BOUND (nfo:fileName (?u)) { ?u a nfo:FileDataObject }
```

### Subselects in expressions

TinySPARQL accepts subselects in place of expressions, these subselects
should return a single variable in order to act as a expression.
E.g. this query:

```SPARQL
SELECT (SELECT ?ret { ?u nie:hasPart ?elem }) { ?elem a nfo:Folder }
```

Would be equivalent to this:

```SPARQL
SELECT nie:hasPart(?elem) { ?elem a nfo:Folder }
```

### INSERT/DELETE SILENT

TinySPARQL allows the use of SILENT after INSERT and DELETE
keywords. Errors will be consequently silenced.

### INSERT OR REPLACE

TinySPARQL adds a special `INSERT OR REPLACE`
operation. This form of update will overwrite any existing values.

```SPARQL
INSERT OR REPLACE { <file:///> nfo:fileName 'root' }
```

This operation works the same independently of the cardinality,
multi-valued properties are cleared before the insertion.

If clearing a property is desired within the operation, the
value list may also contain the NULL keyword, e.g.:

```SPARQL
INSERT OR REPLACE { <file:///> nie:hasPart <a>, <b>, NULL, <c> }
```

Note that the `DELETE { … } INSERT { … } WHERE { … }`
syntax available in SPARQL 1.1 is a more versatile replacement.

### Expressions in ORDER BY

TinySPARQL allows the use of expressions in
`ORDER BY` clauses, e.g.:

```SPARQL
SELECT {
  # …
}
ORDER BY (?a - ?b)
```

### Variable names in SELECT clauses

The SPARQL 1.1 specifications enforce that all expressions
returned in a `SELECT` clause are set a variable name, e.g.:

```SPARQL
SELECT ((?a - ?b) AS ?sub) {
  # …
}
```

TinySPARQL relaxes this restriction, and does not enforce that
expressions are surrounded by parentheses, e.g.:

```SPARQL
SELECT ?a + ?b ?a - ?b AS ?sub {
  # …
}
```

Note that this hinders query readability (e.g. the example above
returns 2 values, an unnamed sum expression, and a named subtraction),
so its use is not recommended.

### Separator of update queries

TinySPARQL makes the use of the `;` separator
between update clauses optional. Its use is still recommended for
readability.

### CONSTRAINT syntax

TinySPARQL supports `CONSTRAINT GRAPH` and `CONSTRAINT SERVICE` clauses
in the query prologue. These clauses limit the access outside of the
specified graphs and services.

```SPARQL
# Only triples in the tracker:Audio graph will be returned
CONSTRAINT GRAPH tracker:Audio
SELECT * { ?s ?p ?o }
```

If a graph is specified within the query, but not allowed by a
`CONSTRAINT GRAPH` clause, it will be
effectively interpreted as an empty graph.

If a service is accessed within the query, but not allowed by a
`CONSTRAINT SERVICE` clause, it will be interpreted as an error, unless
`SERVICE SILENT` syntax is used. In that
case it will be interpreted as an empty graph.

The `CONSTRAINT` clauses cannot be
contradicted, multiple `CONSTRAINT` clauses
effectively intersect the set of allowed graphs/services with
previous clauses.

```SPARQL
CONSTRAINT GRAPH tracker:Video, tracker:Audio
CONSTRAINT GRAPH tracker:Video
# Only tracker:Video graph can be accessed
SELECT * { ?s ?p ?o }
```

```SPARQL
CONSTRAINT GRAPH tracker:Video
CONSTRAINT GRAPH tracker:Video, tracker:Audio
# Only tracker:Video graph can be accessed
SELECT * { ?s ?p ?o }
```

Disjoint sets result in an empty set of accessible graphs and services.

```SPARQL
CONSTRAINT GRAPH tracker:Video
CONSTRAINT GRAPH tracker:Audio
# There are no accessible graphs, this query returns no results
SELECT * { ?s ?p ?o }
```

## Mapping IDs and IRIs

TinySPARQL provides the `tracker:id` and `tracker:uri` SPARQL
functions, that allow converting an URI reference to a numeric
identifier, and back.

These identifiers are expected to be valid a long as the URI is
referenced in the store. The existence of these SPARQL functions
mostly obey legacy reasons and its use is not recommended.

## Parameters and prepared statements

TinySPARQL accepts `~` prefixed variables in place of literals
throughout most of the SPARQL select syntax. These variables
are treated as parameters at query time, so it is possible
to prepare a query statement once and reuse it many times
assigning different values to those parameters at query time.

See [class@SparqlStatement] documentation for more information.

## Full-text search

TinySPARQL provides full-text search capabilities, these are exposed
as a `fts:match` pseudo-property that will match the resources
matching the given text string.

To complement this pseudo property, TinySPARQL provides the
`fts:snippet`, `fts:offsets` and `fts:rank` SPARQL functions that
can be used on the matches.

The ontology needs to define the properties that are matched via
this full-text search mechanism, by toggling the
`tracker:fulltextIndexed` property on in the
text `rdf:Property` instances. See the documentation
on [defining ontologies](ontologies.html).

## DESCRIBE queries

The [SPARQL documentation](https://www.w3.org/TR/sparql11-query/#describe)
says:

```
The DESCRIBE form returns a single result RDF graph containing RDF data about resources.
```

In order to allow serialization to RDF formats that allow expressing graph information
(e.g. [Trig](https://www.w3.org/TR/trig/)), DESCRIBE resultsets have 4 columns for
subject / predicate / object / graph information.
