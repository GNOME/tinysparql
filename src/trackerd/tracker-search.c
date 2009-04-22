/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include "config.h"

#include <string.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-type-utils.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-manager.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>

#include "tracker-dbus.h"
#include "tracker-search.h"
#include "tracker-marshal.h"

#define TRACKER_SEARCH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_SEARCH, TrackerSearchPrivate))

#define DEFAULT_SEARCH_MAX_HITS      1024

typedef struct {
	TrackerConfig	   *config;
	TrackerLanguage    *language;
} TrackerSearchPrivate;

static void tracker_search_finalize (GObject *object);

G_DEFINE_TYPE(TrackerSearch, tracker_search, G_TYPE_OBJECT)


static void
tracker_search_class_init (TrackerSearchClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_search_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerSearchPrivate));
}

static void
tracker_search_init (TrackerSearch *object)
{
}

static void
tracker_search_finalize (GObject *object)
{
	TrackerSearchPrivate *priv;

	priv = TRACKER_SEARCH_GET_PRIVATE (object);

	g_object_unref (priv->language);
	g_object_unref (priv->config);

	G_OBJECT_CLASS (tracker_search_parent_class)->finalize (object);
}

TrackerSearch *
tracker_search_new (TrackerConfig   *config,
		    TrackerLanguage *language)
{
	TrackerSearch	     *object;
	TrackerSearchPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);
	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	object = g_object_new (TRACKER_TYPE_SEARCH, NULL);

	priv = TRACKER_SEARCH_GET_PRIVATE (object);

	priv->config = g_object_ref (config);
	priv->language = g_object_ref (language);

	return object;
}


static const gchar *
search_utf8_p_from_offset_skipping_decomp (const gchar *str,
					   gint		offset)
{
	const gchar *p, *q;
	gchar	    *casefold, *normal;

	g_return_val_if_fail (str != NULL, NULL);

	p = str;

	while (offset > 0) {
		q = g_utf8_next_char (p);
		casefold = g_utf8_casefold (p, q - p);
		normal = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFC);
		offset -= g_utf8_strlen (normal, -1);
		g_free (casefold);
		g_free (normal);
		p = q;
	}

	return p;
}

static const char *
search_utf8_strcasestr_array (const gchar  *haystack,
			      const gchar **needles)
{
	gsize	      needle_len;
	gsize	      haystack_len;
	const gchar  *ret = NULL;
	const gchar  *needle;
	const gchar **array;
	gchar	     *p;
	gchar	     *casefold;
	gchar	     *caseless_haystack;
	gint	      i;

	g_return_val_if_fail (haystack != NULL, NULL);

	casefold = g_utf8_casefold (haystack, -1);
	caseless_haystack = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFC);
	g_free (casefold);

	if (!caseless_haystack) {
		return NULL;
	}

	haystack_len = g_utf8_strlen (caseless_haystack, -1);

	for (array = needles; *array; array++) {
		needle = *array;
		needle_len = g_utf8_strlen (needle, -1);

		if (needle_len == 0) {
			continue;
		}

		if (haystack_len < needle_len) {
			continue;
		}

		p = (gchar *) caseless_haystack;
		needle_len = strlen (needle);
		i = 0;

		while (*p) {
			if ((strncmp (p, needle, needle_len) == 0)) {
				ret = search_utf8_p_from_offset_skipping_decomp (haystack, i);
				goto done;
			}

			p = g_utf8_next_char (p);
			i++;
		}
	}

done:
	g_free (caseless_haystack);

	return ret;
}

static gint
search_get_word_break (const char *a)
{
	gchar **words;
	gint	value;

	words = g_strsplit_set (a, "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]" , -1);

	if (!words) {
		return 0;
	}

	value = strlen (words[0]);
	g_strfreev (words);

	return value;
}


static gboolean
search_is_word_break (const char a)
{
	const gchar *breaks = "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]";
	gint	     i;

	for (i = 0; breaks[i]; i++) {
		if (a == breaks[i]) {
			return TRUE;
		}
	}

	return FALSE;
}

static char *
search_highlight_terms (const gchar  *text,
			const gchar **terms)
{
	const gchar **p;
	GString      *s;
	const gchar  *str;
	gchar	     *text_copy;
	gint	      term_len;

	if (!text || !terms) {
		return NULL;
	}

	s = g_string_new ("");
	text_copy = g_strdup (text);

	for (p = terms; *p; p++) {
		const gchar *text_p;
		const gchar *single_term[2] = { *p, NULL };

		text_p = text_copy;

		while ((str = search_utf8_strcasestr_array (text_p, single_term))) {
			gchar *pre_snip;
			gchar *term;

			pre_snip = g_strndup (text_p, (str - text_p));
			term_len = search_get_word_break (str);
			term = g_strndup (str, term_len);

			text_p = str + term_len;
			g_string_append_printf (s, "%s<b>%s</b>", pre_snip, term);

			g_free (pre_snip);
			g_free (term);
		}

		if (text_p) {
			g_string_append (s, text_p);
		}
	}

	g_free (text_copy);
	text_copy = g_string_free (s, FALSE);

	return text_copy;
}

static gchar *
search_get_snippet (const gchar  *text,
		    const gchar **terms,
		    gint	  length)
{
	const gchar *ptr = NULL;
	const gchar *end_ptr;
	const gchar *tmp;
	gint	     i;
	gint	     text_len;

	if (!text || !terms) {
		return NULL;
	}

	text_len = strlen (text);
	ptr = search_utf8_strcasestr_array (text, terms);

	if (ptr) {
		gchar *snippet;
		gchar *snippet_escaped;
		gchar *snippet_highlighted;

		tmp = ptr;
		i = 0;

		/* Get snippet before the matching term, try to keep it in the middle */
		while (ptr != NULL && *ptr != '\n' && i < length / 2) {
			ptr = g_utf8_find_prev_char (text, ptr);
			i++;
		}

		if (!ptr) {
			/* No newline was found before highlighted term */
			ptr = text;
		} else if (*ptr != '\n') {
			/* Try to start beginning of snippet on a word break */
			while (!search_is_word_break (*ptr) && ptr != tmp) {
				ptr = g_utf8_find_next_char (ptr, NULL);
			}
		} else {
			ptr = g_utf8_find_next_char (ptr, NULL);
		}

		end_ptr = ptr;
		i = 0;

		while (end_ptr != NULL && *end_ptr != '\n' && i < length) {
			end_ptr = g_utf8_find_next_char (end_ptr, NULL);
			i++;
		}

		if (end_ptr && *end_ptr != '\n') {
			/* Try to end snippet on a word break */
			while (!search_is_word_break (*end_ptr) && end_ptr != tmp) {
				end_ptr = g_utf8_find_prev_char (text, end_ptr);
			}

			if (end_ptr == tmp) {
				end_ptr = NULL;
			}
		}

		if (!end_ptr) {
			/* Copy to the end of the string */
			snippet = g_strdup (ptr);
		} else {
			snippet = g_strndup (ptr, end_ptr - ptr);
		}

		snippet_escaped = g_markup_escape_text (snippet, -1);
		g_free (snippet);

		snippet_highlighted = search_highlight_terms (snippet_escaped, terms);
		g_free (snippet_escaped);

		return snippet_highlighted;
	}

	ptr = text;
	i = 0;

	while ((ptr = g_utf8_next_char (ptr)) && ptr <= text_len + text && i < length) {
		i++;

		if (*ptr == '\n') {
			break;
		}
	}

	if (ptr > text_len + text) {
		ptr = g_utf8_prev_char (ptr);
	}

	if (ptr) {
		gchar *snippet;
		gchar *snippet_escaped;
		gchar *snippet_highlighted;

		snippet = g_strndup (text, ptr - text);
		snippet_escaped = g_markup_escape_text (snippet, ptr - text);
		snippet_highlighted = search_highlight_terms (snippet_escaped, terms);

		g_free (snippet);
		g_free (snippet_escaped);

		return snippet_highlighted;
	} else {
		return NULL;
	}
}

void
tracker_search_get_snippet (TrackerSearch	   *object,
			    const gchar		   *uri,
			    const gchar		   *search_text,
			    DBusGMethodInvocation  *context,
			    GError		  **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	GError		   *actual_error = NULL;
	guint		    request_id;
	gchar		   *snippet = NULL;
	guint32		    resource_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (search_text != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get snippet, "
				  "search text:'%s', id:'%s'",
				  search_text,
				  uri);

	if (tracker_is_empty_string (search_text)) {
		g_set_error (&actual_error,
			     TRACKER_DBUS_ERROR,
			     0,
			     "No search term was specified");
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	iface = tracker_db_manager_get_db_interface ();

	resource_id = tracker_data_query_resource_id (uri);
	if (!resource_id) {
		g_set_error (&actual_error,
			     TRACKER_DBUS_ERROR,
			     0,
			     "Service URI '%s' not found",
			     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* TODO: Port to SPARQL */
#if 0
	result_set = tracker_data_manager_exec_proc (iface,
					   "GetAllContents",
					   tracker_guint_to_string (resource_id),
					   NULL);

	if (result_set) {
		TrackerSearchPrivate  *priv;
		gchar		     **strv;
		gchar		      *text;

		priv = TRACKER_SEARCH_GET_PRIVATE (object);

		tracker_db_result_set_get (result_set, 0, &text, -1);

		strv = tracker_parser_text_into_array (search_text,
						       priv->language,
						       tracker_config_get_max_word_length (priv->config),
						       tracker_config_get_min_word_length (priv->config));
		if (strv && strv[0]) {
			snippet = search_get_snippet (text, (const gchar **) strv, 120);
		}

		g_strfreev (strv);
		g_free (text);
		g_object_unref (result_set);
	}
#endif

	/* Sanity check snippet, using NULL will crash */
	if (!snippet || !g_utf8_validate (snippet, -1, NULL) ) {
		snippet = g_strdup (" ");
	}

	dbus_g_method_return (context, snippet);

	g_free (snippet);

	tracker_dbus_request_success (request_id);
}

void
tracker_search_suggest (TrackerSearch	       *object,
			const gchar	       *search_text,
			gint			max_dist,
			DBusGMethodInvocation  *context,
			GError		      **error)
{
	GError		     *actual_error = NULL;
	TrackerSearchPrivate *priv;
	guint		      request_id;
	gchar		     *value;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (search_text != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to for suggested words, "
				  "term:'%s', max dist:%d",
				  search_text,
				  max_dist);

	priv = TRACKER_SEARCH_GET_PRIVATE (object);

	/* TODO: Port to SPARQL */
#if 0
	value = tracker_db_index_get_suggestion (priv->resources_index,
						 search_text,
						 max_dist);

	if (!value) {
		g_set_error (&actual_error,
			     TRACKER_DBUS_ERROR,
			     0,
			     "Possible data error in index, no suggestions given for '%s'",
			     search_text);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	} else {
		dbus_g_method_return (context, value);
		tracker_dbus_request_comment (request_id,
				      "Suggested spelling for '%s' is '%s'",
				      search_text, value);
		g_free (value);
	}
#endif

	tracker_dbus_request_success (request_id);
}

