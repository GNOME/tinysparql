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

#include <tracker-indexer/tracker-module.h>

#define GROUP_DESKTOP_ENTRY "Desktop Entry"
#define KEY_TYPE	    "Type"
#define KEY_HIDDEN	    "Hidden"
#define KEY_NAME	    "Name"
#define KEY_GENERIC_NAME    "GenericName"
#define KEY_COMMENT	    "Comment"
#define KEY_EXECUTABLE	    "Exec"
#define KEY_ICON	    "Icon"
#define KEY_MIMETYPE	    "MimeType"
#define KEY_CATEGORIES	    "Categories"

#define METADATA_FILE_NAME	  "File:Name"
#define METADATA_APP_NAME	  "App:Name"
#define METADATA_APP_DISPLAY_NAME "App:DisplayName"
#define METADATA_APP_GENERIC_NAME "App:GenericName"
#define METADATA_APP_COMMENT	  "App:Comment"
#define METADATA_APP_EXECUTABLE   "App:Exec"
#define METADATA_APP_ICON	  "App:Icon"
#define METADATA_APP_MIMETYPE	  "App:MimeType"
#define METADATA_APP_CATEGORIES   "App:Categories"

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
static TrackerModuleMetadata * tracker_application_file_get_metadata  (TrackerModuleFile *file);


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
insert_data_from_desktop_file (TrackerModuleMetadata *metadata,
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
		tracker_module_metadata_add_string (metadata, metadata_key, str);
		g_free (str);
	}
}

static void
insert_list_from_desktop_file (TrackerModuleMetadata *metadata,
			       const gchar           *metadata_key,
			       GKeyFile              *desktop_file,
			       const gchar           *key,
			       gboolean		      use_locale)
{
	gchar **arr;

	if (use_locale) {
		arr = g_key_file_get_locale_string_list (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL, NULL, NULL);
	} else {
		arr = g_key_file_get_string_list (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL, NULL);
	}

	if (arr) {
		gint i;

		for (i = 0; arr[i]; i++) {
			tracker_module_metadata_add_string (metadata, metadata_key, arr[i]);
		}

		g_strfreev (arr);
	}
}

static TrackerModuleMetadata *
tracker_application_file_get_metadata (TrackerModuleFile *file)
{
	TrackerModuleMetadata *metadata;
	GKeyFile *key_file;
        GFile *f;
	gchar *path, *type, *filename;

        f = tracker_module_file_get_file (file);
        path = g_file_get_path (f);

	/* Check we're dealing with a desktop file */
	if (!g_str_has_suffix (path, ".desktop")) {
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

	if (g_key_file_get_boolean (key_file, GROUP_DESKTOP_ENTRY, KEY_HIDDEN, NULL)) {
                g_debug ("Desktop file is 'hidden', not gathering metadata for it");
		g_key_file_free (key_file);
                g_free (path);
		return NULL;
	}

	type = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, KEY_TYPE, NULL);

	if (!type || g_ascii_strcasecmp (type, "Application") != 0) {
                g_debug ("Desktop file is not of type 'Application', not gathering metadata for it");
		g_key_file_free (key_file);
		g_free (type);
                g_free (path);
		return NULL;
	}

	/* Begin collecting data */
	metadata = tracker_module_metadata_new ();

	insert_data_from_desktop_file (metadata, METADATA_APP_NAME, key_file, KEY_NAME, FALSE);
	insert_data_from_desktop_file (metadata, METADATA_APP_DISPLAY_NAME, key_file, KEY_NAME, TRUE);
	insert_data_from_desktop_file (metadata, METADATA_APP_GENERIC_NAME, key_file, KEY_GENERIC_NAME, TRUE);
	insert_data_from_desktop_file (metadata, METADATA_APP_COMMENT, key_file, KEY_COMMENT, TRUE);
	insert_data_from_desktop_file (metadata, METADATA_APP_EXECUTABLE, key_file, KEY_EXECUTABLE, FALSE);
	insert_data_from_desktop_file (metadata, METADATA_APP_ICON, key_file, KEY_ICON, FALSE);

	insert_list_from_desktop_file (metadata, METADATA_APP_MIMETYPE, key_file, KEY_MIMETYPE, FALSE);
	insert_list_from_desktop_file (metadata, METADATA_APP_CATEGORIES, key_file, KEY_CATEGORIES, FALSE);

	filename = g_filename_display_basename (path);
	tracker_module_metadata_add_string (metadata, METADATA_FILE_NAME, filename);
	g_free (filename);

	g_key_file_free (key_file);
	g_free (type);
        g_free (path);

	return metadata;
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
