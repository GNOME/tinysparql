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

#include "tracker-main.h"
#include "tracker-dbus.h"
#include "tracker-extract.h"

#ifdef HAVE_STREAMANALYZER
#include "tracker-topanalyzer.h"
#endif

#define MAX_EXTRACT_TIME 5
#define TRACKER_EXTRACT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_EXTRACT, TrackerExtractPrivate))

typedef struct {
	GArray *extractors;
} TrackerExtractPrivate;

static void tracker_extract_finalize (GObject *object);

G_DEFINE_TYPE(TrackerExtract, tracker_extract, G_TYPE_OBJECT)

static void
tracker_extract_class_init (TrackerExtractClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_extract_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerExtractPrivate));
}

static void
tracker_extract_init (TrackerExtract *object)
{
#ifdef HAVE_STREAMANALYZER
	tracker_topanalyzer_init ();
#endif
}

static void
tracker_extract_finalize (GObject *object)
{
	TrackerExtractPrivate *priv;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

#ifdef HAVE_STREAMANALYZER
	tracker_topanalyzer_shutdown ();
#endif

	g_array_free (priv->extractors, TRUE);

	G_OBJECT_CLASS (tracker_extract_parent_class)->finalize (object);
}

static void
statements_free (GPtrArray *statements)
{
	guint i;

	for (i = 0; i < statements->len; i++) {
		g_value_array_free (statements->pdata[i]);
	}
	g_ptr_array_free (statements, TRUE);
}

TrackerExtract *
tracker_extract_new (void)
{
	TrackerExtract *object;
	TrackerExtractPrivate *priv;
	GDir *dir;
	GError *error;
	const gchar *name;
	GArray *extractors;
	GArray *generic_extractors;

	if (!g_module_supported ()) {
		g_error ("Modules are not supported for this platform");
		return NULL;
	}

	extractors = g_array_sized_new (FALSE,
					TRUE,
					sizeof (TrackerExtractData),
					10);

	/* This array is going to be used to store
	 * temporarily extractors with mimetypes such as "audio / *"
	 */
	generic_extractors = g_array_sized_new (FALSE,
						TRUE,
						sizeof (TrackerExtractData),
						10);

	error = NULL;
	dir = g_dir_open (MODULESDIR, 0, &error);

	if (!dir) {
		g_error ("Error opening modules directory: %s", error->message);
		g_error_free (error);
		g_array_free (generic_extractors, TRUE);
		g_array_free (extractors, TRUE);
		return NULL;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		TrackerExtractDataFunc func;
		TrackerExtractData *data;
		GModule *module;
		gchar *module_path;

		if (!g_str_has_suffix (name, "." G_MODULE_SUFFIX)) {
			continue;
		}

		module_path = g_build_filename (MODULESDIR, name, NULL);

		module = g_module_open (module_path, G_MODULE_BIND_LOCAL);

		if (!module) {
			g_warning ("Could not load module '%s': %s", name, g_module_error ());
			g_free (module_path);
			continue;
		}

		g_module_make_resident (module);

		if (g_module_symbol (module, "tracker_get_extract_data", (gpointer *) &func)) {
			data = (func) ();

			for (; data->mime; data++) {
				if (strchr (data->mime, '*') != NULL) {
					g_array_append_val (generic_extractors, *data);
				} else {
					g_array_append_val (extractors, *data);
				}
			}
		}

		g_free (module_path);
	}

	g_dir_close (dir);

	/* Append the generic extractors at the end of
	 * the list, so the specific ones are used first
	 */
	g_array_append_vals (extractors,
			     generic_extractors->data,
			     generic_extractors->len);
	g_array_free (generic_extractors, TRUE);

	/* Set extractors */
	object = g_object_new (TRACKER_TYPE_EXTRACT, NULL);

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	priv->extractors = extractors;

	return object;
}

static void
print_file_metadata_item (gpointer key,
			  gpointer value,
			  gpointer user_data)
{
	gchar *value_utf8;

	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);

	value_utf8 = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);

	if (value_utf8) {
		tracker_dbus_request_debug (GPOINTER_TO_UINT (user_data),
					    "  Found '%s'='%s'",
					    key,
					    value_utf8);
		g_free (value_utf8);
	}
}

static GPtrArray *
get_file_metadata (TrackerExtract *extract,
		   guint           request_id,
		   const gchar    *uri,
		   const gchar    *mime_)
{
	GPtrArray *statements;
	GFile *file;
	GFileInfo *info;
	GError *error = NULL;
	const gchar *attributes = NULL;
	gchar *mime_used = NULL;
	goffset size = 0;
	gchar *content_type = NULL;
	gchar *mime = mime_;

	/* Create hash table to send back */
	statements = g_ptr_array_new ();

#ifdef HAVE_STREAMANALYZER
	tracker_topanalyzer_extract (uri, statements, &content_type);
#endif

	if ((!mime || mime[0]=='\0') && content_type)
		mime = content_type;

	file = g_file_new_for_uri (uri);
	if (!file) {
		g_warning ("Could not create GFile for uri:'%s'",
			   uri);
		g_free (content_type);
		statements_free (statements);
		return NULL;
	}

	/* Blocks */
	if (!g_file_query_exists (file, NULL)) {
		g_warning ("File does not exist '%s'", uri);
		g_object_unref (file);
		g_free (content_type);
		statements_free (statements);
		return NULL;
	}

	/* Do we get size and mime? or just size? */
	if (mime && *mime) {
		attributes = 
			G_FILE_ATTRIBUTE_STANDARD_SIZE;
	} else {
		attributes = 
			G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
			G_FILE_ATTRIBUTE_STANDARD_SIZE;
	}

	info = g_file_query_info (file, 
				  attributes, 
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
		g_free (content_type);
		statements_free (statements);
		return NULL;
	}

	/* Check the size is actually non-zero */
	size = g_file_info_get_size (info);

	if (size < 1) {
		tracker_dbus_request_comment (request_id,
					      "  File size is 0 bytes, ignoring file");
		
		g_object_unref (info);
		g_object_unref (file);

		g_free (content_type);
		statements_free (statements);
		return statements;
	}

	/* We know the mime */
	if (mime && *mime) {
		mime_used = g_strdup (mime);
		g_strstrip (mime_used);
	} else {
		mime_used = g_strdup (g_file_info_get_content_type (info));

		tracker_dbus_request_comment (request_id,
					      "  Guessing mime type as '%s' for uri:'%s'",
					      mime_used,
					      uri);
	}

	g_object_unref (info);
	g_object_unref (file);

	/* Now we have sanity checked everything, actually get the
	 * data we need from the extractors.
	 */
	if (mime_used) {
		TrackerExtractPrivate *priv;
		TrackerExtractData *data;
		guint i;

		priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

		for (i = 0; i < priv->extractors->len; i++) {
			data = &g_array_index (priv->extractors, TrackerExtractData, i);

			if (g_pattern_match_simple (data->mime, mime_used)) {
				(*data->extract) (uri, statements);

				if (statements->len == 0) {
					continue;
				}

				tracker_dbus_request_comment (request_id,
							      "  Found %d metadata items",
							      statements->len);

				g_free (mime_used);
				g_free (content_type);

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

	g_free (content_type);

	return statements;
}

void
tracker_extract_get_metadata_by_cmdline (TrackerExtract *object,
					 const gchar    *uri,
					 const gchar    *mime)
{
	guint       request_id;
	gint        i;
	GPtrArray   *statements = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	g_return_if_fail (uri != NULL);

	/* NOTE: Don't reset the timeout to shutdown here */
	statements = get_file_metadata (object, request_id, uri, mime);

	if (statements) {
		statements_free (statements);
	}
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
				  "DBus request to get PID");
	
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
	guint       request_id;
	gint        i;
	GPtrArray  *statements = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to extract metadata, "
				  "uri:'%s', mime:%s",
				  uri,
				  mime);

	tracker_dbus_request_debug (request_id,
				    "  Resetting shutdown timeout");
	
	tracker_main_quit_timeout_reset ();
	alarm (MAX_EXTRACT_TIME);

	statements = get_file_metadata (object, request_id, uri, mime);

	if (statements) {
		dbus_g_method_return (context, statements);
		for (i = 0; i < statements->len; i++) {
			g_value_array_free (statements->pdata[i]);
		}
		g_ptr_array_free (statements, TRUE);
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

	/* Unset alarm so the extractor doesn't die when it's idle */
	alarm (0);
}
