/*
 * Copyright (C) 2016, Sam Thursfield <sam@afuera.me.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "tracker-resource-helpers.h"
#include "tracker-guarantee.h"

/**
 * SECTION:tracker-resource-helpers
 * @title: Resource Helpers
 * @short_description: Helpers for describing certain common kinds of resource.
 * @stability: Stable
 * @include: libtracker-extract/tracker-extract.h
 *
 * Some common patterns for resources used in Tracker extractors have helper
 * functions defined in this module.
 */

/**
 * tracker_extract_new_music_artist:
 * @name: the name of the artist
 *
 * Create a new nmm:Artist resource. The URI will be set based on the URI, so
 * there will only be one resource representing the artist in the Tracker store.
 *
 * Returns: a newly allocated #TrackerResource instance, of type nmm:Artist.
 *
 * Since: 1.10
 */
TrackerResource *
tracker_extract_new_artist (const char *name)
{
	TrackerResource *artist;
	gchar *uri;

	g_return_val_if_fail (name != NULL, NULL);

	uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", name);

	artist = tracker_resource_new (uri);

	tracker_resource_set_uri (artist, "rdf:type", "nmm:Artist");
	tracker_resource_set_string (artist, "nmm:artistName", name);

	g_free (uri);

	return artist;
}

/**
 * tracker_extract_new_contact:
 * @fullname: the full name of the contact
 *
 * Create a new nco:Contact resource. The URI is based on @fullname, so only
 * one instance will exist in the Tracker store.
 *
 * This is used for describing people or organisations, and can be used with
 * many fields including nco:artist, nco:creator, nco:publisher, and
 * nco:representative.
 *
 * Returns: a newly allocated #TrackerResource instance, of type nco:Contact
 *
 * Since: 1.10
 */
TrackerResource *
tracker_extract_new_contact (const char *fullname)
{
	TrackerResource *publisher;
	gchar *uri;

	g_return_val_if_fail (fullname != NULL, NULL);

	uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", fullname);

	publisher = tracker_resource_new (uri);

	tracker_resource_set_uri (publisher, "rdf:type", "nco:Contact");
	tracker_guarantee_resource_utf8_string (publisher, "nco:fullname", fullname);

	g_free (uri);

	return publisher;
}

/**
 * tracker_extract_new_equipment:
 * @make: (allow none): the manufacturer of the equipment, or %NULL
 * @model: (allow none): the model name of the equipment, or %NULL
 *
 * Create a new nfo:Equipment resource. The URI is based on @make and @model,
 * so only one instance will exist in the Tracker store. At least one of @make
 * and @model must be non-%NULL.
 *
 * This is useful for describing equipment used to create something, for
 * example the camera that was used to take a photograph.
 *
 * Returns: a newly allocated #TrackerResource instance, of type nfo:Equipment
 *
 * Since: 1.10
 */
TrackerResource *
tracker_extract_new_equipment (const char *make,
                               const char *model)
{
	TrackerResource *equipment;
	gchar *equip_uri;

	g_return_val_if_fail (make != NULL || model != NULL, NULL);

	equip_uri = tracker_sparql_escape_uri_printf ("urn:equipment:%s:%s:", make ? make : "", model ? model : "");

	equipment = tracker_resource_new (equip_uri);
	tracker_resource_set_uri (equipment, "rdf:type", "nfo:Equipment");

	if (make) {
		tracker_resource_set_string (equipment, "nfo:manufacturer", make);
	}

	if (model) {
		tracker_resource_set_string (equipment, "nfo:model", model);
	}

	g_free (equip_uri);

	return equipment;
}

/**
 * tracker_extract_new_location:
 * @street_address: (allow none): main part of postal address, or %NULL
 * @state: (allow none): regional part of postal address, or %NULL
 * @city: (allow none): locality part of postal address, or %NULL
 * @country: (allow none): country of postal address, or %NULL
 * @gps_altitude: (allow none): altitude (following WGS 84 reference) as a string, or %NULL
 * @gps_latitude: (allow none): latitude as a string, or %NULL
 * @gps_longitude: (allow none): longitude as a string, or %NULL
 *
 * Create a new slo:GeoLocation resource, with the given postal address and/or
 * GPS coordinates.
 *
 * No validation is done here -- it's up to you to ensure the postal address
 * and GPS coordinates describe the same thing.
 *
 * Returns: a newly allocated #TrackerResource instance, of type slo:GeoLocation
 *
 * Since: 1.10
 */
TrackerResource *
tracker_extract_new_location (const char *street_address,
                              const char *state,
                              const char *city,
                              const char *country,
                              const char *gps_altitude,
                              const char *gps_latitude,
                              const char *gps_longitude)
{
	TrackerResource *location;

	g_return_val_if_fail (street_address != NULL || state != NULL || city != NULL ||
	                      country != NULL || gps_altitude != NULL ||
	                      gps_latitude != NULL || gps_longitude != NULL, NULL);

	location = tracker_resource_new (NULL);
	tracker_resource_set_uri (location, "rdf:type", "slo:GeoLocation");

	if (street_address || state || country || city) {
		TrackerResource *address;
		gchar *addruri;

		addruri = tracker_sparql_get_uuid_urn ();
		address = tracker_resource_new (addruri);

		tracker_resource_set_string (address, "rdf:type", "nco:PostalAddress");

		g_free (addruri);

		if (address) {
			tracker_resource_set_string (address, "nco:streetAddress", street_address);
		}

		if (state) {
			tracker_resource_set_string (address, "nco:region", state);
		}

		if (city) {
			tracker_resource_set_string (address, "nco:locality", city);
		}

		if (country) {
			tracker_resource_set_string (address, "nco:country", country);
		}

		tracker_resource_set_relation (location, "slo:postalAddress", address);
		g_object_unref (address);
	}

	if (gps_altitude) {
		tracker_resource_set_string (location, "slo:altitude", gps_altitude);
	}

	if (gps_latitude) {
		tracker_resource_set_string (location, "slo:latitude", gps_latitude);
	}

	if (gps_longitude) {
		tracker_resource_set_string (location, "slo:longitude", gps_longitude);
	}

	return location;
}

/**
 * tracker_extract_new_music_album_disc:
 * @album_title: title of the album
 * @album_artist: (allow none): a #TrackerResource for the album artist, or %NULL
 * @disc_number: disc number of this disc (the first / only disc in a set should be 1, not 0)
 * @date: (allow none):The release date, or %NULL
 *
 * Create new nmm:MusicAlbumDisc and nmm:MusicAlbum resources. The resources are
 * given fixed URIs based on @album_title and @disc_number, so they will be
 * merged with existing entries when serialized to SPARQL and sent to the
 * Tracker store.
 *
 * You can get the album resource from the disc resource by calling:
 *
 *     tracker_resource_get_first_relation (album_disc, "nmm:albumDiscAlbum");
 *
 * Returns: a newly allocated #TrackerResource instance, of type nmm:MusicAlbumDisc
 *
 * Since: 1.10
 */
TrackerResource *
tracker_extract_new_music_album_disc (const char      *album_title,
                                      TrackerResource *album_artist,
                                      int              disc_number,
                                      const char      *date)
{
	GString *album_uri, *disc_uri;
	const gchar *album_artist_name = NULL;
	gchar *tmp_album_uri, *tmp_disc_uri;

	TrackerResource *album, *album_disc;

	g_return_val_if_fail (album_title != NULL, NULL);

	if (album_artist)
		album_artist_name = tracker_resource_get_first_string (album_artist,
		                                                       "nmm:artistName");

	album_uri = g_string_new ("urn:album:");

	g_string_append (album_uri, album_title);

	if (album_artist_name)
		g_string_append_printf (album_uri, ":%s", album_artist_name);

	if (date)
		g_string_append_printf (album_uri, ":%s", date);

	tmp_album_uri = tracker_sparql_escape_uri (album_uri->str);
	album = tracker_resource_new (tmp_album_uri);

	tracker_resource_set_uri (album, "rdf:type", "nmm:MusicAlbum");
	tracker_resource_set_string (album, "nie:title", album_title);

	if (album_artist)
		tracker_resource_add_relation (album, "nmm:albumArtist", album_artist);

	disc_uri = g_string_new ("urn:album-disc:");

	g_string_append (disc_uri, album_title);

	if (album_artist_name)
		g_string_append_printf (disc_uri, ":%s", album_artist_name);

	if (date)
		g_string_append_printf (disc_uri, ":%s", date);

	g_string_append_printf (disc_uri, ":Disc%d", disc_number);

	tmp_disc_uri = tracker_sparql_escape_uri (disc_uri->str);
	album_disc = tracker_resource_new (tmp_disc_uri);

	tracker_resource_set_uri (album_disc, "rdf:type", "nmm:MusicAlbumDisc");
	tracker_resource_set_int (album_disc, "nmm:setNumber", disc_number > 0 ? disc_number : 1);
	tracker_resource_add_relation (album_disc, "nmm:albumDiscAlbum", album);

	g_free (tmp_album_uri);
	g_free (tmp_disc_uri);
	g_string_free (album_uri, TRUE);
	g_string_free (disc_uri, TRUE);

	g_object_unref (album);

	return album_disc;
}

/**
 * tracker_extract_new_tag:
 * @label: the label of the tag
 *
 * Create a new nao:Tag resource. The URI will be set based on the tag label,
 * so there will only be one resource representing the tag in the Tracker store.
 *
 * Returns: a newly allocated #TrackerResource instance, of type nao:Tag.
 *
 * Since: 1.10
 */
TrackerResource *
tracker_extract_new_tag (const char *label)
{
	TrackerResource *tag;
	char *uri;

	uri = tracker_sparql_escape_uri_printf ("urn:tag:%s", label);
	tag = tracker_resource_new (uri);

	tracker_resource_set_uri (tag, "rdf:type", "nao:Tag");
	tracker_guarantee_resource_utf8_string (tag, "nao:prefLabel", label);

	g_free (uri);
	return tag;
}
