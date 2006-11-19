/* Tracker
 * metadata routines
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <string.h>
#include <glib/gstdio.h>

#include "tracker-metadata.h"
#include "tracker-utils.h"


extern Tracker *tracker;


typedef enum {
	IGNORE_METADATA,
	NO_METADATA,
	DOC_METADATA,
	IMAGE_METADATA,
	VIDEO_METADATA,
	AUDIO_METADATA,
	DEVEL_METADATA,
	TEXT_METADATA
} MetadataFileType;


/* document mime type specific metadata groups - NB mime types below may be prefixes */
char *doc_mime_types[] = {
			  "application/rtf",
			  "text/richtext",
			  "application/msword",
			  "application/pdf",
			  "application/postscript",
			  "application/x-dvi",
			  "application/vnd.ms-excel",
			  "vnd.ms-powerpoint",
			  "application/vnd.oasis.opendocument",
			  "application/vnd.sun.xml",
			  "application/vnd.stardivision",
			  "application/x-abiword",
			  "text/html",
			  "text/sgml",
			  "application/x-mswrite",
			  "application/x-applix-word",
			  "application/docbook+xml",
			  "application/x-kword",
			  "application/x-kword-crypt",
			  "application/x-lyx",
			  "application/vnd.lotus-1-2-3",
			  "application/x-applix-spreadsheet",
			  "application/x-gnumeric",
			  "application/x-kspread",
			  "application/x-kspread-crypt",
			  "application/x-quattropro",
			  "application/x-sc",
			  "application/x-siag",
			  "application/x-magicpoint",
			  "application/x-kpresenter",
			  "application/illustrator",
			  "application/vnd.corel-draw",
			  "application/vnd.stardivision.draw",
			  "application/vnd.oasis.opendocument.graphics",
			  "application/x-dia-diagram",
			  "application/x-karbon",
			  "application/x-killustrator",
			  "application/x-kivio",
			  "application/x-kontour",
			  "application/x-wpg"
};


char *text_mime_types[] = {
		"text/plain",
		"text/x-authors",
		"text/x-changelog",
		"text/x-copying",
		"text/x-credits",
		"text/x-install",
		"text/x-readme"
};


char *development_mime_types[] = {
				"application/x-perl",
				"application/x-shellscript",
				"application/x-php",
				"application/x-java",
				"application/x-javascript",
				"application/x-glade",
				"application/x-csh",
				"application/x-class-file",
				"application/x-awk",
				"text/x-adasrc",
				"text/x-c++hdr",
				"text/x-chdr",
				"text/x-csharp",
				"text/x-c++src",
				"text/x-csrc",
				"text/x-dcl",
				"text/x-dsrc",
				"text/x-emacs-lisp",
				"text/x-fortran",
				"text/x-haskell",
				"text/x-literate-haskell",
				"text/x-java",
				"text/x-java-source" ,
				"text/x-makefile",
				"text/x-objcsrc",
				"text/x-pascal",
				"text/x-patch",
				"text/x-python",
				"text/x-scheme",
				"text/x-sql",
				"text/x-tcl"
};


static void
set_child_timeout_cb (gpointer user_data)
{
	alarm (GPOINTER_TO_INT (user_data));
}


static MetadataFileType
tracker_get_metadata_type (const char *mime)
{
	int i;
	int num_elements;

	if (strcmp (mime, "text/plain") == 0) {
		return TEXT_METADATA;
	}

	if (g_str_has_prefix (mime, "image") || (strcmp (mime, "application/vnd.oasis.opendocument.image") == 0) || (strcmp (mime, "application/x-krita") == 0)) {
		return IMAGE_METADATA;

	} else 	if (g_str_has_prefix (mime, "video")) {
			return VIDEO_METADATA;

	} else 	if (g_str_has_prefix (mime, "audio") || (strcmp (mime, "application/ogg") == 0)) {
			return AUDIO_METADATA;

	} else {
		num_elements = sizeof (doc_mime_types) / sizeof (char *);
		for (i = 0; i < num_elements; i++ ) {
			if (g_str_has_prefix (mime, doc_mime_types [i] )) {
				return DOC_METADATA;
			}
		}
	}

	num_elements = sizeof (development_mime_types) / sizeof (char *);

	for (i = 0; i < num_elements; i++ ) {
		if (strcmp (mime, development_mime_types[i]) == 0 ) {
			return DEVEL_METADATA;
		}
	}

	num_elements = sizeof (text_mime_types) / sizeof (char *);

	for (i = 0; i < num_elements; i++ ) {
		if (strcmp (mime, text_mime_types[i]) == 0 ) {
			return TEXT_METADATA;
		}
	}

	return NO_METADATA;
}


char *
tracker_get_service_type_for_mime (const char *mime)
{
	MetadataFileType stype;

	stype = tracker_get_metadata_type (mime);

	switch (stype) {

			case IGNORE_METADATA:
				if (g_str_has_prefix (mime, "video")) {
					return g_strdup ("Videos");
				} else {
					return g_strdup ("Other Files");
				}
				break;

			case NO_METADATA:
				return g_strdup ("Other Files");
				break;

			case TEXT_METADATA:
				return g_strdup ("Text Files");
				break;

			case DOC_METADATA:
				return g_strdup ("Documents");
				break;

			case IMAGE_METADATA:
				return g_strdup ("Images");
				break;

			case VIDEO_METADATA:
				return g_strdup ("Videos");
				break;

			case AUDIO_METADATA:
				return g_strdup ("Music");
				break;

			case DEVEL_METADATA:
				return g_strdup ("Development Files");
				break;
	}

	return g_strdup ("Other Files");
}


char *
tracker_metadata_get_text_file (const char *uri, const char *mime)
{
	MetadataFileType ftype;
	char		 *text_filter_file;

	text_filter_file = NULL;

	ftype = tracker_get_metadata_type (mime);

	/* no need to filter text based files - index em directly */

	if (ftype == TEXT_METADATA || ftype == DEVEL_METADATA) {

		return g_strdup (uri);

	} else {
		char *tmp;

		tmp = g_strdup (DATADIR "/tracker/filters/");

		text_filter_file = g_strconcat (tmp, mime, "_filter", NULL);

		g_free (tmp);
	}

	if (text_filter_file && g_file_test (text_filter_file, G_FILE_TEST_EXISTS)) {
		char *argv[4];
		char *temp_file_name;
		int  fd;

		temp_file_name = g_build_filename (tracker->sys_tmp_root_dir, "tmp_text_file_XXXXXX", NULL);

		fd = g_mkstemp (temp_file_name);

		if (fd == -1) {
			g_warning ("make tmp file %s failed", temp_file_name);
			return NULL;
		} else {
			close (fd);
		}

		argv[0] = g_strdup (text_filter_file);
		argv[1] = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
		argv[2] = g_strdup (temp_file_name);
		argv[3] = NULL;

		g_free (text_filter_file);

		if (!argv[1]) {
			tracker_log ("******ERROR**** uri could not be converted to locale format");
			g_free (argv[0]);
			g_free (argv[2]);
			return NULL;
		}

		g_debug ("extracting text for %s using filter %s", argv[1], argv[0]);

		if (g_spawn_sync (NULL,
				  argv,
				  NULL,
				  G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
				  set_child_timeout_cb,
				  GINT_TO_POINTER (30),
				  NULL,
				  NULL,
				  NULL,
				  NULL)) {

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);

			return temp_file_name;

		} else {
			g_free (temp_file_name);

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);

			return NULL;
		}

	} else {
		g_free (text_filter_file);
	}

	return NULL;
}


char *
tracker_metadata_get_thumbnail (const char *uri, const char *mime, const char *max_size)
{
	char *tmp, *thumbnailer;

	tmp = g_strdup (DATADIR "/tracker/thumbnailers/");

	thumbnailer = g_strconcat (tmp, mime, "_thumbnailer", NULL);

	g_free (tmp);

	if (g_file_test (thumbnailer, G_FILE_TEST_EXISTS)) {
		char *argv[5];
		char *tmp_file;
		int  fd;

		tmp = g_build_filename (g_get_home_dir (), ".Tracker", "thumbs", NULL);

		g_mkdir_with_parents (tmp, 0700);

		tmp_file = g_build_filename (tmp, "thumb_XXXXXX", NULL);
		g_free (tmp);


		fd = g_mkstemp (tmp_file);

		if (fd == -1) {
			g_warning ("make thumb file %s failed", tmp_file);
			g_free (thumbnailer);
			g_free (tmp_file);
			return NULL;
		} else {
			close (fd);
		}

		argv[0] = g_strdup (thumbnailer);
		argv[1] = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
		argv[2] = g_strdup (tmp_file);
		argv[3] = g_strdup (max_size);
		argv[4] = NULL;

		if (!argv[1]) {
			tracker_log ("******ERROR**** uri could not be converted to locale format");
			g_free (argv[0]);
			g_free (argv[2]);
			g_free (argv[3]);
			return NULL;
		}

		g_debug ("Extracting thumbnail for %s using %s", argv[1], argv[0] );

		if (g_spawn_sync (NULL,
				  argv,
				  NULL,
				  G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
				  set_child_timeout_cb,
				  GINT_TO_POINTER (10),
				  NULL,
				  NULL,
				  NULL,
				  NULL)) {

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
			g_free (argv[3]);

			g_free (thumbnailer);

			return tmp_file;

		} else {
			g_free (tmp_file);

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
			g_free (argv[3]);
		}
	}

	g_free (thumbnailer);

	return NULL;
}


void
tracker_metadata_get_embedded (const char *uri, const char *mime, GHashTable *table)
{
	MetadataFileType meta_type;

	if (!uri || !mime || !table) {
		return;
	}

	meta_type = tracker_get_metadata_type (mime);

	if (meta_type == DOC_METADATA || meta_type == IMAGE_METADATA || meta_type == AUDIO_METADATA || meta_type == VIDEO_METADATA) {
		char *argv[4];
		char *value;

		/* we extract metadata out of process using pipes */

		argv[0] = g_strdup ("tracker-extract");
		argv[1] = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
		argv[2] = g_locale_from_utf8 (mime, -1, NULL, NULL, NULL);
		argv[3] = NULL;

		if (!argv[1] || !argv[2]) {
			tracker_log ("******ERROR**** uri or mime could not be converted to locale format");

			g_free (argv[0]);

			if (argv[1]) {
				g_free (argv[1]);
			}

			if (argv[2]) {
				g_free (argv[2]);
			}

			return;
		}

		if (g_spawn_sync (NULL,
				  argv,
				  NULL,
				  G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
				  set_child_timeout_cb,
				  GINT_TO_POINTER (10),
				  &value,
				  NULL,
				  NULL,
				  NULL)) {

			/* parse returned stdout (value) and extract keys and associated metadata values */

			if (value && strchr (value, '=') && strchr (value, ';')) {
				char **values, **values_p;

				values = g_strsplit_set (value, ";", -1);

				for (values_p = values; *values_p; values_p++) {
					char *meta_data, *sep;

					meta_data = g_strdup (g_strstrip (*values_p));

					sep = strchr (meta_data, '=');

					if (sep) {
						char *meta_name;

						meta_name = g_strndup (meta_data, sep - meta_data);

						if (meta_name) {
							char *meta_value;

							meta_value = g_strdup (sep + 1);

							if (meta_value) {
								char *st;

								//tracker_log ("testing %s = %s", meta_name, meta_value);
								st = g_hash_table_lookup (table, meta_name);

								if (st == NULL) {
									char *utf_value;

									if (!g_utf8_validate (meta_value, -1, NULL)) {

										utf_value = g_locale_to_utf8 (meta_value, -1, NULL, NULL, NULL);
									} else {
										utf_value = g_strdup (meta_value);
									}

									if (utf_value) {
										guint32 length = strlen (utf_value);

										if ((length > 0) && (length >= strlen (meta_value))) {

											g_debug ("%s = %s", meta_name, utf_value);
											g_hash_table_insert (table, g_strdup (meta_name), g_strdup (utf_value));
										}

										g_free (utf_value);
									}
								}

								g_free (meta_value);
							}

							g_free (meta_name);
						}
					}

					g_free (meta_data);
				}

				g_strfreev (values);
			}

			if (value) {
				g_free (value);
			}

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
		}
	}
}
