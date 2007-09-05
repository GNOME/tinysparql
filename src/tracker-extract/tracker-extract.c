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

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef OS_WIN32
#include <sys/resource.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include <glib.h>

#include "tracker-extract.h"

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
void tracker_extract_html       (gchar *, GHashTable *);
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
 	{ "text/html",                                  tracker_extract_html            },
 	{ "application/xhtml+xml",                      tracker_extract_html            },
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


static gboolean
read_day (gchar *day, gint *ret)
{
        guint64 val = g_ascii_strtoull (day, NULL, 10);

        if (val == G_MAXUINT64) {
                return FALSE;
        } else {
                *ret = CLAMP (val, 1, 31);
                return TRUE;
        }
}


static gboolean
read_month (gchar *month, gint *ret)
{
        if (g_ascii_isdigit (month[0])) {
                /* month is already given in a numerical form */
                guint64 val = g_ascii_strtoull (month, NULL, 10) - 1;

                if (val == G_MAXUINT64) {
                        return FALSE;
                } else {
                        *ret = CLAMP (val, 0, 11);
                        return TRUE;
                }
        } else {
                /* month is given by its name */
                gchar *months[] = {
                        "Ja", "Fe", "Mar", "Av", "Ma", "Jun",
                        "Jul", "Au", "Se", "Oc", "No", "De",
                        NULL };
                gchar **tmp;
                gint i;

                for (tmp = months, i = 0; *tmp; tmp++, i++) {
                        if (g_str_has_prefix (month, *tmp)) {
                                *ret = i;
                                return TRUE;
                        }
                }

                return FALSE;
        }
}


static gboolean
read_year (gchar *year, gint *ret)
{
        guint64 val = g_ascii_strtoull (year, NULL, 10);

        if (val == G_MAXUINT64) {
                return FALSE;
        } else {
                *ret = CLAMP (val, 0, G_MAXINT) - 1900;
                return TRUE;
        }
}


static gboolean
read_time (gchar *time, gint *ret_hour, gint *ret_min, gint *ret_sec)
{
        /* Hours */
        guint64 val = g_ascii_strtoull (time, &time, 10);
        if (val == G_MAXUINT64 || *time != ':') {
                return FALSE;
        }
        *ret_hour = CLAMP (val, 0, 24);
        time++;

        /* Minutes */
        val = g_ascii_strtoull (time, &time, 10);
        if (val == G_MAXUINT64) {
                return FALSE;
        }
        *ret_min = CLAMP (val, 0, 99);

        if (*time == ':') {
                /* Yeah! We have seconds. */
                time++;
                val = g_ascii_strtoull (time, &time, 10);
                if (val == G_MAXUINT64) {
                        return FALSE;
                }
                *ret_sec = CLAMP (val, 0, 99);
        }

        return TRUE;
}


static gboolean
read_timezone (gchar *timezone, gchar *ret, gsize ret_len)
{
        /* checks that we are not reading word "GMT" instead of timezone */
        if (timezone[0] && g_ascii_isdigit (timezone[1])) {
                gchar *n = timezone + 1;  /* "+1" to not read timezone sign */
                gint  hours, minutes;

                if (strlen (n) < 4) {
                        return FALSE;  
                }

                #define READ_PAIR(ret, min, max)                        \
                {                                                       \
                        gchar buff[3];                                  \
                        guint64 val;                                    \
                        buff[0] = n[0];                                 \
                        buff[1] = n[1];                                 \
                        buff[2] = '\0';                                 \
                                                                        \
                        val = g_ascii_strtoull (buff, NULL, 10);        \
                        if (val == G_MAXUINT64) {                       \
                                return FALSE;                           \
                        }                                               \
                        ret = CLAMP (val, min, max);                    \
                        n += 2;                                         \
                }

                READ_PAIR (hours, 0, 24);
                if (*n == ':') {
                        /* that should not happen, but he... */
                        n++;
                }
                READ_PAIR (minutes, 0, 99);

                g_snprintf (ret, ret_len,
                            "%c%.2d%.2d",
                            (timezone[0] == '-' ? '-' : '+'),
                            hours, minutes);

                #undef READ_PAIR

                return TRUE;

        } else if (g_ascii_isalpha (timezone[1])) {
                /* GMT, so we keep current time */
                if (ret_len >= 2) {
                        ret[0] = 'Z';
                        ret[1] = '\0';
                        return TRUE;

                } else {
                        return FALSE;
                }

        } else {
                return FALSE;
        }
}


gchar *
tracker_generic_date_extractor (gchar *date, steps steps_to_do[])
{
        gchar buffer[20], timezone_buffer[6];
        gchar **date_parts;
        gsize count;
        guint i;

        g_return_val_if_fail (date, NULL);

        struct tm tm;
        memset (&tm, 0, sizeof (struct tm));

        date_parts = g_strsplit (date, " ", 0);

        for (i = 0; date_parts[i] && steps_to_do[i] != LAST_STEP; i++) {
                gchar *part = date_parts[i];

                switch (steps_to_do[i]) {
                        case TIME: {
                                if (!read_time (part, &tm.tm_hour, &tm.tm_min, &tm.tm_sec)) {
                                        goto error;
                                }
                                break;
                        }
                        case TIMEZONE: {
                                if (!read_timezone (part,
                                                    timezone_buffer,
                                                    G_N_ELEMENTS (timezone_buffer))) {
                                        goto error;
                                }
                                break;
                        }
                        case DAY_PART: {
                                if (strcmp (part, "AM") == 0) {
                                        /* We do nothing... */
                                } else if (strcmp (part, "PM") == 0) {
                                        tm.tm_hour += 12;
                                }
                                break;
                        }
                        case DAY_STR: {
                                /* We do not care about monday, tuesday, etc. */
                                break;
                        }
                        case DAY: {
                                if (!read_day (part, &tm.tm_mday)) {
                                        goto error;
                                }
                                break;
                        }
                        case MONTH: {
                                if (!read_month (part, &tm.tm_mon)) {
                                        goto error;
                                }
                                break;
                        }
                        case YEAR : {
                                if (!read_year (part, &tm.tm_year)) {
                                        goto error;
                                }
                                break;
                        }
                        default: {
                                /* that cannot happen! */
                                g_strfreev (date_parts);
                                g_return_val_if_reached (NULL);
                        }
                }
        }

        count = strftime (buffer, sizeof (buffer), "%FT%T", &tm);

        g_strfreev (date_parts);

        if (count > 0) {
                return g_strconcat (buffer, timezone_buffer, NULL);
        } else {
                return NULL;
        }

 error:
        g_strfreev (date_parts);
        return NULL;
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

	set_memory_rlimits();

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


#ifdef HAVE_EXEMPI
void
tracker_append_string_to_hash_table (GHashTable *metadata, const gchar *key, const gchar *value, gboolean append)
{
	gchar *new_value;

	if (append) {
		gchar *orig;
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


void tracker_xmp_iter (XmpPtr xmp, XmpIteratorPtr iter, GHashTable *metadata, gboolean append);
void tracker_xmp_iter_simple (GHashTable *metadata, const gchar *schema, const gchar *path, const gchar *value, gboolean append);


/* We have an array, now recursively iterate over it's children.  Set 'append' to true so that all values of the array are added
   under one entry. */
void
tracker_xmp_iter_array (XmpPtr xmp, GHashTable *metadata, const gchar *schema, const gchar *path)
{
		XmpIteratorPtr iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
		tracker_xmp_iter (xmp, iter, metadata, TRUE);
		xmp_iterator_free (iter);
}


/* We have an array, now recursively iterate over it's children.  Set 'append' to false so that only one item is used. */
void
tracker_xmp_iter_alt_text (XmpPtr xmp, GHashTable *metadata, const gchar *schema, const gchar *path)
{
		XmpIteratorPtr iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
		tracker_xmp_iter (xmp, iter, metadata, FALSE);
		xmp_iterator_free (iter);
}


/* We have a simple element, but need to iterate over the qualifiers */
void
tracker_xmp_iter_simple_qual (XmpPtr xmp, GHashTable *metadata,
                              const gchar *schema, const gchar *path, const gchar *value, gboolean append)
{
	XmpIteratorPtr iter = xmp_iterator_new(xmp, schema, path, XMP_ITER_JUSTCHILDREN | XMP_ITER_JUSTLEAFNAME);

	XmpStringPtr the_path = xmp_string_new ();
	XmpStringPtr the_prop = xmp_string_new ();

	gchar *locale = setlocale (LC_ALL, NULL);
	gchar *sep = strchr (locale,'.');
	if (sep) {
		locale[sep - locale] = '\0';
	}
	sep = strchr (locale, '_');
	if (sep) {
		locale[sep - locale] = '-';
	}

	gboolean ignore_element = FALSE;

	while (xmp_iterator_next (iter, NULL, the_path, the_prop, NULL)) {
		const gchar *qual_path = xmp_string_cstr (the_path);
		const gchar *qual_value = xmp_string_cstr (the_prop);

		if (strcmp (qual_path, "xml:lang") == 0) {
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


/* We have a simple element. Add any metadata we know about to the hash table  */
void
tracker_xmp_iter_simple (GHashTable *metadata,
                         const gchar *schema, const gchar *path, const gchar *value, gboolean append)
{
	gchar *name = g_strdup (strchr (path, ':')+1);
	const gchar *index = strrchr (name, '[');
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
tracker_xmp_iter (XmpPtr xmp, XmpIteratorPtr iter, GHashTable *metadata, gboolean append)
{
	XmpStringPtr the_schema = xmp_string_new ();
	XmpStringPtr the_path = xmp_string_new ();
	XmpStringPtr the_prop = xmp_string_new ();

	uint32_t opt;
	while (xmp_iterator_next (iter, the_schema, the_path, the_prop, &opt)) {
		const gchar *schema = xmp_string_cstr (the_schema);
		const gchar *path = xmp_string_cstr (the_path);
		const gchar *value = xmp_string_cstr (the_prop);

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
tracker_get_file_metadata (const gchar *uri, gchar *mime)
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


gint
main (gint argc, gchar *argv[])
{
	GHashTable	*meta;
	gchar		*filename;

	set_memory_rlimits ();

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
		gchar *mime = g_locale_to_utf8 (argv[2], -1, NULL, NULL, NULL);

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
