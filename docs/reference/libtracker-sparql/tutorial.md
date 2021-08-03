---
title: SPARQL Tutorial
short-description: SPARQL Tutorial
...

# SPARQL Tutorial

This tutorial aims to introduce you to RDF and SPARQL from the ground
up. All examples come from the Nepomuk ontology, and even though
the tutorial aims to be generic enough, it mentions things
specific to Tracker, those are clearly spelled out.

## RDF Triples

RDF data define a graph, composed by vertices and edges. This graph is
directed, because edges point from one vertex to another, and it is
labeled, as those edges have a name. The unit of data in RDF is a
triple of the form:

    subject  predicate  object

Or expressed visually:

![Triple Graph](triple-graph-1.png)

Subject and object are 2 graph vertices and the predicate is the edge,
the accumulation of those triples form the full graph. For example,
the following triples:

```turtle
<a> a nfo:FileDataObject .
<a> a nmm:MusicPiece .
<a> nie:title "Images" .
<a> nmm:musicAlbum <b> .
<a> nmm:albumArtist <c> .
<a> nmm:albumArtist <d> .
<a> nmm:performer <e> .

<b> a nmm:MusicAlbum .
<b> nie:title "Go Off!" .

<c> a nmm:Artist .
<c> nmm:artistName "Jason Becker" .

<d> a nmm:Artist .
<d> nmm:artistName "Marty Friedman" .

<e> a nmm:Artist .
<e> nmm:artistName "Cacophony" .
```

Would visually generate the following graph:

![Triple Graph](triple-graph-2.png)

The dot after each triple is not (just) there for legibility, but is
part of the syntax. The RDF triples in full length are quite
repetitive and cumbersome to write, luckily they can be shortened by
providing multiple objects (with `,` separator) or multiple
predicate/object pairs (with `;` separator), the previous RDF could be
transformed into:

```turtle
<a> a nfo:FileDataObject, nmm:MusicPiece .
<a> nie:title "Images" .
<a> nmm:musicAlbum <b> .
<a> nmm:albumArtist <c> , <d> .
<a> nmm:performer <e> .

<b> a nmm:MusicAlbum .
<b> nie:title "Go Off!" .

<c> a nmm:Artist .
<c> nmm:artistName "Jason Becker" .

<d> a nmm:Artist .
<d> nmm:artistName "Marty Friedman" .

<e> a nmm:Artist .
<e> nmm:artistName "Cacophony" .
```

And further into:

```turtle
<a> a nfo:FileDataObject, nmm:MusicPiece ;
    nie:title "Images" ;
    nmm:musicAlbum <b> ;
    nmm:albumArtist <c>, <d> ;
    nmm:performer <e> .

<b> a nmm:MusicAlbum ;
    nie:title "Go Off!" .

<c> a nmm:Artist ;
    nmm:artistName "Jason Becker" .

<d> a nmm:Artist ;
    nmm:artistName "Marty Friedman" .

<e> a nmm:Artist ;
    nmm:artistName "Cacophony" .
```

## SPARQL

SPARQL defines a query language for RDF data. How does a query
language for graphs work? Naturally by providing a graph to be
matched, it is conveniently called the "graph pattern".

To begin simple, the simplest query would consist of a triple with all
3 elements defined, e.g.:

```SPARQL
ASK { <a> nie:title "Images" }
```

Which would result in `true`, as the triple does exist. The ASK query
syntax is actually the simplest form of graph testing, resulting in a
single boolean row/column containing whether the provided graph exists
in the store or not. It also works for more complex graphs, for
example:

```SPARQL
ASK { <a> nie:title "Images" ;
          nmm:albumArtist <c> ;
          nmm:musicAlbum <b> .
      <b> nie:title "Go Off!" .
      <c> nmm:artistName "Jason Becker" }
```

But of course the deal of a query language is being able to obtain the
stored data. The `SELECT` query syntax is used for that, and variables
are denoted with a `?` prefix, variables act as "placeholders" where
any data will match and be available to the resultset or within the
query as that variable name. The following query would be the opposite
to the first `ASK` query:

```SPARQL
SELECT * { ?subject ?predicate ?object }
```

What does this query do? it provides a triple with 3 variables, that
every known triple in the database will match. The `*` is a shortcut
for all queried variables, the query could also be expressed as:

```SPARQL
SELECT ?subject ?predicate ?object { ?subject ?predicate ?object }
```

However, querying for all known data is most often hardly useful, this
got unwieldly soon! Luckily, that is not necessarily the case, the
variables may be used anywhere in the triple definition, with other
triple elements consisting of literals you want to match for, e.g.:

```SPARQL
# Give me the title of resource <a> (Result: "Images")
SELECT ?songName { <a> nie:title ?songName }
```

```SPARQL
# What is this text to <b>? (Result: the nie:title)
SELECT ?predicate { <b> ?predicate "Go Off!" }
```

```SPARQL
# What is the resource URI of this fine musician? (Result: <d>)
SELECT ?subject { ?subject nmm:artistName "Marty Friedman" }
```

```SPARQL
# Give me all resources that are a music piece (Result: <a>)
SELECT ?song { ?song a nmm:MusicPiece }
```

And also combinations of them, for example:

```SPARQL
# Give me all predicate/object pairs for resource <a>
SELECT ?pred ?obj { <a> ?pred ?obj }
```

```SPARQL
# The Answer to the Ultimate Question of Life, the Universe, and Everything
SELECT ?subj ?pred { ?subj ?pred 42 }
```

```SPARQL
# Give me all resources that have a title, and their title.
SELECT ?subj ?obj { ?subj nie:title ?obj }
```

And of course, the graph pattern can hold more complex triple
definitions, that will be matched as a whole across the stored
data. for example:

```SPARQL
# Give me all songs from this fine album
SELECT ?song { ?album nie:title "Go Off!" .
               ?song nmm:musicAlbum ?album }
```

```SPARQL
# Give me all song resources, their title, and their album title
SELECT ?song ?songTitle ?albumTitle { ?song a nmm:MusicPiece ;
                                            nmm:musicAlbum ?album ;
                                            nie:title ?songTitle .
                                      ?album nie:title ?albumTitle }
```

Stop a bit to think on the graph pattern expressed in the last query:
![Graph Pattern](triple-graph-3.png)

This pattern on one hand consists of specified data (eg. `?song` must be
a `nmm:MusicPiece`, it must have a `nmm:musicAlbum` and a `nie:title`,
`?album` must have a `nie:title`, which must all apply for a match
to happen.

On the other hand, the graph pattern contains a number of variables,
some only used internally in the graph pattern, as a temporary
variable of sorts (?album, in order to express the relation between
?song and its album title), while other variables are requested in the
result set.

  <!-- FIXME: Keep writing! -->
  <!--
  <chapter id="tracker-tutorial-ontologies">
    <title>Ontologies</title>
  </chapter>
  <chapter id="tracker-tutorial-inserting-data">
    <title>Inserting data</title>
  </chapter>
  <chapter id="tracker-tutorial-updates-deletes">
    <title>Updates and deletes</title>
  </chapter>
  <chapter id="tracker-tutorial-named-nodes">
    <title>Named nodes</title>
  </chapter>
  <chapter id="tracker-tutorial-blank-nodes">
    <title>Blank nodes</title>
  </chapter>
  <chapter id="tracker-tutorial-property-paths">
    <title>Property paths</title>
  </chapter>
  <chapter id="tracker-tutorial-optional">
    <title>Optional data</title>
  </chapter>
  <chapter id="tracker-tutorial-filtering">
    <title>Filtering data</title>
  </chapter>
  <chapter id="tracker-tutorial-binding">
    <title>Binding expressions to variables</title>
  </chapter>
  <chapter id="tracker-tutorial-aggregates">
    <title>Aggregates</title>
  </chapter>
  <chapter id="tracker-tutorial-graphs">
    <title>Graphs</title>
  </chapter>
  <chapter id="tracker-tutorial-services">
    <title>Services</title>
  </chapter>
  <chapter id="tracker-tutorial-import-export">
    <title>Importing and exporting data</title>
  </chapter>
  <chapter id="tracker-tutorial-graph-management">
    <title>Graph Management</title>
  </chapter>
  -->
</part>
