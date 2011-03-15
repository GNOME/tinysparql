/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-common/tracker-locale.h>

#include "tracker-miner-applications.h"
#include "tracker-miner-applications-locale.h"

#ifdef HAVE_MEEGOTOUCH
#include "tracker-miner-applications-meego.h"
#endif

#define GROUP_DESKTOP_ENTRY          "Desktop Entry"

#define APPLICATION_DATASOURCE_URN   "urn:nepomuk:datasource:84f20000-1241-11de-8c30-0800200c9a66"
#define APPLET_DATASOURCE_URN        "urn:nepomuk:datasource:192bd060-1f9a-11de-8c30-0800200c9a66"
#define SOFTWARE_CATEGORY_URN_PREFIX "urn:software-category:"
#define THEME_ICON_URN_PREFIX        "urn:theme-icon:"

static void     miner_applications_initable_iface_init     (GInitableIface       *iface);
static gboolean miner_applications_initable_init           (GInitable            *initable,
                                                            GCancellable         *cancellable,
                                                            GError              **error);
static gboolean miner_applications_check_file              (TrackerMinerFS       *fs,
                                                            GFile                *file);
static gboolean miner_applications_check_directory         (TrackerMinerFS       *fs,
                                                            GFile                *file);
static gboolean miner_applications_process_file            (TrackerMinerFS       *fs,
                                                            GFile                *file,
                                                            TrackerSparqlBuilder *sparql,
                                                            GCancellable         *cancellable);
static gboolean miner_applications_process_file_attributes (TrackerMinerFS       *fs,
                                                            GFile                *file,
                                                            TrackerSparqlBuilder *sparql,
                                                            GCancellable         *cancellable);
static gboolean miner_applications_monitor_directory       (TrackerMinerFS       *fs,
                                                            GFile                *file);
static void     miner_applications_finalize                (GObject              *object);


static GQuark miner_applications_error_quark = 0;

typedef struct ProcessApplicationData ProcessApplicationData;

struct ProcessApplicationData {
	TrackerMinerFS *miner;
	GFile *file;
	TrackerSparqlBuilder *sparql;
	GCancellable *cancellable;
	GKeyFile *key_file;
	gchar *type;
};

static GInitableIface* miner_applications_initable_parent_iface;

G_DEFINE_TYPE_WITH_CODE (TrackerMinerApplications, tracker_miner_applications, TRACKER_TYPE_MINER_FS,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                miner_applications_initable_iface_init));

static void
tracker_miner_applications_class_init (TrackerMinerApplicationsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerFSClass *miner_fs_class = TRACKER_MINER_FS_CLASS (klass);

	object_class->finalize = miner_applications_finalize;

	miner_fs_class->check_file = miner_applications_check_file;
	miner_fs_class->check_directory = miner_applications_check_directory;
	miner_fs_class->monitor_directory = miner_applications_monitor_directory;
	miner_fs_class->process_file = miner_applications_process_file;
	miner_fs_class->process_file_attributes = miner_applications_process_file_attributes;

	miner_applications_error_quark = g_quark_from_static_string ("TrackerMinerApplications");
}

static void
tracker_miner_applications_init (TrackerMinerApplications *ma)
{
}

static void
miner_applications_initable_iface_init (GInitableIface *iface)
{
	miner_applications_initable_parent_iface = g_type_interface_peek_parent (iface);
	iface->init = miner_applications_initable_init;
}

static gboolean
miner_applications_initable_init (GInitable     *initable,
                                  GCancellable  *cancellable,
                                  GError       **error)
{
	TrackerMinerApplications *ma;
	TrackerMinerFS *fs;
	GFile *file;
	GError *inner_error = NULL;

	ma = TRACKER_MINER_APPLICATIONS (initable);
	fs = TRACKER_MINER_FS (initable);

	/* Chain up parent's initable callback before calling child's one */
	if (!miner_applications_initable_parent_iface->init (initable, cancellable, &inner_error)) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	file = g_file_new_for_path ("/usr/share/applications/");
	tracker_miner_fs_directory_add (fs, file, TRUE);
	g_object_unref (file);

	file = g_file_new_for_path ("/usr/share/desktop-directories/");
	tracker_miner_fs_directory_add (fs, file, TRUE);
	g_object_unref (file);

#ifdef HAVE_MEEGOTOUCH
	file = g_file_new_for_path ("/usr/lib/duicontrolpanel/");
	tracker_miner_fs_directory_add (fs, file, TRUE);
	g_object_unref (file);
	tracker_miner_applications_meego_init ();
#endif /* HAVE_MEEGOTOUCH */

	/* FIXME: Check XDG_DATA_DIRS and also process applications in there */

	return TRUE;
}

static void
miner_applications_finalize (GObject *object)
{
#ifdef HAVE_MEEGOTOUCH
	tracker_miner_applications_meego_shutdown ();
#endif /* HAVE_MEEGOTOUCH */

	G_OBJECT_CLASS (tracker_miner_applications_parent_class)->finalize (object);
}

static void
insert_data_from_desktop_file (TrackerSparqlBuilder *sparql,
                               const gchar          *subject,
                               const gchar          *metadata_key,
                               GKeyFile             *desktop_file,
                               const gchar          *key,
                               const gchar          *locale)
{
	gchar *str;

	if (locale) {
		/* Try to get the key with our desired LANG locale... */
		str = g_key_file_get_locale_string (desktop_file, GROUP_DESKTOP_ENTRY, key, locale, NULL);
		/* If our desired locale failed, use the list of LANG locales prepared by GLib
		 * (will return untranslated string if none of the locales available) */
		if (!str) {
			str = g_key_file_get_locale_string (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL, NULL);
		}
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
miner_applications_check_file (TrackerMinerFS *fs,
                               GFile          *file)
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
miner_applications_check_directory (TrackerMinerFS *fs,
                                    GFile          *file)
{
	/* We want to inspect all the passed dirs and their children */
	return TRUE;
}

static gboolean
miner_applications_monitor_directory (TrackerMinerFS *fs,
                                      GFile          *file)
{
	/* We want to monitor all the passed dirs and their children */
	return TRUE;
}

static GKeyFile *
get_desktop_key_file (GFile   *file,
                      gchar  **type,
                      GError **error)
{
	GKeyFile *key_file;
	gchar *path;
	gchar *str;

	path = g_file_get_path (file);
	key_file = g_key_file_new ();
	*type = NULL;

	if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL)) {
		g_set_error (error, miner_applications_error_quark, 0, "Couldn't load desktop file:'%s'", path);
		g_key_file_free (key_file);
		g_free (path);
		return NULL;
	}

	if (g_key_file_get_boolean (key_file, GROUP_DESKTOP_ENTRY, "Hidden", NULL)) {
		g_set_error_literal (error, miner_applications_error_quark, 0, "Desktop file is 'hidden', not gathering metadata for it");
		g_key_file_free (key_file);
		g_free (path);
		return NULL;
	}

	str = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "Type", NULL);

	if (G_UNLIKELY (!str)) {
		*type = NULL;

		g_set_error_literal (error, miner_applications_error_quark, 0, "Desktop file doesn't contain type");
		g_key_file_free (key_file);
		g_free (path);
		return NULL;
	} else {
		/* Sanitize type */
		*type = g_strstrip (str);
	}

	g_free (path);

	return key_file;
}

static void
process_directory (ProcessApplicationData  *data,
                   GFileInfo               *file_info,
                   GError                 **error)
{
	TrackerSparqlBuilder *sparql;
	gchar *urn, *path, *uri;

	sparql = data->sparql;

	path = g_file_get_path (data->file);
	uri = g_file_get_uri (data->file);
	urn = tracker_sparql_escape_uri_printf ("urn:applications-dir:%s", path);

	tracker_sparql_builder_insert_silent_open (sparql, TRACKER_MINER_FS_GRAPH_URN);

	tracker_sparql_builder_subject_iri (sparql, urn);

	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:FileDataObject");
	tracker_sparql_builder_object (sparql, "nie:DataObject");
	tracker_sparql_builder_object (sparql, "nie:Folder");

	tracker_sparql_builder_predicate (sparql, "tracker:available");
	tracker_sparql_builder_object_boolean (sparql, TRUE);

	tracker_sparql_builder_predicate (sparql, "nie:isStoredAs");
	tracker_sparql_builder_object_iri (sparql, urn);

	tracker_sparql_builder_predicate (sparql, "nie:url");
	tracker_sparql_builder_object_string (sparql, uri);

	if (file_info) {
		guint64 time;

		time = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		tracker_sparql_builder_predicate (sparql, "nfo:fileLastModified");
		tracker_sparql_builder_object_date (sparql, (time_t *) &time);
	}

	tracker_sparql_builder_insert_close (data->sparql);

	g_free (path);
	g_free (urn);
	g_free (uri);
}

static void
process_desktop_file (ProcessApplicationData  *data,
                      GFileInfo               *file_info,
                      GError                 **error)
{
	TrackerSparqlBuilder *sparql;
	GKeyFile *key_file;
	gchar *name = NULL;
	gchar *path;
	gchar *type;
	gchar *filename;
	gchar *uri = NULL;
	GStrv cats;
	gsize cats_len;
	gboolean is_software = TRUE;
	const gchar *parent_urn;
	gchar *lang;
#ifdef HAVE_MEEGOTOUCH
	gchar *logical_id = NULL;
	gchar *translation_catalog = NULL;
#endif /* HAVE_MEEGOTOUCH */

	sparql = data->sparql;
	key_file = data->key_file;
	type = data->type;

	path = g_file_get_path (data->file);

	/* Retrieve LANG locale setup */
	lang = tracker_locale_get (TRACKER_LOCALE_LANGUAGE);

	/* Try to get the categories with our desired LANG locale... */
	cats = g_key_file_get_locale_string_list (key_file, GROUP_DESKTOP_ENTRY, "Categories", lang, &cats_len, NULL);
	if (!cats) {
		/* If our desired locale failed, use the list of LANG locales prepared by GLib
		 * (will return untranslated string if none of the locales available) */
		cats = g_key_file_get_locale_string_list (key_file, GROUP_DESKTOP_ENTRY, "Categories", NULL, &cats_len, NULL);
	}

	/* NOTE: We sanitize categories later on when iterating them */

#ifdef HAVE_MEEGOTOUCH
	/* If defined, start with the logical strings */
	logical_id = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "X-MeeGo-Logical-Id", NULL);
	translation_catalog = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "X-MeeGo-Translation-Catalog", NULL);

	if (logical_id && translation_catalog) {
		name = tracker_miner_applications_meego_translate (translation_catalog, logical_id);
	}

	g_free (logical_id);
	g_free (translation_catalog);
#endif /* HAVE_MEEGOTOUCH */

	if (!name) {
		/* Try to get the name with our desired LANG locale... */
		name = g_key_file_get_locale_string (key_file, GROUP_DESKTOP_ENTRY, "Name", lang, NULL);
		if (!name) {
			/* If our desired locale failed, use the list of LANG locales prepared by GLib
			 * (will return untranslated string if none of the locales available) */
			name = g_key_file_get_locale_string (key_file, GROUP_DESKTOP_ENTRY, "Name", NULL, NULL);
		}
	}

	/* Sanitize name */
	if (name) {
		g_strstrip (name);
	}

	if (name && g_ascii_strcasecmp (type, "Directory") == 0) {
		gchar *canonical_uri = tracker_sparql_escape_uri_printf (SOFTWARE_CATEGORY_URN_PREFIX "%s", path);
		gchar *icon = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "Icon", NULL);

		uri = canonical_uri;
		tracker_sparql_builder_insert_silent_open (sparql, TRACKER_MINER_FS_GRAPH_URN);
		tracker_sparql_builder_subject_iri (sparql, uri);

		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:SoftwareCategory");

		if (icon) {
			gchar *icon_uri;

			/* Sanitize icon */
			g_strstrip (icon);

			icon_uri = g_strdup_printf (THEME_ICON_URN_PREFIX "%s", icon);

			tracker_sparql_builder_subject_iri (sparql, icon_uri);
			tracker_sparql_builder_predicate (sparql, "a");
			tracker_sparql_builder_object (sparql, "nfo:Image");

			tracker_sparql_builder_subject_iri (sparql, uri);
			tracker_sparql_builder_predicate (sparql, "nfo:softwareCategoryIcon");
			tracker_sparql_builder_object_iri (sparql, icon_uri);

			g_free (icon_uri);
			g_free (icon);
		}

		is_software = FALSE;
	} else if (name && g_ascii_strcasecmp (type, "Application") == 0) {
		uri = g_file_get_uri (data->file);
		tracker_sparql_builder_insert_silent_open (sparql, TRACKER_MINER_FS_GRAPH_URN);

		tracker_sparql_builder_subject_iri (sparql, APPLICATION_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nie:DataSource");

		tracker_sparql_builder_subject_iri (sparql, uri);

		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:SoftwareApplication");
		tracker_sparql_builder_object (sparql, "nie:DataObject");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLICATION_DATASOURCE_URN);
#ifdef HAVE_MEEGOTOUCH
	} else if (name && g_ascii_strcasecmp (type, "ControlPanelApplet") == 0) {
		/* Special case control panel applets */
		/* The URI of the InformationElement should be a UUID URN */
		uri = g_file_get_uri (data->file);
		tracker_sparql_builder_insert_silent_open (sparql, TRACKER_MINER_FS_GRAPH_URN);

		tracker_sparql_builder_subject_iri (sparql, APPLET_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nie:DataSource");

		/* TODO This is atm specific for Maemo */
		tracker_sparql_builder_subject_iri (sparql, uri);

		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "maemo:ControlPanelApplet");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLET_DATASOURCE_URN);

		/* This matches SomeApplet as Type= */
	} else if (name && g_str_has_suffix (type, "Applet")) {
		/* The URI of the InformationElement should be a UUID URN */
		uri = g_file_get_uri (data->file);
		tracker_sparql_builder_insert_silent_open (sparql, TRACKER_MINER_FS_GRAPH_URN);

		tracker_sparql_builder_subject_iri (sparql, APPLET_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nie:DataSource");

		/* TODO This is atm specific for Maemo */
		tracker_sparql_builder_subject_iri (sparql, uri);

		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "maemo:SoftwareApplet");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLET_DATASOURCE_URN);

	} else if (name && g_ascii_strcasecmp (type, "DUIApplication") == 0) {

		uri = g_file_get_uri (data->file);
		tracker_sparql_builder_insert_silent_open (sparql, TRACKER_MINER_FS_GRAPH_URN);

		tracker_sparql_builder_subject_iri (sparql, APPLICATION_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nie:DataSource");

		tracker_sparql_builder_subject_iri (sparql, uri);

		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:SoftwareApplication");
		tracker_sparql_builder_object (sparql, "nie:DataObject");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLICATION_DATASOURCE_URN);
#endif /* HAVE_MEEGOTOUCH */
	} else {
		/* Invalid type, all valid types are already listed above */
		uri = g_file_get_uri (data->file);
		tracker_sparql_builder_insert_silent_open (sparql, TRACKER_MINER_FS_GRAPH_URN);

		tracker_sparql_builder_subject_iri (sparql, APPLICATION_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nie:DataSource");

		tracker_sparql_builder_subject_iri (sparql, uri);

		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nfo:SoftwareApplication");
		tracker_sparql_builder_object (sparql, "nie:DataObject");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, APPLICATION_DATASOURCE_URN);

		if (name) {
			/* If we got a name, then the issue comes from the type.
			 * As we're defaulting to Application here, we just g_debug() the problem. */
			g_debug ("Invalid desktop file: '%s'", uri);
			g_debug ("  Type '%s' is not part of the desktop file specification (expected 'Application', 'Link' or 'Directory')", type);
			g_debug ("  Defaulting to 'Application'");
		} else {
			/* If we didn't get a name, the problem is more severe as we don't default it
			 * to anything, so we g_warning() it.  */
			g_warning ("Invalid desktop file: '%s'", uri);
#ifdef HAVE_MEEGOTOUCH
			g_warning ("  Couldn't get name, missing or wrong key (X-MeeGo-Logical-Id, X-MeeGo-Translation-Catalog or Name)");
#else
			g_warning ("  Couldn't get name, missing key (Name)");
#endif
		}
	}

	if (sparql && uri) {
		gchar *desktop_file_uri;

		tracker_sparql_builder_predicate (sparql, "a");

		if (is_software) {
			tracker_sparql_builder_object (sparql, "nfo:Executable");
		}

		tracker_sparql_builder_object (sparql, "nfo:FileDataObject");
		tracker_sparql_builder_object (sparql, "nie:DataObject");

		/* Apparently this gets added by the file-module ATM
		   tracker_sparql_builder_predicate (sparql, "tracker:available");
		   tracker_sparql_builder_object_boolean (sparql, TRUE); */

		/* We should always always have a proper name if the desktop file is correct
		 * w.r.t to the Meego or Freedesktop specs, but sometimes this is not true,
		 * so instead of passing wrong stuff to the SPARQL builder, we avoid it.
		 * If we don't have a proper name, we already warned it before. */
		if (name) {
			tracker_sparql_builder_predicate (sparql, "nie:title");
			tracker_sparql_builder_object_string (sparql, name);
		}

		if (is_software) {
			gchar *icon;

			insert_data_from_desktop_file (sparql,
			                               uri,
			                               TRACKER_NIE_PREFIX "comment",
			                               key_file,
			                               "Comment",
			                               lang);
			insert_data_from_desktop_file (sparql,
			                               uri,
			                               TRACKER_NFO_PREFIX "softwareCmdLine",
			                               key_file,
			                               "Exec",
			                               lang);

			icon = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, "Icon", NULL);

			if (icon) {
				gchar *icon_uri;

				/* Sanitize icon */
				g_strstrip (icon);

				icon_uri = g_strdup_printf (THEME_ICON_URN_PREFIX "%s", icon);

				tracker_sparql_builder_subject_iri (sparql, icon_uri);
				tracker_sparql_builder_predicate (sparql, "a");
				tracker_sparql_builder_object (sparql, "nfo:Image");

				tracker_sparql_builder_subject_iri (sparql, uri);
				tracker_sparql_builder_predicate (sparql, "nfo:softwareIcon");
				tracker_sparql_builder_object_iri (sparql, icon_uri);

				g_free (icon_uri);
				g_free (icon);
			}
		}

		if (cats) {
			gsize i;

			for (i = 0 ; cats[i] && i < cats_len ; i++) {
				gchar *cat_uri;
				gchar *cat;

				cat = cats[i];

				if (!cat) {
					continue;
				}

				/* Sanitize category */
				g_strstrip (cat);

				cat_uri = tracker_sparql_escape_uri_printf (SOFTWARE_CATEGORY_URN_PREFIX "%s", cat);

				/* There are also .desktop
				 * files that describe these categories, but we can handle
				 * preemptively creating them if we visit a app .desktop
				 * file that mentions one that we don't yet know about */

				tracker_sparql_builder_subject_iri (sparql, cat_uri);
				tracker_sparql_builder_predicate (sparql, "a");
				tracker_sparql_builder_object (sparql, "nfo:SoftwareCategory");

				tracker_sparql_builder_predicate (sparql, "nie:title");
				tracker_sparql_builder_object_string (sparql, cat);

				tracker_sparql_builder_subject_iri (sparql, uri);
				tracker_sparql_builder_predicate (sparql, "nie:isLogicalPartOf");
				tracker_sparql_builder_object_iri (sparql, cat_uri);

				g_free (cat_uri);
			}
		}

		filename = g_filename_display_basename (path);
		tracker_sparql_builder_predicate (sparql, "nfo:fileName");
		tracker_sparql_builder_object_string (sparql, filename);
		g_free (filename);

		/* The URL of the DataObject */
		desktop_file_uri = g_file_get_uri (data->file);
		tracker_sparql_builder_predicate (sparql, "nie:url");
		tracker_sparql_builder_object_string (sparql, desktop_file_uri);

		/* Laying the link between the IE and the DO */
		tracker_sparql_builder_subject_iri (sparql, uri);
		tracker_sparql_builder_predicate (sparql, "nie:isStoredAs");
		tracker_sparql_builder_object_iri (sparql, desktop_file_uri);


		g_free (desktop_file_uri);
	}

	if (file_info) {
		guint64 time;

		time = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		tracker_sparql_builder_predicate (sparql, "nfo:fileLastModified");
		tracker_sparql_builder_object_date (sparql, (time_t *) &time);
	}

	parent_urn = tracker_miner_fs_get_parent_urn (TRACKER_MINER_FS (data->miner), data->file);

	if (parent_urn) {
		tracker_sparql_builder_predicate (sparql, "nfo:belongsToContainer");
		tracker_sparql_builder_object_iri (sparql, parent_urn);
	}

	tracker_sparql_builder_insert_close (sparql);

	g_strfreev (cats);

	g_free (uri);
	g_free (path);
	g_free (name);
	g_free (lang);
}

static void
process_application_data_free (ProcessApplicationData *data)
{
	g_object_unref (data->miner);
	g_object_unref (data->file);
	g_object_unref (data->sparql);
	g_object_unref (data->cancellable);
	g_free (data->type);

	if (data->key_file) {
		g_key_file_free (data->key_file);
	}

	g_slice_free (ProcessApplicationData, data);
}

static void
process_file_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	ProcessApplicationData *data;
	GFileInfo *file_info;
	GError *error = NULL;
	GFile *file;

	data = user_data;
	file = G_FILE (object);
	file_info = g_file_query_info_finish (file, result, &error);

	if (error) {
		tracker_miner_fs_file_notify (TRACKER_MINER_FS (data->miner), file, error);
		process_application_data_free (data);
		g_error_free (error);
		return;
	}

	if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
		process_directory (data, file_info, &error);
	} else if (data->key_file) {
		process_desktop_file (data, file_info, &error);
	} else {
		error = g_error_new_literal (miner_applications_error_quark, 0, "File is not a key file");
	}

	tracker_miner_fs_file_notify (TRACKER_MINER_FS (data->miner), data->file, error);
	process_application_data_free (data);

	if (error) {
		g_error_free (error);
	}

	if (file_info) {
		g_object_unref (file_info);
	}
}

static gboolean
miner_applications_process_file (TrackerMinerFS       *fs,
                                 GFile                *file,
                                 TrackerSparqlBuilder *sparql,
                                 GCancellable         *cancellable)
{
	ProcessApplicationData *data;
	GKeyFile *key_file;
	const gchar *attrs;
	gchar *type;

	key_file = get_desktop_key_file (file, &type, NULL);

	data = g_slice_new0 (ProcessApplicationData);
	data->miner = g_object_ref (fs);
	data->sparql = g_object_ref (sparql);
	data->file = g_object_ref (file);
	data->cancellable = g_object_ref (cancellable);
	data->key_file = key_file;
	data->type = type;

	attrs = G_FILE_ATTRIBUTE_TIME_MODIFIED ","
		G_FILE_ATTRIBUTE_STANDARD_TYPE;

	g_file_query_info_async (file,
	                         attrs,
	                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                         G_PRIORITY_DEFAULT,
	                         cancellable,
	                         process_file_cb,
	                         data);

	return TRUE;
}

static gboolean
miner_applications_process_file_attributes (TrackerMinerFS       *fs,
                                            GFile                *file,
                                            TrackerSparqlBuilder *sparql,
                                            GCancellable         *cancellable)
{
	gchar *uri;

	/* We don't care about file attribute changes here */
	uri = g_file_get_uri (file);
	g_debug ("Ignoring file attribute changes in '%s'", uri);
	g_free (uri);

	return FALSE;
}

/* If a reset is requested, we will remove from the store all items previously
 * inserted by the tracker-miner-applications, this is:
 *  (a) all elements which are nfo:softwareIcon of a given nfo:Software
 *  (b) all nfo:Software in our graph (includes both applications and maemo applets)
 *  (c) all elements which are nfo:softwareCategoryIcon of a given nfo:SoftwareCategory
 *  (d) all nfo:SoftwareCategory in our graph
 */
static void
miner_applications_reset (TrackerMiner *miner)
{
	GError *error = NULL;
	TrackerSparqlBuilder *sparql;

	sparql = tracker_sparql_builder_new_update ();

	/* (a) all elements which are nfo:softwareIcon of a given nfo:Software */
	tracker_sparql_builder_delete_open (sparql, TRACKER_MINER_FS_GRAPH_URN);
	tracker_sparql_builder_subject_variable (sparql, "icon");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "rdfs:Resource");
	tracker_sparql_builder_delete_close (sparql);

	tracker_sparql_builder_where_open (sparql);
	tracker_sparql_builder_subject_variable (sparql, "software");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:Software");
	tracker_sparql_builder_subject_variable (sparql, "icon");
	tracker_sparql_builder_predicate (sparql, "nfo:softwareIcon");
	tracker_sparql_builder_object_variable (sparql, "software");
	tracker_sparql_builder_where_close (sparql);

	/* (b) all nfo:Software in our graph (includes both applications and maemo applets) */
	tracker_sparql_builder_delete_open (sparql, TRACKER_MINER_FS_GRAPH_URN);
	tracker_sparql_builder_subject_variable (sparql, "software");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "rdfs:Resource");
	tracker_sparql_builder_delete_close (sparql);

	tracker_sparql_builder_where_open (sparql);
	tracker_sparql_builder_subject_variable (sparql, "software");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:Software");
	tracker_sparql_builder_where_close (sparql);

	/* (c) all elements which are nfo:softwareCategoryIcon of a given nfo:SoftwareCategory */
	tracker_sparql_builder_delete_open (sparql, TRACKER_MINER_FS_GRAPH_URN);
	tracker_sparql_builder_subject_variable (sparql, "icon");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "rdfs:Resource");
	tracker_sparql_builder_delete_close (sparql);

	tracker_sparql_builder_where_open (sparql);
	tracker_sparql_builder_subject_variable (sparql, "category");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:SoftwareCategory");
	tracker_sparql_builder_subject_variable (sparql, "icon");
	tracker_sparql_builder_predicate (sparql, "nfo:softwareCategoryIcon");
	tracker_sparql_builder_object_variable (sparql, "category");
	tracker_sparql_builder_where_close (sparql);

	/* (d) all nfo:SoftwareCategory in our graph */
	tracker_sparql_builder_delete_open (sparql, TRACKER_MINER_FS_GRAPH_URN);
	tracker_sparql_builder_subject_variable (sparql, "category");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "rdfs:Resource");
	tracker_sparql_builder_delete_close (sparql);

	tracker_sparql_builder_where_open (sparql);
	tracker_sparql_builder_subject_variable (sparql, "category");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:SoftwareCategory");
	tracker_sparql_builder_where_close (sparql);

	/* Execute a sync update, we don't want the apps miner to start before
	 * we finish this. */
	tracker_sparql_connection_update (tracker_miner_get_connection (miner),
	                                  tracker_sparql_builder_get_result (sparql),
	                                  G_PRIORITY_HIGH,
	                                  NULL,
	                                  &error);

	if (error) {
		/* Some error happened performing the query, not good */
		g_critical ("Couldn't reset mined applications: %s",
		            error ? error->message : "unknown error");
		g_error_free (error);
	}

	g_object_unref (sparql);
}

gboolean
tracker_miner_applications_detect_locale_changed (TrackerMiner *miner)
{
	gboolean changed;

	changed = tracker_miner_applications_locale_changed ();
	if (changed) {
		g_message ("Locale change detected, so resetting miner to "
		           "remove all previously created items...");
		miner_applications_reset (miner);
	}
	return changed;
}

TrackerMiner *
tracker_miner_applications_new (GError **error)
{
	return g_initable_new (TRACKER_TYPE_MINER_APPLICATIONS,
	                       NULL,
	                       error,
	                       "name", "Applications",
	                       "processing-pool-wait-limit", 10,
	                       "processing-pool-ready-limit", 100,
	                       "processing-pool-requests-limit", 10,
	                       NULL);
}
