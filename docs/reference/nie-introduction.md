# Overview

## Introduction

The core of the NEPOMUK Information Element Ontology and the entire
Ontology Framework revolves around the concepts of [nie:DataObject](nie-ontology.html#nie:DataObject) and
[nie:InformationElement](nie-ontology.html#nie:InformationElement). They express  the representation
and content of a piece of data. Their specialized subclasses (defined
in the other ontologies) can be used to classify
a wide array of desktop resources and express them in RDF.

[nie:DataObject](nie-ontology.html#nie:DataObject) class represents a collection of
bytes somewhere (local or remote), the physical entity that contain
data. The *meaning* (interpretation) of that entity (e.g. a music file,
a picture) is represented on the
[nie:InformationElement](nie-ontology.html#nie:InformationElement) side of the
ontology.

All resources on the desktop are basically related to each other with two most fundamental types
of relations: interpretation, Expressed through [nie:interpretedAs](nie-ontology.html#nie:interpretedAs) and its reverse
[nie:isStoredAs](nie-ontology.html#nie:isStoredAs).

![](interpretation.svg)

And containment, expressed through [nie:hasPart](nie-ontology.html#nie:hasPart) and its reverse
[nie:isPartOf](nie-ontology.html#nie:isPartOf).

![](containment.svg)

These properties (or their subproperties with a more specific semantic meaning) provide
the scaffolding to give an uniform view of the data with an arbitrary level of detail.
For a more thorough example, the figure below represents an image in an archive in the
attachment of a PDF document in the filesystem:

![](example-interpretation-containment.svg)

The horizontal edges express interpretation, the diagonal edges express containment.
This approach gives a uniform overview of data regardless of how it's represented.

## Common properties

Given that the classes defined in this ontology are the superclasses for almost
everything in the Nepomuk set of ontologies, the
properties defined here will be inherited for a lot of classes. It is
worth to comment few of them with special relevance:

 - [nie:title](nie-ontology.html#nie:title): Title or name or short text describing the item
 - [nie:description](nie-ontology.html#nie:description): More verbose comment about the element
 - [nie:language](nie-ontology.html#nie:language): To specify the language of the item.
 - [nie:plainTextContent](nie-ontology.html#nie:plainTextContent): Just the raw content of the file, if it makes sense as text.
 - [nie:generator](nie-ontology.html#nie:generator): Software/Agent that set/produced the information.
 - [nie:usageCounter](nie-ontology.html#nie:usageCounter): Count number of accesses to the information. It can be an indicator of relevance for advanced searches

## Date and timestamp representations

There are few important dates for the life-cycle of a resource. These dates are properties of the nie:InformationElement class, and inherited for its subclasses:

 - [nie:informationElementDate](nie-ontology.html#nie:informationElementDate): This is an ''abstract'' property that act as superproperty of the other dates. Don't use it directly.
 - [nie:contentLastModified](nie-ontology.html#nie:contentLastModified): Modification time of a resource. Usually the mtime of a local file, or information from the server for online resources.
 - [nie:contentCreated](nie-ontology.html#nie:contentCreated): Creation time of the content. If the contents is created by an application, the same application should set the value of this property. Note that this property can be undefined for resources in the filesystem because the creation time is not available in the most common filesystem formats.
 - [nie:contentAccessed](nie-ontology.html#nie:contentAccessed): For resources coming from the filesystem, this is the usual access time to the file. For other  kind of resources (online or virtual), the application accessing it should update its value.
 - [nie:lastRefreshed](nie-ontology.html#nie:lastRefreshed): The time that the content was last refreshed. Usually for remote resources.

## URIs and full representation of a file

One of the most common resources in a desktop is a file. Given the split between Data Objects and Information Elements, some times it is not clear how a real file is represented into Nepomuk. Here are some indications:

 1. Every file (local or remote) should generate one DataObject instance and an InformationElement instance.
 2. Even when Data Objects and Information Elements are different entities.
 3. The URI of the DataObject is the real location of the item (e.g. ''file://path/to/file.mp3'')
 3. The URI of the InformationElement(s) will be generated IDs.
 4. Every DataObject must have the property [nie:url](nie-ontology.html#nie:url), that points to the location of the resource, and should be used by any program that wants to access it.
 5. The InformationElement and DataObject are related via the [nie:isStoredAs](nie-ontology.html#nie:isStoredAs) / [nie:interpretedAs](nie-ontology.html#nie:interpretedAs) properties.

Here comes an example, for the image file `/home/user/a.jpeg`:

```turtle
# Properties as nmm:Photo
<urn:uuid:10293801928301293> a nmm:Photo ;
  nie:isStoredAs <file:///home/user/a.jpeg> ;
  nfo:width 49 ;
  nfo:height 36 ;
  nmm:flash nmm:flash-off;
  nmm:whiteBalance nmm:white-balance-automatic ;
  nfo:equipment [
    a nfo:Equipment ;
    nfo:make 'Nokia';
    nfo:model 'N900';
    nfo:equipmentSoftware 'Tracknon'
  ] .

# Properties from nfo:FileDataObject
<file:///home/user/a.jpeg> a nfo:FileDataObject ;
  nie:interpretedAs <urn:uuid:10293801928301293> ;
  nfo:fileSize 12341234 ;
  nie:url 'file:///home/user/a.jpeg' .
```
