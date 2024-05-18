Title: Performance dos and donts
slug: performance-advise

SPARQL is a very powerful query language. As it should be
suspected, this means there are areas where performance is
inherently sacrificed for versatility.

These are some tips to get the best of SPARQL as implemented
by TinySPARQL.

## Avoid queries with unrestricted predicates

Queries with unrestricted predicates are those like:

```SPARQL
SELECT ?p { <a> ?p 42 }
```

These involve lookups across all possible triples of
an object, which roughly translates to a traversal
through all tables and columns.

The most pathological case is:

```SPARQL
SELECT ?s ?p ?o { ?s ?p ?o }
```

Which does retrieve every triple existing in the RDF
triple store.

Queries with unrestricted predicates are most useful to
introspect resources, or the triple store in its entirety.
Production code should do this in rare occasions.

## Avoid the negated property path

The `!` negation operator in property paths negate the
match. For example:

```SPARQL
SELECT ?s ?o { ?s !nie:url ?o }
```

This query looks for every other property that is not
`nie:url`. The same reasoning than unrestricted predicates
apply, since that specific query is equivalent to:

```SPARQL
SELECT ?s ?o {
   ?s ?p ?o .
   FILTER (?p != nie:url)
}
```

## Specify graphs wherever possible

Queries on the union graph, or with unrestricted graphs, for
example:

```SPARQL
SELECT ?u { ?u a rdfs:Resource }
SELECT ?g ?u { GRAPH ?g { ?u a rdfs:Resource }}
```

Will traverse across all graphs. Query complexity will increment
linearly with the amount of graphs. Production code should rarely
need to introspect graphs, and should strive to being aware of
the graph(s) involved. The fastest case is accessing one graph.

The graph(s) may be specified through
`WITH / FROM / FROM NAMED / GRAPH` and other
SPARQL syntax for graphs. For example:

```SPARQL
WITH <http://example.com/Graph> SELECT ?u { ?u a rdfs:Resource }
SELECT ?g ?u FROM NAMED <http://example.com/Graph> { GRAPH ?g { ?u a rdfs:Resource }}
```

## Avoid globs and substring matching

Matching for regexp/glob/substrings defeats any index text fields
could have. For example:

```SPARQL
SELECT ?u {
  ?u nie:title ?title .
  FILTER (CONTAINS (?title, "banana"))
}
```

Will traverse all title strings looking for the substring. It is
encouraged to use fulltext search for finding matches within strings
where possible, for example:

```SPARQL
SELECT ?u { ?u fts:match "banana" }
```

## Use prepared statements

Using [class@SparqlStatement] allows to parse and compile
a query once, and reuse it many times. Its usage
is recommended wherever possible.
