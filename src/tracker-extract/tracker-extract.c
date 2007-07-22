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
#define MAX_MEM_AMD64 512

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>
#endif

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
#ifdef HAVE_LIBXML2
void tracker_extract_html		(gchar *, GHashTable *);
#endif
#ifdef HAVE_POPPLER
void tracker_extract_pdf	(gchar *, GHashTable *);
#endif
void tracker_extract_abw	(gchar *, GHashTable *);
#ifdef HAVE_LIBGSF
void tracker_extract_msoffice	(gchar *, GHashTable *);
#endif
#ifdef HAVE_EXEMPI
void tracker_extract_xmp	(gchar *, GHashTable *);
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
#ifdef HAVE_LIBXML2
 	{ "text/html",							tracker_extract_html	},
 	{ "application/xhtml+xml",	tracker_extract_html	},
#endif
#ifdef HAVE_POPPLER
 	{ "application/pdf",				tracker_extract_pdf		},
#endif
	{ "application/x-abiword",			tracker_extract_abw		},
#ifdef HAVE_LIBGSF
 	{ "application/msword",				tracker_extract_msoffice	},
 	{ "application/vnd.ms-*",			tracker_extract_msoffice	},
#endif
#ifdef HAVE_EXEMPI
 	{ "application/rdf+xml",			tracker_extract_xmp		},
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
  
	{ "audio/mp3",					tracker_extract_mp3		},

     /* Image extractors */
	{ "image/png",					tracker_extract_png		},
#ifdef HAVE_LIBEXIF
	{ "image/jpeg",					tracker_extract_exif		},
#endif
	{ "image/*",					tracker_extract_imagemagick	},
	{ "",						NULL				}
};
  

gboolean
set_memory_rlimits (void)
{
	struct	rlimit rl;
	int	fail = 0;

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

	if(fail)
		g_printerr ("Error trying to set memory limit for tracker-extract\n");
	return ! fail;
}


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

	set_memory_rlimits();

	/* Set child's niceness to 19 */
	nice (19);		
}


gboolean
tracker_spawn (char **argv, int timeout, char **tmp_stdout, int *exit_status)
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

#ifdef HAVE_EXEMPI
void
tracker_append_string_to_hash_table( GHashTable *metadata, const gchar *key, const gchar *value, gboolean append )
{
	char *new_value;
	if (append) {
		char *orig;
		if (g_hash_table_lookup_extended (metadata, key, NULL, (gpointer)&orig )) {
			new_value = g_strconcat (orig, " ", value, NULL);
		} else {
			new_value = g_strdup (value);
		}
	} else {
		new_value = g_strdup (value);
	}
	g_hash_table_insert (metadata, g_strdup(key), new_value);
}

void tracker_xmp_iter(XmpPtr xmp, XmpIteratorPtr iter, GHashTable *metadata, gboolean append);
void tracker_xmp_iter_simple(GHashTable *metadata, const char *schema, const char *path, const char *value, gboolean append);

/* We have an array, now recursively iterate over it's children.  Set 'append' to true so that all values of the array are added
   under one entry. */
void
tracker_xmp_iter_array(XmpPtr xmp, GHashTable *metadata, const char *schema, const char *path)
{
		XmpIteratorPtr iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
		tracker_xmp_iter (xmp, iter, metadata, TRUE);
		xmp_iterator_free (iter);
}

/* We have an array, now recursively iterate over it's children.  Set 'append' to false so that only one item is used. */
void
tracker_xmp_iter_alt_text(XmpPtr xmp, GHashTable *metadata, const char *schema, const char *path)
{
		XmpIteratorPtr iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
		tracker_xmp_iter (xmp, iter, metadata, FALSE);
		xmp_iterator_free (iter);
}

/* We have a simple element, but need to iterate over the qualifiers */
void
tracker_xmp_iter_simple_qual(XmpPtr xmp, GHashTable *metadata,
   const char *schema, const char *path, const char *value, gboolean append)
{
	XmpIteratorPtr iter = xmp_iterator_new(xmp, schema, path, XMP_ITER_JUSTCHILDREN | XMP_ITER_JUSTLEAFNAME);

	XmpStringPtr the_path = xmp_string_new ();
	XmpStringPtr the_prop = xmp_string_new ();

	char *locale = setlocale (LC_ALL, NULL);
	char *sep = strchr (locale,'.');
	if (sep) {
		locale[sep-locale] = '\0';
	}
	sep = strchr(locale,'_');
	if (sep) {
		locale[sep-locale] = '-';
	}

	gboolean ignore_element = FALSE;

	while(xmp_iterator_next (iter, NULL, the_path, the_prop, NULL))
	{
		const char *qual_path = xmp_string_cstr (the_path);
		const char *qual_value = xmp_string_cstr (the_prop);

		if (strcmp(qual_path,"xml:lang") == 0) {
			/* is this a language we should ignore? */
			if (strcmp (qual_value, "x-default") != 0 && strcmp (qual_value, "x-repair") != 0 && strcmp (qual_value, locale) != 0) {
				ignore_element = TRUE;
				break;
			}
		}
	}

	if (!ignore_element) {
		tracker_xmp_iter_simple (metadata, schema, path, value, append);
	}

	xmp_string_free (the_prop);
	xmp_string_free (the_path);

	xmp_iterator_free (iter);
}

/* We have a simple element.  Add any metadata we know about to the hash table  */
void
tracker_xmp_iter_simple(GHashTable *metadata,
   const char *schema, const char *path, const char *value, gboolean append)
{
	char *name = g_strdup (strchr (path, ':')+1);
	const char *index = strrchr (name, '[');
	if (index) {
		name[index-name] = '\0';
	}

	/* Dublin Core */
	if (strcmp(schema, NS_DC) == 0) {
		if (strcmp (name, "title") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Title", value, append);
		}
		else if (strcmp (name, "rights") == 0) {
			tracker_append_string_to_hash_table (metadata, "File:Copyright", value, append);
		}
		else if (strcmp (name, "creator") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Creator", value, append);
		}
		else if (strcmp (name, "description") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Description", value, append);
		}
		else if (strcmp (name, "date") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Date", value, append);
		}
		else if (strcmp (name, "keywords") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Keywords", value, append);
		}
	}
	/* Creative Commons */
	else if (strcmp (schema, NS_CC) == 0) {
		if (strcmp (name, "license") == 0) {
			tracker_append_string_to_hash_table (metadata, "File:License", value, append);
		}
	}
	free(name);
}

/* Iterate over the XMP, dispatching to the appropriate element type (simple, simple w/qualifiers, or an array) handler */
void
tracker_xmp_iter(XmpPtr xmp, XmpIteratorPtr iter, GHashTable *metadata, gboolean append)
{
	XmpStringPtr the_schema = xmp_string_new ();
	XmpStringPtr the_path = xmp_string_new ();
	XmpStringPtr the_prop = xmp_string_new ();

	uint32_t opt;
	while(xmp_iterator_next (iter, the_schema, the_path, the_prop, &opt))
	{
		const char *schema = xmp_string_cstr (the_schema);
		const char *path = xmp_string_cstr (the_path);
		const char *value = xmp_string_cstr (the_prop);

		if (XMP_IS_PROP_SIMPLE (opt)) {
			if (strcmp (path,"") != 0) {
				if (XMP_HAS_PROP_QUALIFIERS (opt)) {
					tracker_xmp_iter_simple_qual (xmp, metadata, schema, path, value, append);
				} else {
					tracker_xmp_iter_simple (metadata, schema, path, value, append);
				}
			}	
		}
		else if (XMP_IS_PROP_ARRAY (opt)) {
			if (XMP_IS_ARRAY_ALTTEXT (opt)) {
				tracker_xmp_iter_alt_text (xmp, metadata, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			} else {
				tracker_xmp_iter_array (xmp, metadata, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			}
		}
	}

	xmp_string_free (the_prop);
	xmp_string_free (the_path);
	xmp_string_free (the_schema);
}
#endif

void
tracker_read_xmp (const gchar *buffer, size_t len, GHashTable *metadata)
{
	#ifdef HAVE_EXEMPI
	xmp_init ();

	XmpPtr xmp = xmp_new_empty ();
	xmp_parse (xmp, buffer, len);
	if (xmp != NULL) {
		XmpIteratorPtr iter = xmp_iterator_new (xmp, NULL, NULL, XMP_ITER_PROPERTIES);
		tracker_xmp_iter (xmp, iter, metadata, FALSE);
		xmp_iterator_free (iter);
	
		xmp_free (xmp);
	}
	xmp_terminate ();
	#endif
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


int
main (int argc, char **argv)
{
	GHashTable	*meta;
	char		*filename;

	set_memory_rlimits();

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
