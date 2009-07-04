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

void
tracker_search_get_snippet (TrackerSearch	   *object,
			    const gchar		   *uri,
			    const gchar		   *search_text,
			    DBusGMethodInvocation  *context,
			    GError		  **error)
{
#if 0
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

	/* Sanity check snippet, using NULL will crash */
	if (!snippet || !g_utf8_validate (snippet, -1, NULL) ) {
		snippet = g_strdup (" ");
	}

	dbus_g_method_return (context, snippet);

	g_free (snippet);

	tracker_dbus_request_success (request_id);
#else
	GError *actual_error = NULL;
	gint request_id;

	/* TODO: Port to SPARQL */
	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (search_text != NULL, context);

	tracker_dbus_request_new (request_id,
				  "%s: term:'%s'",
				  __FUNCTION__,
				  search_text);
	tracker_dbus_request_failed (request_id,
				     &actual_error,
				     _("Function unsupported or not ported to SparQL yet"));
	dbus_g_method_return_error (context, actual_error);
	g_error_free (actual_error);
#endif
}

void
tracker_search_suggest (TrackerSearch	       *object,
			const gchar	       *search_text,
			gint			max_dist,
			DBusGMethodInvocation  *context,
			GError		      **error)
{
#if 0
	TrackerSearchPrivate *priv;
	guint		      request_id;
 	GError		     *actual_error = NULL;
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

	tracker_dbus_request_success (request_id);
#else
	GError *actual_error = NULL;
	gint request_id;

	/* TODO: Port to SPARQL */
	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (search_text != NULL, context);

	tracker_dbus_request_new (request_id,
				  "%s: term:'%s', max dist:%d",
				  __FUNCTION__,
				  search_text,
				  max_dist);
	tracker_dbus_request_failed (request_id,
				     &actual_error,
				     _("Function unsupported or not ported to SparQL yet"));
	dbus_g_method_return_error (context, actual_error);
	g_error_free (actual_error);
#endif
}

