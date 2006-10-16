/* Tracker Extract - extracts embedded metadata from files
 *
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)	
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */

#include <locale.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <glib.h>

#include "config.h"

typedef enum {
	IGNORE_METADATA,
	NO_METADATA,
	DOC_METADATA,
	IMAGE_METADATA,
	VIDEO_METADATA,
	AUDIO_METADATA
} MetadataFileType;


/* document mime type specific metadata groups - NB mime types below are prefixes */
static  char *doc_mime_types[] = {"application/msword","application/pdf", "application/postscript","application/x-dvi",
				  "application/vnd.ms-excel", "vnd.ms-powerpoint", "application/vnd.oasis",
				  "application/vnd.sun.xml",  "application/vnd.stardivision", "application/x-abiword",
				  "text/html","text/sgml", "text/xml" };



typedef void (*MetadataExtractFunc)(gchar *, GHashTable *);
typedef struct {
	char                 *mime;
	MetadataExtractFunc  extractor;
} MimeToExtractor;

void tracker_extract_oasis (gchar *, GHashTable *);
void tracker_extract_ps    (gchar *, GHashTable *);
#ifdef HAVE_POPPLER
void tracker_extract_pdf   (gchar *, GHashTable *);
#endif
void tracker_extract_abw   (gchar *, GHashTable *);
#ifdef HAVE_LIBGSF
void tracker_extract_msoffice   (gchar *, GHashTable *);
#endif
void tracker_extract_mp3   (gchar *, GHashTable *);
#ifdef HAVE_VORBIS
void tracker_extract_vorbis   (gchar *, GHashTable *);
#endif
#ifdef HAVE_LIBPNG
void tracker_extract_png   (gchar *, GHashTable *);
#endif
#ifdef HAVE_LIBEXIF
void tracker_extract_exif   (gchar *, GHashTable *);
#endif
void tracker_extract_imagemagick   (gchar *, GHashTable *);
void tracker_extract_mplayer   (gchar *, GHashTable *);

MimeToExtractor extractors[] = {
	/* Document extractors */
	{ "application/vnd.oasis.opendocument.*",  tracker_extract_oasis       },
	{ "application/postscript",                tracker_extract_ps          },
#ifdef HAVE_POPPLER
	{ "application/pdf",                       tracker_extract_pdf         },
#endif
	{ "application/x-abiword",                 tracker_extract_abw         },
#ifdef HAVE_LIBGSF
	{ "application/msword",                    tracker_extract_msoffice    },
	{ "application/vnd.ms-*",                  tracker_extract_msoffice    },
#endif


	/* Video extractors */
	{ "video/*",                               tracker_extract_mplayer     },


	/* Audio extractors */
	{ "audio/mpeg",                            tracker_extract_mp3         },
	{ "audio/x-mp3",                           tracker_extract_mp3         },
	{ "audio/x-mpeg",                          tracker_extract_mp3         },
#ifdef HAVE_VORBIS
	{ "audio/x-vorbis+ogg",                    tracker_extract_vorbis      },
#endif
	{ "audio/*",                               tracker_extract_mplayer     },


   /* Image extractors */
#ifdef HAVE_LIBPNG
	{ "image/png",                             tracker_extract_png         },
#endif
#ifdef HAVE_LIBEXIF
	{ "image/jpeg",                            tracker_extract_exif        },
#endif
	{ "image/*",                               tracker_extract_imagemagick },
	{ "",                                      NULL                        }
};

static MetadataFileType
get_metadata_type (const char *mime)
{
	int i;
	int num_elements;

	if (!mime) {
		return NO_METADATA;
	}
	
	if (g_str_has_prefix (mime, "image")) {
		return IMAGE_METADATA;		
	} else { 
		if (g_str_has_prefix (mime, "video")) {
			return VIDEO_METADATA;		
		} else { 
			if (g_str_has_prefix (mime, "audio")) {
				return AUDIO_METADATA;		
			} else { 
				num_elements = sizeof (doc_mime_types) / sizeof (char *);
				for (i =0; i < num_elements; i++ ) {
					if (g_str_has_prefix (mime, doc_mime_types [i] )) {
						return DOC_METADATA;
					}
				}
			}
		}
	}
	return NO_METADATA;
}



static GHashTable *
tracker_get_file_metadata (const char *uri, char *mime)
{

	GHashTable 		*meta_table;
	char			*uri_in_locale = NULL;

	
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
		MimeToExtractor  *p;
		for (p = extractors; p->extractor; ++p) {
			if (g_pattern_match_simple (p->mime, mime)) {
				(*p->extractor)(uri_in_locale, meta_table);
				if (g_hash_table_size (meta_table) == 0)
					continue;
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

	g_return_if_fail (pkey || pvalue);
	
	char *value = g_locale_to_utf8 ((char *)  pvalue, -1, NULL, NULL, NULL);

	if (value && (strlen (value) > 0)) {
	
		/* replace any embedded semicolons or "=" as we use them for delimiters */
		value = g_strdelimit (value, ";", ',');
		value = g_strdelimit (value, "=", '-');
		value = g_strstrip (value);

		g_print ("%s=%s;\n", (char *)pkey, value);
		g_free (value);
	}


	
}

int
main (int argc, char **argv) 
{

	GHashTable *meta;
	char *filename;

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
		char *mime = g_locale_to_utf8 (argv[2], -1, NULL, NULL, NULL);

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
