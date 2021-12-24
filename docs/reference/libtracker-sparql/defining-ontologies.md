---
title: Defining ontologies
short-description: Defining Ontologies
...

# Defining ontologies

An ontology defines the entities that a Tracker endpoint can store, as
well as their properties and the relationships between different entities.

Tracker internally uses the following ontologies as its base, all ontologies
defined by the user of the endpoint are recommended to be build around this
base:

- XML Schema (XSD), defining basic types
- Resource Description Framework (RDF), defining classes, properties and
  inheritance
- Nepomuk Resource Language (NRL), defining resource uniqueness, inheritance
  and indexes.
- Dublin Core (DC), defining common superproperties for documents

Ontologies are Turtle files with the .ontology extension, Tracker parses all
ontology files from the given directory. The individual ontology files may
not be self-consistent (i.e. use missing definitions), but
all the ontology files as a whole must be.

Tracker loads the ontology files in alphanumeric order, it is advisable
that those have a numbered prefix in order to load those at a consistent
order despite future additions.

## Creating an ontology

### Defining a namespace

A namespace is the topmost layer of an individual ontology, it will
contain all classes and properties defined by it. In order to define
a namespace you can do:

```turtle
# These prefixes will be used in the definition of the ontology,
# thus must be explicitly defined
@prefix nrl: <http://tracker.api.gnome.org/ontology/v3/nrl#> .
@prefix rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .
@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .
@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .

# This is our example namespace
@prefix ex: <http://example.org/#>

ex: a nrl:Namespace, nrl:Ontology
    nrl:prefix "ex"
    rdfs:comment "example ontology"
    nrl:lastModified "2017-01-01T15:00:00Z"
```

### Defining classes

Classes are the base of an ontology, all stored resources must define
themselves as "being" at least one of these classes. They all derive
from the base rdfs:Resource type. To eg. define classes representing
animals and plants, you can do:

```turtle
ex:Eukaryote a rdfs:Class;
             rdfs:subClassOf rdfs:Resource;
             rdfs:comment "An eukaryote".
```

By convention all classes use CamelCase names, although class names
are not restricted. The allowed charset is UTF-8.

Declaring subclasses is possible:

```turtle
ex:Animal a rdfs:Class;
          rdfs:subClassOf ex:Eukaryote;
          rdfs:comment "An animal".

ex:Plant a rdfs:Class;
          rdfs:subClassOf ex:Eukaryote;
          rdfs:comment "A plant".

ex:Mammal a rdfs:Class;
          rdfs:subClassOf ex:Animal;
          rdfs:comment "A mammal".
```

With such classes defined, resources may be inserted to the endpoint,
eg. with the SPARQL:

```SPARQL
INSERT DATA { <merry> a ex:Mammal }
INSERT DATA { <treebeard> a ex:Animal, ex:Plant }
```

Note that multiple inheritance is possible, resources will just inherit
all properties from all classes and superclasses.

### Defining properties

Properties relate to a class, so all resources pertaining to that class
can define values for these.

```turtle
ex:cromosomes a rdf:Property;
              rdfs:domain ex:Eukaryote;
              rdfs:range xsd:integer.

ex:unicellular a rdf:Property;
               rdfs:domain ex:Eukaryote;
               rdfs:range xsd:bool;

ex:dateOfBirth a rdf:Property;
               rdfs:domain ex:Mammal;
               rdfs:range xsd:dateTime;
```

The class the property belongs to is defined by `rdfs:domain`, while the
data type contained is defined by `rdfs:range`. By convention all
properties use dromedaryCase names, although property names are not
restricted. The allowed charset is UTF-8.

The following basic types are supported:

- `xsd:boolean`
- `xsd:string` and `rdf:langString`
- `xsd:integer`, ranging from -2^63 to 2^63-1.
- `xsd:double`, able to store a 8 byte IEEE floating point number.
- `xsd:date` and `xsd:dateTime`, able to store dates and times since
   January 1st 1 AD, with microsecond resolution.

Of course, properties can also point to resources of the same or
other classes, so stored resources can conform a graph:

```turtle
ex:parent a rdf:Property;
          rdfs:domain ex:Mammal;
          rdfs:range ex:Mammal;

ex:pet a rdf:Property;
       rdfs:domain ex:Mammal;
       rdfs:range ex:Eukaryote;
```

There is also inheritance of properties, an example would be a property
in a subclass concretizing a more generic property from a superclass.

```turtle
ex:geneticInformation a rdf:Property;
                      rdfs:domain ex:Eukaryote;
                      rdfs:range xsd:string;

ex:dna a rdf:Property;
       rdfs:domain ex:Mammal;
       rdfs:range xsd:string;
       rdfs:subPropertyOf ex:geneticInformation.
```

SPARQL queries are expected to provide the same result when queried
for a property or one of its superproperties.

```SPARQL
# These two queries should provide the exact same result(s)
SELECT { ?animal a ex:Animal;
                 ex:geneticInformation "AGCT" }
SELECT { ?animal a ex:Animal;
                 ex:dna "AGCT" }
```

### Defining cardinality of properties

By default, properties are multivalued, there are no restrictions in
the number of values a property can store.

```SPARQL
INSERT DATA {
  <cat> a ex:Mammal .
  <dog> a ex:Mammal .

  <peter> a ex:Mammal ;
     ex:pets <cat>, <dog>
}
```

Wherever this is not desirable, cardinality can be limited on properties
through nrl:maxCardinality.

```turtle
ex:cromosomes a rdf:Property;
              rdfs:domain ex:Eukaryote;
              rdfs:range xsd:integer;
              nrl:maxCardinality 1.
```

This will raise an error if the SPARQL updates in the endpoint end up
in the property inserted multiple times.

```SPARQL
# This will fail
INSERT DATA { <cat> a ex:Mammal;
                    ex:cromosomes 38;
                    ex:cromosomes 42 }

# This will succeed
INSERT DATA { <donald> a ex:Mammal;
                       ex:cromosomes 47 }
```

Tracker does not implement support for other maximum cardinalities
than 1.

<!---
    XXX: explain how cardinality affects subproperties, superproperties
--->

### Defining uniqueness

It is desirable for certain properties to keep their values unique
across all resources, this can be expressed by defining the properties
as being a nrl:InverseFunctionalProperty.

```turtle
ex:geneticInformation a rdf:Property, nrl:InverseFunctionalProperty;
                      rdfs:domain ex:Eukaryote;
                      rdfs:range xsd:string;
```

With that in place, no two resources can have the same value on the
property.

```SPARQL
# First insertion, this will succeed
INSERT DATA { <drosophila> a ex:Eukariote;
                           ex:geneticInformation "AGCT" }

# This will fail
INSERT DATA { <melanogaster> a ex:Eukariote;
                             ex:geneticInformation "AGCT" }
```

<!---
    XXX: explain how inverse functional proeprties affect sub/superproperties
--->

### Defining indexes

It may be the case that SPARQL queries performed on the endpoint are
known to match, sort, or filter on certain properties more often than others.
In this case, the ontology may use nrl:domainIndex in the class definition:

```turtle
# Make queries on ex:dateOfBirth faster
ex:Mammal a rdfs:Class;
          rdfs:subClassOf ex:Animal;
          rdfs:comment "A mammal";
          nrl:domainIndex ex:dateOfBirth.
```

Classes may define multiple domain indexes.

**Note**: Be frugal with indexes, do not add these proactively. An index in the wrong
place might not affect query performance positively, but all indexes come at
a cost in disk size.

### Defining full-text search properties

Tracker provides nonstandard full-text search capabilities, in order to use
these, the string properties can use nrl:fulltextIndexed:

```turtle
ex:name a rdf:Property;
        rdfs:domain ex:Mammal;
        rdfs:range xsd:string;
        nrl:fulltextIndexed true;
        nrl:weight 10.
```

Weighting can also be applied, so certain properties rank higher than others
in full-text search queries. With nrl:fulltextIndexed in place, sparql
queries may use full-text search capabilities:

```SPARQL
SELECT { ?mammal a ex:Mammal;
                 fts:match "timmy" }
```

### Predefined elements

It may be desirable for the ontology to offer predefined elements of a
certain class, which can then be used by the endpoint.

```turtle
ex:self a ex:Mammal.
```

Usage does not differ in use from the elements of that same class that
could be inserted in the endpoint.

```SPARQL
INSERT DATA { ex:self ex:pets <cat> .
              <cat> ex:pets ex:self }
```

### Accompanying metadata

Ontology files are optionally accompanied by description files, those have
the same basename, but the ".description" extension.

```turtle
@prefix dsc: <http://tracker.api.gnome.org/ontology/v3/dsc#> .

<virtual-ontology-uri:30-nie.ontology> a dsc:Ontology ;
	dsc:title "Example ontology" ;
	dsc:description "A little bit of this and that." ;
	dsc:upstream "http://www.example.org/ontologies";
	dsc:author "John doe, &lt;john@example.org&gt;";
	dsc:editor "Jane doe, &lt;jane@example.org&gt;";
	dsc:gitlog "http://git.example.org/cgit/tracker/log/example.ontology";
	dsc:contributor "someone else, &lt;some1@example.org&gt;";

	dsc:localPrefix "ex" ;
	dsc:baseUrl "http://www.example.org/ontologies/ex#";
	dsc:relativePath "./10-ex.ontology" ;

	dsc:copyright "All rights given away".
```

## Updating an ontology

As software evolves, sometimes changes in the ontology are unavoidable.
Tracker can transparently handle certain ontology changes on existing
databases.

1. Adding a class.
2. Removing a class.
   All resources will be removed from this class, and all related
   properties will disappear.
3. Adding a property.
4. Removing a property.
   The property will disappear from all elements pertaining to the
   class in domain of the property.
5. Changing rdfs:range of a property.
   The following conversions are allowed:

    - `xsd:integer` to `xsd:bool`, `xsd:double` and `xsd:string`</listitem></varlistentry>
    - `xsd:double` to `xsd:bool`, `xsd:integer` and `xsd:string`</listitem></varlistentry>
    - `xsd:string` to `xsd:bool`, `xsd:integer` and `xsd:double`</listitem></varlistentry>

6. Adding and removing `nrl:domainIndex` from a class.
7. Adding and removing `nrl:fulltextIndexed` from a property.
8. Changing the `nrl:weight` on a property.
9. Removing `nrl:maxCardinality` from a property.

<!---
    XXX: these need documenting too
    add intermediate superproperties
    add intermediate superclasses
    remove intermediate superproperties
    remove intermediate superclasses
--->

However, there are certain ontology changes that Tracker will find
incompatible. Either because they are incoherent or resulting into
situations where it can not deterministically satisfy the change
in the stored data. Tracker will error out and refuse to do any data
changes in these situations:

- Properties with rdfs:range being `xsd:bool`, `xsd:date`, `xsd:dateTime`,
  or any other custom class are not convertible. Only conversions
  covered in the list above are accepted.
- You can not add `rdfs:subClassOf` in classes that are not being
  newly added. You can not remove `rdfs:subClassOf` from classes.
  The only allowed change to `rdfs:subClassOf` is to correct
  subclasses when deleting a class, so they point a common
  superclass.
- You can not add `rdfs:subPropertyOf` to properties that are not
  being newly added. You can not change an existing
  `rdfs:subPropertyOf` unless it is made to point to a common
  superproperty. You can however remove `rdfs:subPropertyOf` from
  non-new properties.
- Properties can not move across classes, thus any change in
  `rdfs:domain` is forbidden.
- You can not add `nrl:maxCardinality` restrictions on properties that
  are not being newly added.
- You can not add nor remove `nrl:InverseFunctionalProperty` from a
  property that is not being newly added.

The recommendation to bypass these situations is the same for all,
use different property and class names and use SPARQL to manually
migrate the old data to the new format if necessary.

High level code is in a better position to solve the
possible incoherences (e.g. picking a single value if a property
changes from multiple values to single value). After the manual
data migration has been completed, the old classes and properties
can be dropped.

Once changes are made, the nrl:lastModified value should be updated
so Tracker knows to reprocess the ontology.
