/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
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

#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-ontology.h>
#include "tracker-miner-applications.h"

#define RDF_TYPE 	TRACKER_RDF_PREFIX "type"
#define NFO_PREFIX	TRACKER_NFO_PREFIX
#define NIE_PREFIX	TRACKER_NIE_PREFIX
#define MAEMO_PREFIX	TRACKER_MAEMO_PREFIX

#define GROUP_DESKTOP_ENTRY "Desktop Entry"

#define APPLICATION_DATASOURCE_URN	    "urn:nepomuk:datasource:84f20000-1241-11de-8c30-0800200c9a66"
#define APPLET_DATASOURCE_URN		    "urn:nepomuk:datasource:192bd060-1f9a-11de-8c30-0800200c9a66"
#define SOFTWARE_CATEGORY_URN_PREFIX	    "urn:software-category:"
#define THEME_ICON_URN_PREFIX		    "urn:theme-icon:"

static void tracker_miner_applications_finalize (GObject *object);

static gboolean tracker_miner_applications_check_file      (TrackerMinerProcess  *miner,
							    GFile                *file);
static gboolean tracker_miner_applications_check_directory (TrackerMinerProcess  *miner,
							    GFile                *file);
static gboolean tracker_miner_applications_process_file    (TrackerMinerProcess  *miner,
							    GFile                *file,
							    TrackerSparqlBuilder *sparql);
static gboolean tracker_miner_applications_monitor_directory (TrackerMinerProcess  *miner,
							      GFile                *file);

G_DEFINE_TYPE (TrackerMinerApplications, tracker_miner_applications, TRACKER_TYPE_MINER_PROCESS)


static void
tracker_miner_applications_class_init (TrackerMinerApplicationsClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerMinerProcessClass *miner_process_class = TRACKER_MINER_PROCESS_CLASS (klass);

        object_class->finalize = tracker_miner_applications_finalize;

	miner_process_class->check_file = tracker_miner_applications_check_file;
	miner_process_class->check_directory = tracker_miner_applications_check_directory;
	miner_process_class->monitor_directory = tracker_miner_applications_monitor_directory;
        miner_process_class->process_file = tracker_miner_applications_process_file;
}

static void
tracker_miner_applications_init (TrackerMinerApplications *miner)
{
}

static void
tracker_miner_applications_finalize (GObject *object)
{
        G_OBJECT_CLASS (tracker_miner_applications_parent_class)->finalize (object);
}

static void
insert_data_from_desktop_file (TrackerSparqlBuilder *sparql,
			       const gchar          *subject,
			       const gchar          *metadata_key,
			       GKeyFile             *desktop_file,
			       const gchar          *key,
			       gboolean		     use_locale)
{
	gchar *str;

	if (use_locale) {
		str = g_key_file_get_locale_string (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL, NULL);
	} else {
		str = g_key_file_get_string (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL);
	}

	if (str) {
		tracker_sparql_builder_subject_iri (sparql, subject);
		tracker_sparql_builder_predicate_iri (sparql, metadata_key);
		tracker_sparql_builder_object_string (sparql, str);
		g_free (str);
	}
}

static gboolean
tracker_miner_applications_check_file (TrackerMinerProcess *miner,
				       GFile               *file)
{
	gboolean retval = FALSE;
	gchar *basename;

	basename = g_file_get_basename (file);

	/* Check we're dealing with a desktop file */
	if (g_str_has_suffix (basename, ".desktop") ||
	    g_str_has_suffix (basename, ".directory")) {
		retval = TRUE;
	}

	g_free (basename);

	return retval;
}

static gboolean
tracker_miner_applications_check_directory (TrackerMinerProcess  *miner,
					    GFile                *file)
{
	/* We want to inspect all the passed dirs and their children */
	return TRUE;
}

static gboolean
tracker_miner_applications_monitor_directory (TrackerMinerProcess  *miner,
					      GFile                *file)
{
	/* We want to monitor all the passed dirs and their children */
	return TRUE;
}

static gboolean
tracker_miner_applications_process_file (TrackerMinerProcess  *miner,
					 GFile                *file,
					 TrackerSparqlBuilder *sparql)
{
	GKeyFile *key_file;
	gchar *path, *type, *filename, *name = NULL, *uri = NULL;
	GFileInfo *file_info;
	GStrv cats = NULL;
	gsize cats_len;

	path = g_file_get_path (file);
	key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL)) {
		g_debug ("Couldn't load desktop file:'%s'", path);
		g_key_file_free (key_file);
		g_free (path);
		return FALSE;
	}

	if (g_key_file_get_boolean (key_file, GROUP_DESKTOP_ENTRY, "Hidden", NULL)) {
		g_debug ("Desktop file is 'hidden', not gathering metadata for it");
		g_key_file_free (key_file);
		g_free (path);
		return FALSE;
	}

	type = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "Type", NULL);

	if (!type) {
		g_key_file_free (key_file);
		g_free (type);
		g_free (path);
		return FALSE;
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

		uri = canonical_uri;

		tracker_sparql_builder_subject_iri (sparql, uri);

		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:SoftwareCategory");

		tracker_sparql_builder_predicate (sparql, "nie:title");
		tracker_sparql_builder_object_string (sparql, name);

	} else if (name && g_ascii_strcasecmp (type, "Application") == 0) {

		uri = g_file_get_uri (file);
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

		/* The URI of the InformationElement should be a UUID URN */
		uri = g_file_get_uri (file);
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
		gchar *desktop_file_uri;

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

		tracker_sparql_builder_subject_iri (sparql, uri);
		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLICATION_DATASOURCE_URN);

		filename = g_filename_display_basename (path);
		tracker_sparql_builder_predicate (sparql, "nfo:fileName");
		tracker_sparql_builder_object_string (sparql, filename);
		g_free (filename);

		desktop_file_uri = g_file_get_uri (file);
		tracker_sparql_builder_subject_iri (sparql, desktop_file_uri);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:FileDataObject");

		tracker_sparql_builder_subject_iri (sparql, uri);
		tracker_sparql_builder_predicate (sparql, "nie:isStoredAs");
		tracker_sparql_builder_object_iri (sparql, desktop_file_uri);
		g_free (desktop_file_uri);

	}

	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_TIME_MODIFIED,
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				       NULL, NULL);

	if (file_info) {
		guint64 time;

		time = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		tracker_sparql_builder_predicate (sparql, "nfo:fileLastModified");
		tracker_sparql_builder_object_date (sparql, (time_t *) &time);

		g_object_unref (file_info);
	}


	if (cats)
		g_strfreev (cats);

	g_free (uri);
	g_key_file_free (key_file);
	g_free (type);
	g_free (path);
	g_free (name);

	return TRUE;
}

TrackerMiner *
tracker_miner_applications_new (void)
{
	return g_object_new (TRACKER_TYPE_MINER_APPLICATIONS,
			     "name", "Applications",
                             NULL);
}
