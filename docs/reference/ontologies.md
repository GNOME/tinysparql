Title: Ontologies

Ontologies define the structure of the data that the RDF triple store
can hold. It defines the possible resource classes,
the properties these classes may have, and the relation between
the different classes as expressed by these properties.

# Base ontology

The base ontology is the seed for defining application-specific
ontologies. It defines the building blocks to build ontologies
upon, like the definition of classes, properties, and literal
types themselves. The base ontology is based on
[RDFS Schema](https://www.w3.org/TR/rdf-schema/).

It is made up of several components:

- [XML schema (XSD)](xsd-ontology.html) defines the basic literal types.
- [Resource description framework](rdf-ontology.html) defines properties,
  lists and language-tagged strings.
- [RDF Schema](rdfs-ontology.html) defines classes and inheritance.
- [Nepomuk Resource Language (NRL)](nrl-ontology.html) defines resource
  cardinality and database-level indexes.
- [Dublin core metadata (DC)](dc-ontology.html) defines a common set of
  document-oriented superproperties for RDF resources.

# Nepomuk

Nepomuk is the swiss army knife of the semantic desktop, similar
in scope to [Schema.org](https://schema.org). It defines
data structures for almost any kind of data you might want to store
in a personal computer.

It is split into several domains:

- [Nepomuk Information Element (NIE)](nie-ontology.html) is the
  core of Nepomuk. It settles the basic principles like the split
  between "container" and "content", and defines the base
  [nie:DataObject](nie-ontology.html#nie:DataObject) and
  [nie:InformationElement](nie-ontology.html#nie:InformationElement) objects
  that represent this split.
- [Nepomuk File Ontology (NFO)](nfo-ontology.html) describes the basic
  filesystem-oriented objects.
- [Nepomuk Multimedia (NMM)](nmm-ontology.html) describes multi-media data.
- [Nepomuk Contacts Ontology (NCO)](nco-ontology.html) describes contacts and
  addresses.
- [Libosinfo ontology](osinfo-ontology.html) describes OS images.
- [Maemo Feeds Ontology (MFO)](mfo-ontology.html) describes feeds.
- [Simplified Location Ontology (SLO)](slo-ontology.html) extends metadata
  with geolocation tagging.
- [Nepomuk Annotation Ontology (NAO)](nao-ontology.html) extends metadata
  with annotations.
- Other [TinySPARQL extensions](tracker-ontology.html) to further annotate
  data and link to external services.

# Creating custom ontologies

TinySPARQL does also allow developers to define ontologies that are tailored
for their use.

Ontologies are made themselves of RDF data in any of the formats supported by
the TinySPARQL library:

- [Turtle](https://www.w3.org/TR/turtle/) with `.ttl` or `.ontology` file
  extension.
- [Trig](https://www.w3.org/TR/trig/) with `.trig` file extension
- [JSON-LD](https://www.w3.org/TR/json-ld11/) with `.jsonld` file extension.

Custom-made ontologies will build upon the [base ontology](#base-ontology)
provided for this purpose. The following examples will use the Turtle format
for simplicity.

Ontologies may be split in multiple documents in a same directory. The individual
ontology files do not need be self-consistent (e.g. they may use definitions from
other files), but all the ontology files as a whole must be self-consistent.
TinySPARQL will not open or create a RDF triple store if the ontology is not
consistent, and will roll back any change if necessary.

TinySPARQL loads the ontology files in alphanumeric order, it is advisable
that those have a numbered prefix in order to load those at a consistent
order despite future additions.

## Defining a namespace

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
@prefix ex: <http://example.org/#> .

ex: a nrl:Namespace, nrl:Ontology;
    nrl:prefix "ex";
    rdfs:comment "example ontology";
    nrl:lastModified "2017-01-01T15:00:00Z".
```

## Defining classes

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

## Defining properties

Properties relate to a class, so all resources pertaining to that class
can define values for these.

```turtle
ex:cromosomes a rdf:Property;
              rdfs:domain ex:Eukaryote;
              rdfs:range xsd:integer.

ex:unicellular a rdf:Property;
               rdfs:domain ex:Eukaryote;
               rdfs:range xsd:bool.

ex:dateOfBirth a rdf:Property;
               rdfs:domain ex:Mammal;
               rdfs:range xsd:dateTime.
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
          rdfs:range ex:Mammal.

ex:pet a rdf:Property;
       rdfs:domain ex:Mammal;
       rdfs:range ex:Eukaryote.
```

There is also inheritance of properties, an example would be a property
in a subclass concretizing a more generic property from a superclass.

```turtle
ex:geneticInformation a rdf:Property;
                      rdfs:domain ex:Eukaryote;
                      rdfs:range xsd:string.

ex:dna a rdf:Property;
       rdfs:domain ex:Mammal;
       rdfs:range xsd:string;
       rdfs:subPropertyOf ex:geneticInformation.
```

SPARQL queries are expected to provide the same result when queried
for a property or one of its superproperties. Superproperties must
belong to a superclass of the class owning this property.

```SPARQL
# These two queries should provide the exact same result(s)
SELECT { ?animal a ex:Animal;
                 ex:geneticInformation "AGCT" }
SELECT { ?animal a ex:Animal;
                 ex:dna "AGCT" }
```

## Defining cardinality of properties

By default, properties are multivalued, there are no restrictions in
the number of values a property can store.

```SPARQL
INSERT DATA {
  <cat> a ex:Mammal .
  <dog> a ex:Mammal .

  <peter> a ex:Mammal ;
     ex:pets <cat>, <dog>.
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
                    ex:cromosomes 42. }

# This will succeed
INSERT DATA { <donald> a ex:Mammal;
                       ex:cromosomes 47. }
```

TinySPARQL does not implement support for other maximum cardinalities
than 1.

<!---
    XXX: explain how cardinality affects subproperties, superproperties
--->

## Defining uniqueness

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
    XXX: explain how inverse functional properties affect sub/superproperties
--->

## Defining indexes

It may be the case that SPARQL queries performed on the endpoint are
known to match, sort, or filter on certain properties more often than others.
In this case, the ontology may use `nrl:indexed` in property definitions:

```turtle
# Make matching/sorting on ex:name faster
ex:name a rdf:Property ;
        nrl:indexed true ;
        rdfs:domain ex:Animal ;
        rdfs:range xsd:string .
```

Sometimes, it may be desirable to make a combined index on two properties
at the same time, since they are often queried in combination. `nrl:secondaryIndex`
may be used for this purpose:

```turtle
ex:latitude a rdf:Property ;
            nrl:indexed true ;
            rdfs:domain ex:Location ;
            rdfs:range xsd:double .

ex:longitude a rdf:Property ;
             nrl:indexed true ;
             nrl:secondaryIndex ex:latitude ;
             rdfs:domain ex:Location ;
             rdfs:range xsd:double .
```

Lastly, `nrl:domainIndex` may be used to speed up queries by narrowing
down a property inherited from a superclass to the elements of the specific
subclass. It is most useful with very generic and populated superclasses,
and queries that only make sense to speed up for specific subclasses.

```turtle
# Make queries on ex:dateOfBirth faster for mammals, of all Animalia
ex:Mammal a rdfs:Class;
          rdfs:subClassOf ex:Animal;
          rdfs:comment "A mammal";
          nrl:domainIndex ex:dateOfBirth.
```

Classes may define multiple domain indexes. The domain indexes must be properties
owned by superclasses

**Note**: Be frugal with indexes, do not add these proactively. An index in the wrong
place might not affect query performance positively, but all indexes come at
a cost in disk size.

## Defining full-text search properties

TinySPARQL provides nonstandard full-text search capabilities, in order to use
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

## Predefined elements

It may be desirable for the ontology to offer predefined elements of a
certain class, which can then be used by the endpoint.

```turtle
ex:self a ex:Mammal.
```

Usage does not differ in use from the elements of that same class that
could be inserted in the endpoint.

```SPARQL
INSERT DATA { ex:self ex:pets <cat> .
              <cat> ex:pets ex:self . }
```

## Updating an ontology

As software evolves, sometimes changes in the ontology are unavoidable.
TinySPARQL can transparently handle certain ontology changes on existing
databases.

1. Adding a new class.
2. Removing a class.
   All resources will be removed from this class, and all related
   properties will disappear.
3. Changing the rdfs:subClassOf relationship of a class
   All existing resources of this class will satisfy the
   `{ ?resource a ?superclass }` graph pattern for the updated superclasses.
4. Adding a new property.
5. Removing a property.
   The property will disappear from all elements pertaining to the
   class in domain of the property.
6. Changing the rdfs:subPropertyOf relationship of a property.
   All existing values of the property will satisfy the
   `{ ?resource ?superproperty ?value }` graph pattern for the new
   superproperties.
7. Changing rdfs:range of a property.
   The following conversions are allowed:

    - `xsd:integer` to `xsd:bool`, `xsd:double` and `xsd:string`</listitem></varlistentry>
    - `xsd:double` to `xsd:bool`, `xsd:integer` and `xsd:string`</listitem></varlistentry>
    - `xsd:string` to `xsd:bool`, `xsd:integer` and `xsd:double`</listitem></varlistentry>

8. Adding and removing `nrl:domainIndex` from a class.
9. Adding and removing any kind of index from a property, this
   includes `nrl:indexed`, `nrl:fulltextIndexed` and `nrl:secondaryIndex`.
10. Changing the `nrl:weight` on a full-text indexed property.
11. Removing `nrl:maxCardinality` restrictions from a property.

However, there are certain ontology changes that TinySPARQL will find
incompatible. Either because they are incoherent or resulting into
situations where it can not deterministically satisfy the change
in the stored data. TinySPARQL will error out and refuse to do any data
changes in these situations:

- Properties with rdfs:range being `xsd:bool`, `xsd:date`, `xsd:dateTime`,
  or any other custom class are not convertible. Only conversions
  covered in the list above are accepted.
- Properties can not move across classes, thus any change in
  `rdfs:domain` is forbidden.
- You can not add `nrl:maxCardinality` restrictions on properties that
  are not being newly added.
- You can not add `nrl:InverseFunctionalProperty` restrictions on
  properties that are not being newly added.

The recommendation to bypass these situations is the same for all,
use different property and class names and use SPARQL to manually
migrate the old data to the new format if necessary.

High level code is in a better position to solve the
possible incoherences (e.g. picking a single value if a property
changes from multiple values to single value). After the manual
data migration has been completed, the old classes and properties
can be dropped.
