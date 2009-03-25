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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-ontology.h>
#include <tracker-indexer/tracker-module.h>

/* This is ONLY needed for the indexer to run standalone with
 * the -p option, otherwise it will pick up all sorts of crap
 * from the file system to get the metadata for. The daemon
 * currently does all this for us.
 */
#undef ENABLE_FILE_EXCLUDE_CHECKING

#define TRACKER_TYPE_REGULAR_FILE    (tracker_regular_file_get_type ())
#define TRACKER_REGULAR_FILE(module) (G_TYPE_CHECK_INSTANCE_CAST ((module), TRACKER_TYPE_REGULAR_FILE, TrackerRegularFile))

typedef struct TrackerRegularFile TrackerRegularFile;
typedef struct TrackerRegularFileClass TrackerRegularFileClass;

struct TrackerRegularFile {
        TrackerModuleFile parent_instance;
};

struct TrackerRegularFileClass {
        TrackerModuleFileClass parent_class;
};


static GType                   tracker_regular_file_get_type         (void) G_GNUC_CONST;

static const gchar *           tracker_regular_file_get_service_type (TrackerModuleFile *file);
static gchar *                 tracker_regular_file_get_text         (TrackerModuleFile *file);
static TrackerModuleMetadata * tracker_regular_file_get_metadata     (TrackerModuleFile *file);
static void                    tracker_regular_file_cancel           (TrackerModuleFile *file);


G_DEFINE_DYNAMIC_TYPE (TrackerRegularFile, tracker_regular_file, TRACKER_TYPE_MODULE_FILE);


static void
tracker_regular_file_class_init (TrackerRegularFileClass *klass)
{
        TrackerModuleFileClass *file_class = TRACKER_MODULE_FILE_CLASS (klass);

        file_class->get_service_type = tracker_regular_file_get_service_type;
        file_class->get_text = tracker_regular_file_get_text;
        file_class->get_metadata = tracker_regular_file_get_metadata;
	file_class->cancel = tracker_regular_file_cancel;
}

static void
tracker_regular_file_class_finalize (TrackerRegularFileClass *klass)
{
}

static void
tracker_regular_file_init (TrackerRegularFile *file)
{
}

static const gchar *
tracker_regular_file_get_service_type (TrackerModuleFile *file)
{
        GFile *f;
	const gchar *service_type;
	gchar *mime_type, *path;

        f = tracker_module_file_get_file (file);

	if (!g_file_query_exists (f, NULL)) {
		return NULL;
	}

	path = g_file_get_path (f);

	mime_type = tracker_file_get_mime_type (path);
	service_type = tracker_ontology_get_service_by_mime (mime_type);

	g_free (mime_type);
        g_free (path);

	return service_type;
}

#ifdef ENABLE_FILE_EXCLUDE_CHECKING

static gboolean
check_exclude_file (const gchar *path)
{
	gchar *name;
	guint i;

	const gchar const *ignore_suffix[] = {
		"~", ".o", ".la", ".lo", ".loT", ".in",
		".csproj", ".m4", ".rej", ".gmo", ".orig",
		".pc", ".omf", ".aux", ".tmp", ".po",
		".vmdk",".vmx",".vmxf",".vmsd",".nvram",
		".part", ".bak"
	};

	const gchar const *ignore_prefix[] = {
		"autom4te", "conftest.", "confstat",
		"config."
	};

	const gchar const *ignore_name[] = {
		"po", "CVS", "aclocal", "Makefile", "CVS",
		"SCCS", "ltmain.sh","libtool", "config.status",
		"conftest", "confdefs.h"
	};

	if (g_str_has_prefix (path, "/proc/") ||
	    g_str_has_prefix (path, "/dev/") ||
	    g_str_has_prefix (path, "/tmp/") ||
	    g_str_has_prefix (path, g_get_tmp_dir ())) {
		return TRUE;
	}

	name = g_path_get_basename (path);

	if (name[0] == '.') {
		g_free (name);
		return TRUE;
	}

	for (i = 0; i < G_N_ELEMENTS (ignore_suffix); i++) {
		if (g_str_has_suffix (name, ignore_suffix[i])) {
			g_free (name);
			return TRUE;
		}
	}

	for (i = 0; i < G_N_ELEMENTS (ignore_prefix); i++) {
		if (g_str_has_prefix (name, ignore_prefix[i])) {
			g_free (name);
			return TRUE;
		}
	}

	for (i = 0; i < G_N_ELEMENTS (ignore_name); i++) {
		if (strcmp (name, ignore_name[i]) == 0) {
			g_free (name);
			return TRUE;
		}
	}

	/* FIXME: check NoIndexFileTypes in configuration */

	g_free (name);
	return FALSE;
}

#endif /* ENABLE_FILE_EXCLUDE_CHECKING */

static TrackerModuleMetadata *
tracker_regular_file_get_metadata (TrackerModuleFile *file)
{
#ifdef ENABLE_FILE_EXCLUDE_CHECKING
	if (check_exclude_file (file->path)) {
		return NULL;
	}
#endif

	return tracker_module_metadata_utils_get_data (tracker_module_file_get_file (file));
}

static gchar *
tracker_regular_file_get_text (TrackerModuleFile *file)
{
#ifdef ENABLE_FILE_EXCLUDE_CHECKING
	if (check_exclude_file (file->path)) {
		return NULL;
	}
#endif

	return tracker_module_metadata_utils_get_text (tracker_module_file_get_file (file));
}

static void
tracker_regular_file_cancel (TrackerModuleFile *file)
{
        GFile *f;

        f = tracker_module_file_get_file (file);

	tracker_module_metadata_utils_cancel (f);
}

void
indexer_module_initialize (GTypeModule *module)
{
        tracker_regular_file_register_type (module);
}

void
indexer_module_shutdown (void)
{
}

TrackerModuleFile *
indexer_module_create_file (GFile *file)
{
        return g_object_new (TRACKER_TYPE_REGULAR_FILE,
                             "file", file,
                             NULL);
}
