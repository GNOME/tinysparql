/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>

#include <glib.h>

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>

#include <tracker-miner-fs/tracker-module.h>

#define RDF_TYPE 	TRACKER_RDF_PREFIX "type"
#define NFO_PREFIX	TRACKER_NFO_PREFIX
#define NIE_PREFIX	TRACKER_NIE_PREFIX
#define MAEMO_PREFIX	TRACKER_MAEMO_PREFIX

#define GROUP_DESKTOP_ENTRY "Desktop Entry"

#define APPLICATION_DATASOURCE_URN	    "urn:nepomuk:datasource:84f20000-1241-11de-8c30-0800200c9a66"
#define APPLET_DATASOURCE_URN		    "urn:nepomuk:datasource:192bd060-1f9a-11de-8c30-0800200c9a66"
#define SOFTWARE_CATEGORY_URN_PREFIX	    "urn:software-category:"
#define THEME_ICON_URN_PREFIX		    "urn:theme-icon:"

/*
#define METADATA_FILE_NAME	  "File:Name"
#define METADATA_APP_NAME	  "App:Name"
#define METADATA_APP_DISPLAY_NAME "App:DisplayName"
#define METADATA_APP_GENERIC_NAME "App:GenericName"
#define METADATA_APP_COMMENT	  "App:Comment"
#define METADATA_APP_EXECUTABLE   "App:Exec"
#define METADATA_APP_ICON	  "App:Icon"
#define METADATA_APP_MIMETYPE	  "App:MimeType"
#define METADATA_APP_CATEGORIES   "App:Categories"
*/

#define TRACKER_TYPE_APPLICATION_FILE    (tracker_application_file_get_type ())
#define TRACKER_APPLICATION_FILE(module) (G_TYPE_CHECK_INSTANCE_CAST ((module), TRACKER_TYPE_APPLICATION_FILE, TrackerApplicationFile))

typedef struct TrackerApplicationFile TrackerApplicationFile;
typedef struct TrackerApplicationFileClass TrackerApplicationFileClass;

struct TrackerApplicationFile {
	TrackerModuleFile parent_instance;
};

struct TrackerApplicationFileClass {
	TrackerModuleFileClass parent_class;
};

static GType                   tracker_application_file_get_type      (void) G_GNUC_CONST;
static TrackerSparqlBuilder *  tracker_application_file_get_metadata  (TrackerModuleFile *file, gchar **mime_type);


G_DEFINE_DYNAMIC_TYPE (TrackerApplicationFile, tracker_application_file, TRACKER_TYPE_MODULE_FILE);


static void
tracker_application_file_class_init (TrackerApplicationFileClass *klass)
{
	TrackerModuleFileClass *file_class = TRACKER_MODULE_FILE_CLASS (klass);

	file_class->get_metadata = tracker_application_file_get_metadata;
}

static void
tracker_application_file_class_finalize (TrackerApplicationFileClass *klass)
{
}

static void
tracker_application_file_init (TrackerApplicationFile *file)
{
}

static void
insert_data_from_desktop_file (TrackerSparqlBuilder  *sparql,
			       const gchar           *subject,
			       const gchar           *metadata_key,
			       GKeyFile              *desktop_file,
			       const gchar           *key,
			       gboolean		      use_locale)
{
	gchar *str;

	if (use_locale) {
		str = g_key_file_get_locale_string (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL, NULL);
	} else {
		str = g_key_file_get_string (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL);
	}

	if (str) {
		tracker_sparql_builder_predicate_iri (sparql, metadata_key);
		tracker_sparql_builder_object_string (sparql, str);
		g_free (str);
	}
}


static TrackerSparqlBuilder *
tracker_application_file_get_metadata (TrackerModuleFile *file, gchar **mime_type)
{
	TrackerSparqlBuilder *sparql = NULL;
	GKeyFile *key_file;
	GFile *f;
	gchar *path, *type, *filename, *name = NULL, *uri = NULL;
	GStrv cats = NULL;
	gsize cats_len;

	f = tracker_module_file_get_file (file);
	path = g_file_get_path (f);

	/* Check we're dealing with a desktop file */
	if (!g_str_has_suffix (path, ".desktop") &&
	    !g_str_has_suffix (path, ".directory")) {
		g_free (path);
		return NULL;
	}

	key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL)) {
		g_debug ("Couldn't load desktop file:'%s'", path);
		g_key_file_free (key_file);
		g_free (path);
		return NULL;
	}

	if (g_key_file_get_boolean (key_file, GROUP_DESKTOP_ENTRY, "Hidden", NULL)) {
		g_debug ("Desktop file is 'hidden', not gathering metadata for it");
		g_key_file_free (key_file);
		g_free (path);
		return NULL;
	}

	type = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "Type", NULL);

	if (!type) {
		g_key_file_free (key_file);
		g_free (type);
		g_free (path);
		return NULL;
	}

	
	cats = g_key_file_get_locale_string_list (key_file, GROUP_DESKTOP_ENTRY, "Categories", NULL, &cats_len, NULL);

	if (!cats)
		cats = g_key_file_get_string_list (key_file, GROUP_DESKTOP_ENTRY, "Categories", &cats_len, NULL);

	name = g_key_file_get_locale_string (key_file, GROUP_DESKTOP_ENTRY, "Name", NULL, NULL);

	if (!name)
		g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "Name", NULL);

	if (name && g_ascii_strcasecmp (type, "Directory") == 0) {
		gchar *canonical_uri = tracker_uri_printf_escaped (SOFTWARE_CATEGORY_URN_PREFIX "%s", name);
		gchar *icon = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "Icon", NULL);

		uri = NULL;
		sparql = tracker_sparql_builder_new_update ();
		tracker_sparql_builder_insert_open (sparql);

		if (icon) {
			gchar *icon_uri = g_strdup_printf (THEME_ICON_URN_PREFIX "%s", icon);

			tracker_sparql_builder_subject_iri (sparql, icon_uri);
			tracker_sparql_builder_predicate (sparql, "a");
			tracker_sparql_builder_object (sparql, "nfo:Image");

			tracker_sparql_builder_subject_iri (sparql, canonical_uri);
			tracker_sparql_builder_predicate (sparql, "nfo:softwareCategoryIcon");
			tracker_sparql_builder_object_iri (sparql, icon_uri);

			g_free (icon_uri);
			g_free (icon);
		}

		tracker_sparql_builder_subject_iri (sparql, canonical_uri);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:SoftwareCategory");

		tracker_sparql_builder_predicate (sparql, "nie:title");
		tracker_sparql_builder_object_string (sparql, name);

		g_free (canonical_uri);

	} else if (name && g_ascii_strcasecmp (type, "Application") == 0) {

		uri = tracker_module_file_get_uri (file);
		sparql = tracker_sparql_builder_new_update ();
		tracker_sparql_builder_insert_open (sparql);

		tracker_sparql_builder_subject_iri (sparql, APPLICATION_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nie:DataSource");

		tracker_sparql_builder_subject_iri (sparql, uri);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:SoftwareApplication");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLICATION_DATASOURCE_URN);

	/* This matches SomeApplet as Type= */
	} else if (name && g_str_has_suffix (type, "Applet")) {

		uri = tracker_module_file_get_uri (file);
		sparql = tracker_sparql_builder_new_update ();
		tracker_sparql_builder_insert_open (sparql);

		tracker_sparql_builder_subject_iri (sparql, APPLET_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nie:DataSource");

		/* TODO This is atm specific for Maemo */
		tracker_sparql_builder_subject_iri (sparql, uri);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:SoftwareApplet");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLET_DATASOURCE_URN);
	}

	if (sparql && uri) {
		gchar *icon;

		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:Executable");
		tracker_sparql_builder_object (sparql, "nfo:FileDataObject");

		tracker_sparql_builder_predicate (sparql, "nie:title");
		tracker_sparql_builder_object_string (sparql, name);

		insert_data_from_desktop_file (sparql, uri, NIE_PREFIX "comment", key_file, "Comment", TRUE);
		insert_data_from_desktop_file (sparql, uri, NFO_PREFIX "softwareCmdLine", key_file, "Exec", TRUE);

		icon = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "Icon", NULL);

		if (icon) {
			gchar *icon_uri = g_strdup_printf (THEME_ICON_URN_PREFIX "%s", icon);

			tracker_sparql_builder_subject_iri (sparql, icon_uri);
			tracker_sparql_builder_predicate (sparql, "a");
			tracker_sparql_builder_object (sparql, "nfo:Image");

			tracker_sparql_builder_subject_iri (sparql, uri);
			tracker_sparql_builder_predicate (sparql, "nfo:softwareIcon");
			tracker_sparql_builder_object_iri (sparql, icon_uri);

			g_free (icon_uri);
			g_free (icon);
		}

		if (cats) {
			gsize i;

			for (i = 0 ; cats[i] && i < cats_len ; i++) {
				gchar *cat_uri = tracker_uri_printf_escaped (SOFTWARE_CATEGORY_URN_PREFIX "%s", cats[i]);

				/* There are also .desktop
				 * files that describe these categories, but we can handle 
				 * preemptively creating them if we visit a app .desktop
				 * file that mentions one that we don't yet know about */

				tracker_sparql_builder_subject_iri (sparql, cat_uri);
				tracker_sparql_builder_predicate (sparql, "a");
				tracker_sparql_builder_object (sparql, "nfo:SoftwareCategory");

				tracker_sparql_builder_predicate (sparql, "nie:title");
				tracker_sparql_builder_object_string (sparql, cats[i]);

				tracker_sparql_builder_subject_iri (sparql, uri);
				tracker_sparql_builder_predicate (sparql, "nfo:belongsToContainer");
				tracker_sparql_builder_object_iri (sparql, cat_uri);

				g_free (cat_uri);
			}
		}

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLICATION_DATASOURCE_URN);

		filename = g_filename_display_basename (path);
		tracker_sparql_builder_predicate (sparql, "nfo:fileName");
		tracker_sparql_builder_object_string (sparql, filename);
		g_free (filename);
	}

	if (cats)
		g_strfreev (cats);

	g_free (uri);
	g_key_file_free (key_file);
	g_free (type);
	g_free (path);
	g_free (name);

	return sparql;
}


void
indexer_module_initialize (GTypeModule *module)
{
	tracker_application_file_register_type (module);
}

void
indexer_module_shutdown (void)
{
}

TrackerModuleFile *
indexer_module_create_file (GFile *file)
{
	return g_object_new (TRACKER_TYPE_APPLICATION_FILE,
	                     "file", file,
	                     NULL);
}
