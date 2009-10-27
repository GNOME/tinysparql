/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
#include <unistd.h>

#include <gmodule.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-sparql-builder.h>

#include "tracker-dbus.h"
#include "tracker-extract.h"
#include "tracker-main.h"
#include "tracker-marshal.h"

#ifdef HAVE_LIBSTREAMANALYZER
#include "tracker-topanalyzer.h"
#endif /* HAVE_STREAMANALYZER */

#define MAX_EXTRACT_TIME 10

#define TRACKER_EXTRACT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_EXTRACT, TrackerExtractPrivate))

extern gboolean debug;

typedef struct {
	GArray *specific_extractors;
	GArray *generic_extractors;
	gboolean disable_shutdown;
	gboolean force_internal_extractors;
} TrackerExtractPrivate;

typedef struct { 
	const GModule *module;
	const TrackerExtractData *edata; 
}  ModuleData;

enum {
	QUEUE_THUMBNAIL,
	LAST_SIGNAL
};

static void tracker_extract_finalize (GObject *object);

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE(TrackerExtract, tracker_extract, G_TYPE_OBJECT)

static void
tracker_extract_class_init (TrackerExtractClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_extract_finalize;

	signals[QUEUE_THUMBNAIL] =
		g_signal_new ("queue-thumbnail",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (TrackerExtractPrivate));
}

static void
tracker_extract_init (TrackerExtract *object)
{
#ifdef HAVE_LIBSTREAMANALYZER
	tracker_topanalyzer_init ();
#endif /* HAVE_STREAMANALYZER */
}

static void
tracker_extract_finalize (GObject *object)
{
	TrackerExtractPrivate *priv;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

#ifdef HAVE_LIBSTREAMANALYZER
	tracker_topanalyzer_shutdown ();
#endif /* HAVE_STREAMANALYZER */

	g_array_free (priv->specific_extractors, TRUE);
	g_array_free (priv->generic_extractors, TRUE);

	G_OBJECT_CLASS (tracker_extract_parent_class)->finalize (object);
}

TrackerExtract *
tracker_extract_new (gboolean disable_shutdown,
		     gboolean force_internal_extractors)
{
	TrackerExtract *object;
	TrackerExtractPrivate *priv;
	GDir *dir;
	GError *error;
	const gchar *name;
	GArray *specific_extractors;
	GArray *generic_extractors;

	if (!g_module_supported ()) {
		g_error ("Modules are not supported for this platform");
		return NULL;
	}

	error = NULL;
	dir = g_dir_open (MODULESDIR, 0, &error);

	if (!dir) {
		g_error ("Error opening modules directory: %s", error->message);
		g_error_free (error);
		return NULL;
	}

#ifdef HAVE_LIBSTREAMANALYZER
	if (!force_internal_extractors) {
		g_message ("Adding extractor for libstreamanalyzer");
		g_message ("  Generic  match for ALL (tried first before our module)");
		g_message ("  Specific match for NONE (fallback to our modules)");
	} else {
		g_message ("Not using libstreamanalyzer");
		g_message ("  It is available but disabled by command line");
	}
#endif /* HAVE_STREAMANALYZER */
	specific_extractors = g_array_new (FALSE,
					   TRUE,
					   sizeof (ModuleData));

	generic_extractors = g_array_new (FALSE,
					  TRUE,
					  sizeof (ModuleData));

	while ((name = g_dir_read_name (dir)) != NULL) {
		TrackerExtractDataFunc func;
		GModule *module;
		gchar *module_path;

		if (!g_str_has_suffix (name, "." G_MODULE_SUFFIX)) {
			continue;
		}

		module_path = g_build_filename (MODULESDIR, name, NULL);

		module = g_module_open (module_path, G_MODULE_BIND_LOCAL);

		if (!module) {
			g_warning ("Could not load module '%s': %s", 
				   name, 
				   g_module_error ());
			g_free (module_path);
			continue;
		}

		g_module_make_resident (module);

		if (g_module_symbol (module, "tracker_get_extract_data", (gpointer *) &func)) {
			ModuleData mdata;
		
			mdata.module = module;
			mdata.edata = (func) ();

			g_message ("Adding extractor:'%s' with:",
				   g_module_name ((GModule*) mdata.module));

			for (; mdata.edata->mime; mdata.edata++) {
				if (G_UNLIKELY (strchr (mdata.edata->mime, '*') != NULL)) {
					g_message ("  Generic  match for mime:'%s'",
						   mdata.edata->mime);
					g_array_append_val (generic_extractors, mdata);
				} else {
					g_message ("  Specific match for mime:'%s'",
						   mdata.edata->mime);
					g_array_append_val (specific_extractors, mdata);
				}
			}
		}

		g_free (module_path);
	}

	g_dir_close (dir);

	/* Set extractors */
	object = g_object_new (TRACKER_TYPE_EXTRACT, NULL);

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	priv->disable_shutdown = disable_shutdown;
	priv->force_internal_extractors = force_internal_extractors;

	priv->specific_extractors = specific_extractors;
	priv->generic_extractors = generic_extractors;

	return object;
}

static TrackerSparqlBuilder *
get_file_metadata (TrackerExtract *extract,
		   guint           request_id,
		   const gchar    *uri,
		   const gchar    *mime)
{
	TrackerExtractPrivate *priv;
	TrackerSparqlBuilder *statements;
	gchar *mime_used = NULL;
	gchar *content_type = NULL;

	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

	/* Create hash table to send back */
	statements = tracker_sparql_builder_new_update ();

	tracker_sparql_builder_insert_open (statements);

#ifdef HAVE_LIBSTREAMANALYZER
	if (!priv->force_internal_extractors) {
		tracker_dbus_request_comment (request_id,
					      "  Extracting with libstreamanalyzer...");
		
		tracker_topanalyzer_extract (uri, statements, &content_type);
		
		if (tracker_sparql_builder_get_length (statements) > 0) {
			g_free (content_type);
			tracker_sparql_builder_insert_close (statements);
			return statements;
		}
	} else {
		tracker_dbus_request_comment (request_id,
					      "  Extracting with internal extractors ONLY...");
	}
#endif /* HAVE_LIBSTREAMANALYZER */

	if (mime && *mime) {
		/* We know the mime */
		mime_used = g_strdup (mime);
		g_strstrip (mime_used);
	} else if (content_type && *content_type) {
		/* We know the mime from LSA */
		mime_used = content_type;
		g_strstrip (mime_used);
	} else {
		GFile *file;
		GFileInfo *info;
		GError *error = NULL;

		file = g_file_new_for_uri (uri);
		if (!file) {
			g_warning ("Could not create GFile for uri:'%s'",
				   uri);
			g_object_unref (statements);
			return NULL;
		}

		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
					  G_FILE_QUERY_INFO_NONE, 
					  NULL, 
					  &error);
	
		if (error || !info) {
			tracker_dbus_request_comment (request_id,
						      "  Could not create GFileInfo for file size check, %s",
						      error ? error->message : "no error given");
			g_error_free (error);
		
			if (info) {
				g_object_unref (info);
			}

			g_object_unref (file);
			g_object_unref (statements);
			return NULL;
		}

		mime_used = g_strdup (g_file_info_get_content_type (info));

		tracker_dbus_request_comment (request_id,
					      "  Guessing mime type as '%s' for uri:'%s'",
					      mime_used,
					      uri);

		g_object_unref (info);
		g_object_unref (file);
	}

	/* Now we have sanity checked everything, actually get the
	 * data we need from the extractors.
	 */
	if (mime_used) {
		guint i;

		for (i = 0; i < priv->specific_extractors->len; i++) {
			const TrackerExtractData *edata;
			ModuleData mdata;

			mdata = g_array_index (priv->specific_extractors, ModuleData, i);
			edata = mdata.edata;

			if (g_pattern_match_simple (edata->mime, mime_used)) {
				gint items;

				tracker_dbus_request_comment (request_id,
							      "  Extracting with module:'%s'",
							      g_module_name ((GModule*) mdata.module));

				(*edata->extract) (uri, statements);

				items = tracker_sparql_builder_get_length (statements);

				tracker_dbus_request_comment (request_id,
							      "  Found %d metadata items",
							      items);
				if (items == 0) {
					continue;
				}

				tracker_sparql_builder_insert_close (statements);

				g_free (mime_used);

				return statements;
			}
		}

		for (i = 0; i < priv->generic_extractors->len; i++) {
			const TrackerExtractData *edata;
			ModuleData mdata;

			mdata = g_array_index (priv->generic_extractors, ModuleData, i);
			edata = mdata.edata;

			if (g_pattern_match_simple (edata->mime, mime_used)) {
				gint items;

				tracker_dbus_request_comment (request_id,
							      "  Extracting with module:'%s'",
							      g_module_name ((GModule*) mdata.module));
				
				(*edata->extract) (uri, statements);

				items = tracker_sparql_builder_get_length (statements);

				tracker_dbus_request_comment (request_id,
							      "  Found %d metadata items",
							      items);
				if (items == 0) {
					continue;
				}

				tracker_sparql_builder_insert_close (statements);

				g_free (mime_used);

				return statements;
			}
		}

		g_free (mime_used);

		tracker_dbus_request_comment (request_id,
					      "  Could not find any extractors to handle metadata type");
	} else {
		tracker_dbus_request_comment (request_id,
					      "  No mime available, not extracting data");
	}

	tracker_sparql_builder_insert_close (statements);

	return statements;
}

void
tracker_extract_get_metadata_by_cmdline (TrackerExtract *object,
					 const gchar    *uri,
					 const gchar    *mime)
{
	guint       request_id;
	TrackerSparqlBuilder *statements = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	g_return_if_fail (uri != NULL);

	tracker_dbus_request_new (request_id,
				  "Command line request to extract metadata, "
				  "uri:'%s', mime:%s",
				  uri,
				  mime);

	/* NOTE: Don't reset the timeout to shutdown here */
	statements = get_file_metadata (object, request_id, uri, mime);

	if (statements) {
		tracker_dbus_request_info (request_id, "%s",
					   tracker_sparql_builder_get_result (statements));
		g_object_unref (statements);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_extract_get_pid (TrackerExtract	        *object,
			 DBusGMethodInvocation  *context,
			 GError		       **error)
{
	guint request_id;
	pid_t value;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "D-Bus request to get PID");
	
	value = getpid ();
	tracker_dbus_request_debug (request_id,
				    "PID is %d", value);

	dbus_g_method_return (context, value);

	tracker_dbus_request_success (request_id);
}

void
tracker_extract_get_metadata (TrackerExtract	     *object,
			      const gchar            *uri,
			      const gchar            *mime,
			      DBusGMethodInvocation  *context,
			      GError		    **error)
{
	guint request_id;
	TrackerExtractPrivate *priv;
	TrackerSparqlBuilder *sparql = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "D-Bus request to extract metadata, "
				  "uri:'%s', mime:%s",
				  uri,
				  mime);

	tracker_dbus_request_debug (request_id,
				    "  Resetting shutdown timeout");

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);
	
	tracker_main_quit_timeout_reset ();
	if (!priv->disable_shutdown) {
		alarm (MAX_EXTRACT_TIME);
	}

	sparql = get_file_metadata (object, request_id, uri, mime);

	if (sparql) {
		if (tracker_sparql_builder_get_length (sparql) > 0) {
			dbus_g_method_return (context, tracker_sparql_builder_get_result (sparql));
		} else {
			dbus_g_method_return (context, "");
		}
		g_object_unref (sparql);
		tracker_dbus_request_success (request_id);
	} else {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Could not get any metadata for uri:'%s' and mime:'%s'",
					     uri, 
					     mime);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
	}

	if (!priv->disable_shutdown) {
		/* Unset alarm so the extractor doesn't die when it's idle */
		alarm (0);
	}
}
