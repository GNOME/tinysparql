/*
 * Copyright (C) 2011, 2014 Red Hat, Inc
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
 *
 * Authors: Christophe Fergeau <cfergeau@redhat.com>
 *          Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#include "config.h"

#include <stdio.h>

#include <osinfo/osinfo.h>

#include <gio/gio.h>

#include <libtracker-extract/tracker-extract.h>
#include <libtracker-sparql/tracker-sparql.h>

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info_)
{
	/* NOTE: This function has to exist, tracker-extract checks
	 * the symbole table for this function and if it doesn't
	 * exist, the module is not loaded to be used as an extractor.
	 */

	/* File information */
	GFile *file;
	GError *error = NULL;
	gchar *filename;
	OsinfoLoader *loader = NULL;
	OsinfoMedia *media;
	OsinfoDb *db;
	OsinfoOs *os;
	OsinfoOsVariantList *variants;

	/* Data input */
	gboolean bootable;
	const gchar *id;
	const gchar *name;
        GList *languages, *l;
	TrackerSparqlBuilder *metadata;

	metadata = tracker_extract_info_get_metadata_builder (info_);

	file = tracker_extract_info_get_file (info_);
	filename = g_file_get_path (file);

	media = osinfo_media_create_from_location (filename, NULL, &error);
	if (error != NULL) {
		if (error->code != OSINFO_MEDIA_ERROR_NOT_BOOTABLE) {
			g_message ("Could not extract iso info from '%s': %s",
				   filename, error->message);
			g_free (filename);
			g_error_free (error);
			return FALSE;
		}
		bootable = FALSE;
		goto no_os;
	} else {
		bootable = TRUE;
	}
	g_free (filename);

	loader = osinfo_loader_new ();
	osinfo_loader_process_default_path (loader, &error);
	if (error != NULL) {
		g_message ("Error loading libosinfo OS data: %s",
			   error->message);
		g_error_free (error);
		goto no_os;
	}
	g_warn_if_fail (media != NULL);
	g_warn_if_fail (loader != NULL);

	db = osinfo_loader_get_db (loader);
	osinfo_db_identify_media (db, media);
	os = osinfo_media_get_os (media);

	if (os == NULL)
		goto unknown_os;

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:FilesystemImage");

	variants = osinfo_media_get_os_variants (media);
	if (osinfo_list_get_length (OSINFO_LIST (variants)) > 0) {
		OsinfoEntity *variant;

		/* FIXME: Assuming first variant from multivariant medias. */
		variant = osinfo_list_get_nth (OSINFO_LIST (variants), 0);
		name = osinfo_os_variant_get_name (OSINFO_OS_VARIANT (variant));
	} else {
		name = osinfo_product_get_name (OSINFO_PRODUCT (os));
	}

	if (name != NULL) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_string (metadata, name);
	}

	if (osinfo_media_get_live (media)) {
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nfo:OperatingSystem");
	}

	if (osinfo_media_get_installer (media)) {
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "osinfo:Installer");
	}

	tracker_sparql_builder_predicate (metadata, "nfo:isBootable");
	tracker_sparql_builder_object_boolean (metadata, bootable);

	id = osinfo_entity_get_id (OSINFO_ENTITY (os));
	if (id != NULL) {
		tracker_sparql_builder_predicate (metadata, "osinfo:id");
		tracker_sparql_builder_object_string (metadata, id);
	}

        id = osinfo_entity_get_id (OSINFO_ENTITY (media));
	if (id != NULL) {
		tracker_sparql_builder_predicate (metadata, "osinfo:mediaId");
		tracker_sparql_builder_object_string (metadata, id);
	}

        languages = osinfo_media_get_languages (media);
        for (l = languages; l != NULL; l = l->next) {
		tracker_sparql_builder_predicate (metadata, "osinfo:language");
		tracker_sparql_builder_object_string (metadata, (char *) l->data);
        }
        g_list_free (languages);

	g_object_unref (G_OBJECT (media));
	g_object_unref (G_OBJECT (loader));

	return TRUE;

unknown_os:
        name = osinfo_media_get_volume_id (media);
	if (name != NULL) {
                gchar *stripped = g_strdup (name);

                g_strstrip (stripped);
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_string (metadata, stripped);
                g_free (stripped);
	}

no_os:
	if (media != NULL) {
		g_object_unref (G_OBJECT (media));
	}
	if (loader != NULL) {
		g_object_unref (G_OBJECT (loader));
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:FilesystemImage");

	tracker_sparql_builder_predicate (metadata, "nfo:isBootable");
	tracker_sparql_builder_object_boolean (metadata, bootable);

	return TRUE;
}
