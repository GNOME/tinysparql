/* vim: set noet ts=8 sw=8 sts=0: */
/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <sys/types.h>
#ifndef OS_WIN32
#include <sys/resource.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include <glib.h>
#include <gmodule.h>

#define _XOPEN_SOURCE
#include <time.h>

#include "tracker-extract.h"

#define MAX_MEM 128
#define MAX_MEM_AMD64 512

#define ISO8601_FORMAT "%Y-%m-%dT%H:%M:%S%z"

GArray *extractors = NULL;


gchar *
tracker_generic_date_to_iso8601 (const gchar *date, const gchar *format)
{

        gchar *processed;
        gchar *result;
        struct tm date_tm;
        
        memset (&date_tm, 0, sizeof (struct tm));

        processed = strptime (date, format, &date_tm);
        
        if (processed == NULL) {
                // Unable to parse the input
                return NULL;
        }

        result = g_malloc (sizeof (char)*25);

        strftime (result, 25, ISO8601_FORMAT , &date_tm);

        return result;
}


gboolean
tracker_is_empty_string (const gchar *s)
{
        return s == NULL || s[0] == '\0';
}


static gboolean
set_memory_rlimits (void)
{
#ifndef OS_WIN32
	struct	rlimit rl;
	gint	fail = 0;

	/* We want to limit the max virtual memory
	 * most extractors use mmap() so only virtual memory can be effectively limited */
#ifdef __x86_64__
	/* many extractors on AMD64 require 512M of virtual memory, so we limit heap too */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = MAX_MEM_AMD64*1024*1024;
	fail |= setrlimit (RLIMIT_AS, &rl);

	getrlimit (RLIMIT_DATA, &rl);
	rl.rlim_cur = MAX_MEM*1024*1024;
	fail |= setrlimit (RLIMIT_DATA, &rl);
#else
	/* on other architectures, 128M of virtual memory seems to be enough */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = MAX_MEM*1024*1024;
	fail |= setrlimit (RLIMIT_AS, &rl);
#endif

	if (fail) {
		g_printerr ("Error trying to set memory limit for tracker-extract\n");
        }

	return !fail;
#endif
}


void
tracker_child_cb (gpointer user_data)
{
#ifndef OS_WIN32
	struct 	rlimit cpu_limit;
	gint	timeout = GPOINTER_TO_INT (user_data);

	/* set cpu limit */
	getrlimit (RLIMIT_CPU, &cpu_limit);
	cpu_limit.rlim_cur = timeout;
	cpu_limit.rlim_max = timeout + 1;

	if (setrlimit (RLIMIT_CPU, &cpu_limit) != 0) {
		g_printerr ("Error trying to set resource limit for cpu\n");
	}

	set_memory_rlimits ();

	/* Set child's niceness to 19 */
        errno = 0;
        /* nice() uses attribute "warn_unused_result" and so complains if we do not check its
           returned value. But it seems that since glibc 2.2.4, nice() can return -1 on a
           successful call so we have to check value of errno too. Stupid... */
        if (nice (19) == -1 && errno) {
                g_printerr ("ERROR: trying to set nice value\n");
        }

	/* have this as a precaution in cases where cpu limit has not been reached due to spawned app sleeping */
	alarm (timeout+2);
#endif
}


gboolean
tracker_spawn (gchar **argv, gint timeout, gchar **tmp_stdout, gint *exit_status)
{
	return g_spawn_sync (NULL,
                             argv,
                             NULL,
                             G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                             tracker_child_cb,
                             GINT_TO_POINTER (timeout),
                             tmp_stdout,
                             NULL,
                             exit_status,
                             NULL);
}


static void
initialize_extractors (void)
{
	GDir        *dir;
	GError      *error = NULL;
	const gchar *name;
	GArray      *generic_extractors = NULL;

	if (extractors != NULL)
		return;

	if (!g_module_supported ()) {
		g_error ("Modules are not supported for this platform");
		return;
	}

	extractors = g_array_sized_new (FALSE, TRUE,
					sizeof (TrackerExtractorData),
					10);

	/* This array is going to be used to store
	 * temporarily extractors with mimetypes such as "audio / *"
	 */
	generic_extractors = g_array_sized_new (FALSE, TRUE,
						sizeof (TrackerExtractorData),
						10);

	dir = g_dir_open (MODULES_DIR, 0, &error);

	if (!dir) {
		g_error ("Error opening modules directory: %s\n", error->message);
		g_error_free (error);
                g_array_free (extractors, TRUE);
                extractors = NULL;
                g_array_free (generic_extractors, TRUE);
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		GModule                  *module;
		gchar                    *module_path;
		TrackerExtractorDataFunc func;
		TrackerExtractorData     *data;

		if (!g_str_has_suffix (name, "." G_MODULE_SUFFIX)) {
			continue;
		}

		module_path = g_build_filename (MODULES_DIR, name, NULL);

		module = g_module_open (module_path, G_MODULE_BIND_LOCAL);

		if (!module) {
			g_warning ("Could not load module: %s", name);
			g_free (module_path);
			continue;
		}

		g_module_make_resident (module);

		if (g_module_symbol (module, "tracker_get_extractor_data", (gpointer *) &func)) {
			data = (func) ();

			while (data->mime) {
				if (strchr (data->mime, '*') != NULL) {
					g_array_append_val (generic_extractors, *data);
				} else {
					g_array_append_val (extractors, *data);
				}

				data++;
			}
		}

		g_free (module_path);
	}

	/* append the generic extractors at the end of
	 * the list, so the specific ones are used first
	 */
	g_array_append_vals (extractors, generic_extractors->data, generic_extractors->len);

	g_array_free (generic_extractors, TRUE);
}


static GHashTable *
tracker_get_file_metadata (const gchar *uri, const gchar *mime)
{
	GHashTable      *meta_table;
	gchar		*uri_in_locale;

	if (!uri) {
		return NULL;
	}

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		return NULL;
	}

	if (!g_file_test (uri_in_locale, G_FILE_TEST_EXISTS)) {
		g_free (uri_in_locale);
		return NULL;
	}

	meta_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	if (mime) {
		guint i;
		TrackerExtractorData *data;

		for (i = 0; i < extractors->len; i++) {
			data = &g_array_index (extractors, TrackerExtractorData, i);

			if (g_pattern_match_simple (data->mime, mime)) {
				(*data->extractor) (uri_in_locale, meta_table);

				if (g_hash_table_size (meta_table) == 0) {
					continue;
				}

				g_free (uri_in_locale);

				return meta_table;
			}
		}
	}

	g_free (uri_in_locale);

	return NULL;
}


static void
get_meta_table_data (gpointer pkey, gpointer pvalue, gpointer user_data)
{
	char *value;

	g_return_if_fail (pkey && pvalue);

	value = g_locale_to_utf8 ((char *) pvalue, -1, NULL, NULL, NULL);

	if (value) {
		if (value[0] != '\0') {
			/* replace any embedded semicolons or "=" as we use them for delimiters */
			value = g_strdelimit (value, ";", ',');
			value = g_strdelimit (value, "=", '-');
			value = g_strstrip (value);

			g_print ("%s=%s;\n", (char *) pkey, value);
		}

		g_free (value);
	}
}


static void
kill_app_timeout (void)
{
	g_usleep (1000 * 1000 * 10);
	exit (EXIT_FAILURE);
}


gint
main (gint argc, gchar *argv[])
{
	GHashTable *meta;
	gchar	   *filename;

	set_memory_rlimits ();

	if (!g_thread_supported ())
		g_thread_init (NULL);

	g_thread_create ((GThreadFunc) kill_app_timeout, NULL, FALSE, NULL);

	g_set_application_name ("tracker-extract");

	setlocale (LC_ALL, "");

	initialize_extractors ();

	if (argc == 1 || argc > 3) {
		g_print ("usage: tracker-extract file [mimetype]\n");
		return EXIT_FAILURE;
	}

	filename = g_filename_to_utf8 (argv[1], -1, NULL, NULL, NULL);

	if (!filename) {
		g_warning ("locale to UTF8 failed for filename!");
		return 1;
	}

	if (argc == 3) {
		gchar *mime = g_locale_to_utf8 (argv[2], -1, NULL, NULL, NULL);

		if (!mime) {
			g_warning ("locale to UTF8 failed for mime!");
			return EXIT_FAILURE;
		}

		meta = tracker_get_file_metadata (filename, mime);
		g_free (mime);
	} else {
		meta = tracker_get_file_metadata (filename, NULL);
	}

	g_free (filename);

	if (meta) {
		g_hash_table_foreach (meta, get_meta_table_data, NULL);
		g_hash_table_destroy (meta);
	}

	return EXIT_SUCCESS;
}
