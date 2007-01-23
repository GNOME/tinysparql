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

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <glib.h>

#include "config.h"

#define MAX_MEM 128

typedef enum {
	IGNORE_METADATA,
	NO_METADATA,
	DOC_METADATA,
	IMAGE_METADATA,
	VIDEO_METADATA,
	AUDIO_METADATA
} MetadataFileType;



typedef void (*MetadataExtractFunc)(gchar *, GHashTable *);
 
typedef struct {
 	char			*mime;
 	MetadataExtractFunc	extractor;
} MimeToExtractor;

void tracker_extract_mplayer	(gchar *, GHashTable *);
void tracker_extract_totem	(gchar *, GHashTable *);
void tracker_extract_oasis	(gchar *, GHashTable *);
void tracker_extract_ps		(gchar *, GHashTable *);
#ifdef HAVE_POPPLER
void tracker_extract_pdf	(gchar *, GHashTable *);
#endif
void tracker_extract_abw	(gchar *, GHashTable *);
#ifdef HAVE_LIBGSF
void tracker_extract_msoffice	(gchar *, GHashTable *);
#endif
void tracker_extract_mp3	(gchar *, GHashTable *);
#ifdef HAVE_VORBIS
void tracker_extract_vorbis	(gchar *, GHashTable *);
#endif
void tracker_extract_png	(gchar *, GHashTable *);
#ifdef HAVE_LIBEXIF
void tracker_extract_exif	(gchar *, GHashTable *);
#endif
void tracker_extract_imagemagick (gchar *, GHashTable *);
#ifdef HAVE_GSTREAMER
void tracker_extract_gstreamer	(gchar *, GHashTable *);
#else
#ifdef HAVE_LIBXINE
void tracker_extract_xine	(gchar *, GHashTable *);
#else
#ifdef USING_EXTERNAL_VIDEO_PLAYER

#endif
#endif
#endif
  
MimeToExtractor extractors[] = {
	/* Document extractors */
 	{ "application/vnd.oasis.opendocument.*",	tracker_extract_oasis		},
 	{ "application/postscript",			tracker_extract_ps		},
#ifdef HAVE_POPPLER
 	{ "application/pdf",				tracker_extract_pdf		},
#endif
	{ "application/x-abiword",			tracker_extract_abw		},
#ifdef HAVE_LIBGSF
 	{ "application/msword",				tracker_extract_msoffice	},
 	{ "application/vnd.ms-*",			tracker_extract_msoffice	},
#endif
  
  	/* Video extractors */
#ifdef HAVE_GSTREAMER
 	{ "video/*",					tracker_extract_gstreamer	},
 	{ "video/*",					tracker_extract_mplayer		},
 	{ "video/*",					tracker_extract_totem		},
#else
#ifdef HAVE_LIBXINE
 	{ "video/*",					tracker_extract_xine		},
 	{ "video/*",					tracker_extract_mplayer		},
 	{ "video/*",					tracker_extract_totem		},
#else
#ifdef USING_EXTERNAL_VIDEO_PLAYER
 	{ "video/*",					tracker_extract_mplayer		},
 	{ "video/*",					tracker_extract_totem		},
#endif
#endif
#endif
  
  	/* Audio extractors */
#ifdef HAVE_GSTREAMER
	{ "audio/*",					tracker_extract_gstreamer	},
 	{ "audio/*",					tracker_extract_mplayer		},

#else
#ifdef HAVE_LIBXINE
	{ "audio/*",					tracker_extract_xine		},
 	{ "audio/*",					tracker_extract_mplayer		},
#else
#ifdef USING_EXTERNAL_VIDEO_PLAYER
 	{ "audio/*",					tracker_extract_mplayer		},
 	{ "audio/*",					tracker_extract_totem		},
#endif
#endif
#endif
  
     /* Image extractors */
	{ "image/png",					tracker_extract_png		},
#ifdef HAVE_LIBEXIF
	{ "image/jpeg",					tracker_extract_exif		},
#endif
	{ "image/*",					tracker_extract_imagemagick	},
	{ "",						NULL				}
};
  

void
tracker_child_cb (gpointer user_data)
{
	struct 	rlimit mem_limit, cpu_limit;
	int	timeout = GPOINTER_TO_INT (user_data);

	/* set cpu limit */
	getrlimit (RLIMIT_CPU, &cpu_limit);
	cpu_limit.rlim_cur = timeout;
	cpu_limit.rlim_max = timeout+1;

	if (setrlimit (RLIMIT_CPU, &cpu_limit) != 0) {
		g_printerr ("Error trying to set resource limit for cpu\n");
	}

	/* Set memory usage to max limit (MAX_MEM) */
	getrlimit (RLIMIT_AS, &mem_limit);
 	mem_limit.rlim_cur = MAX_MEM*1024*1024;
	if (setrlimit (RLIMIT_AS, &mem_limit) != 0) {
		g_printerr ("Error trying to set resource limit for memory usage\n");
	}

	
}


gboolean
tracker_spawn (char **argv, int timeout, char **stdout, int *exit_status)
{

	return g_spawn_sync (NULL,
			  argv,
			  NULL,
			  G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
			  tracker_child_cb,
			  GINT_TO_POINTER (timeout),
			  stdout,
			  NULL,
			  exit_status,
			  NULL);

}


static GHashTable *
tracker_get_file_metadata (const char *uri, char *mime)
{

	GHashTable 		*meta_table;
	char			*uri_in_locale;

	
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
		MimeToExtractor *p;

		for (p = extractors; p->extractor; ++p) {
			if (g_pattern_match_simple (p->mime, mime)) {
				(*p->extractor)(uri_in_locale, meta_table);

				if (g_hash_table_size (meta_table) == 0) {
					continue;
				}

				g_free (uri_in_locale);
				g_free (mime);

				return meta_table;
			}
		}

		g_free (mime);
	}

	g_free (uri_in_locale);

	return NULL;
}


static void
get_meta_table_data (gpointer pkey, gpointer pvalue, gpointer user_data)
{
	char *value;

	g_return_if_fail (pkey || pvalue);

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


int
main (int argc, char **argv)
{
	GHashTable	*meta;
	char		*filename;
	struct 		rlimit rl;

	/* Obtain the current limits memory usage limits */
	getrlimit (RLIMIT_AS, &rl);

	/* Set virtual memory usage to max limit (128MB) - most extractors use mmap() so only virtual memory can be effectively limited */
 	rl.rlim_cur = 128*1024*1024;
	if (setrlimit (RLIMIT_AS, &rl) != 0) g_printerr ("Error trying to set resource limit for tracker-extract\n");


	g_set_application_name ("tracker-extract");

	setlocale (LC_ALL, "");

	
	if ((argc == 1) || (argc > 3)) {
		g_print ("usage: tracker-extract file [mimetype]\n");
		return 0;
	}

	filename = g_filename_to_utf8 (argv[1], -1, NULL, NULL, NULL);

	if (!filename) {
		g_warning ("locale to UTF8 failed for filename!");
		return 1;
	}

	if (argc == 3) {
		char *mime;

		mime = g_locale_to_utf8 (argv[2], -1, NULL, NULL, NULL);

		if (!mime) {
			g_warning ("locale to UTF8 failed for mime!");
			return 1;
		}

		/* mime will be free in tracker_get_file_metadata() */
		meta = tracker_get_file_metadata (filename, mime);
	} else {
		meta = tracker_get_file_metadata (filename, NULL);
	}

	g_free (filename);

	if (meta) {
		g_hash_table_foreach (meta, get_meta_table_data, NULL);
		g_hash_table_destroy (meta);
	}

	return 0;
}
