# Overview

## Introduction

This ontology is an abstract representation of entries coming from feeds. These feeds can be blogs (any of the common syndication formats can be translated into this representation), podcasts or even some online services (like flickr).

The basic assumption in the ontology is that all these feeds are unidirectional conversations with (from) the author of the content and every post on those channels is a message.

The source of the posts, the feed itself, is an instance of [mfo:FeedChannel](mfo-ontology.md#mfo:FeedChannel). Each post in that feed will be an instance of [mfo:FeedMessage](mfo-ontology.md#mfo:FeedMessage). The relation between the messages and the channel comes from their superclasses, [nmo:communicationChannel](nmo-ontology.md#nmo:communicationChannel) (taking into account that [mfo:FeedChannel](mfo-ontology.md#mfo:FeedChannel) is a subclass of [nmo:CommunicationChannel](nmo-ontology.md#nmo:CommunicationChannel) and [mfo:FeedMessage](mfo-ontology.md#mfo:FeedMessage) a subclass of [nmo:Message](nmo-ontology.md#nmo:Message).

A post can be plain text but can contain also more things like links, videos or Mp3. We represent those internal pieces in instances of [mfo:Enclosure](mfo-ontology.md#mfo:Enclosure). This class has properties to link with the remote and local representation of the resource (in case the content has been downloaded).

Finally, the three important classes (mfo:FeedChannel, mfo:FeedMessage, mfo:Enclosure) are subclasses of [mfo:FeedElement](mfo-ontology.md#mfo:FeedElement), just an abstract class to share the link with mfo:FeedSettings. [mfo:FeedSettings](mfo-ontology.md#mfo:FeedSettings) contains some common configuration options. Not all of them applies to all, but it is a quite cleaner solution. For instance the [mfo:maxSize](mfo-ontology.md#mfo:maxSize) property only makes sense per-enclosure, while the [mfo:updateInterval](mfo-ontology.md#mfo:updateInterval) is useful for the channel.

## Special remarks

In some feeds there can be multiple enclosures together in a group, representing the same resource in different formats, qualities, resolutions, etc. Until further notify, the group will be represented using [nie:identifier](nie-ontology.md#nie:identifier) property. To mark the default enclosure of the group, there is a [mfo:groupDefault](mfo-ontology.md#mfo:groupDefault) property.
