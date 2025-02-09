# Overview

## Introduction

This ontology extends NIE (Nepomuk Information Element ontology) and NFO (Nepomuk File Ontology) into the domains of multimedia, including Images, Videos, Music and Radio, and includes a couple of properties to support uPnP sharing.

This ontology replaces/complements NID3 and NEXIF. Those ontologies are a direct map of the respective metadata standards, which makes very difficult to map cleanly other standards (like gstreamer metadata, mp4, ogg, and so on). Besides, those ontologies contain a lot of very low level technical information that is useless to the average user of the desktop.

Our approach in NMM is to keep the minimum properties that make sense for the user, and are present in all (or almost all) specific metadata formats. A profesional photographer (or musician) who needs more fine-grained details for its documents, is free to add also nID3 information or other extensions to the ontology.

## Images domain

The core of images in NMM ontology is the class [nmm:Photo](nmm-ontology.html#nmm:Photo). It is (through a long hierarchy) a [nie:InformationElement](nie-ontology.html#nie:InformationElement), an interpretation of some bytes. It has properties to store the basic information (camera, metering mode, white balance, flash), and inherits from [nfo:Image](nfo-ontology.html#nfo:Image) orientation ([nfo:orientation](nfo-ontology.html#nfo:orientation)) and resolution ([nfo:verticalResolution](nfo-ontology.html#nfo:verticalResolution) and [nfo:horizontalResolution](nfo-ontology.html#nfo:horizontalResolution)).

Note that for tags, nie:keywords (from nie:InformationElement) can be used, or the [NAO](nao-ontology.html) ontology.

## Radio domain

NMM includes classes and properties to represent analog and digital radio stations. There is a class [nmm:RadioStation](nmm-ontology.html#nmm:RadioStation) on the [nie:InformationElement](nie-ontology.html#nie:InformationElement) side of the ontology, representing what the user sees about that station (genre via PTY codes, icon, plus title inherited from nie:InformationElement)

A [nmm:RadioStation](nmm-ontology.html#nmm:RadioStation) can have one or more [nmm:carrier](nmm-ontology.html#nmm:carrier) properties representing the different frequencies (or links when it is digitial) it can be tuned. This property links the station with [nfo:MediaStream](nfo-ontology.html#nfo:MediaStream), but usually it will point to one of the subclasses: [nmm:DigitalRadio](nmm-ontology.html#nmm:DigitalRadio) (if digital) or [nmm:AnalogRadio](nmm-ontology.html#nmm:AnalogRadio) (if analog). An analog station has properties as modulation and frequency, while the digial station has streaming bitrate, encoding or protocol.

Note that nfo:MediaStream refers to a flux of bytes/data, and it is on the [nie:DataObject](nie-ontology.html#nie:DataObject) side of the ontology.
