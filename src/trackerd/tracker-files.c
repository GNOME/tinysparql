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
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-type-utils.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-dbus.h"
#include "tracker-files.h"
#include "tracker-db.h"
#include "tracker-marshal.h"
#include "tracker-indexer-client.h"

#define TRACKER_FILES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_FILES, TrackerFilesPrivate))

typedef struct {
	TrackerProcessor *processor;
} TrackerFilesPrivate;

static void tracker_files_finalize (GObject *object);

G_DEFINE_TYPE(TrackerFiles, tracker_files, G_TYPE_OBJECT)

static void
tracker_files_class_init (TrackerFilesClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_files_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerFilesPrivate));
}

static void
tracker_files_init (TrackerFiles *object)
{
}

static void
tracker_files_finalize (GObject *object)
{
	TrackerFilesPrivate *priv;

	priv = TRACKER_FILES_GET_PRIVATE (object);

	g_object_unref (priv->processor);

	G_OBJECT_CLASS (tracker_files_parent_class)->finalize (object);
}

TrackerFiles *
tracker_files_new (TrackerProcessor *processor)
{
	TrackerFiles	    *object;
	TrackerFilesPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), NULL);

	object = g_object_new (TRACKER_TYPE_FILES, NULL);

	priv = TRACKER_FILES_GET_PRIVATE (object);

	priv->processor = g_object_ref (processor);

	return object;
}

/*
 * Functions
 */
void
tracker_files_exist (TrackerFiles	    *object,
		     const gchar	    *uri,
		     gboolean		     auto_create,
		     DBusGMethodInvocation  *context,
		     GError		   **error)
{
	TrackerDBInterface *iface;
	guint		    request_id;
	guint32		    file_id;
	gboolean	    exists;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to see if files exist, "
				  "uri:'%s' auto-create:'%d'",
				  uri, auto_create);

	/* This API is broken. The Daemon doesn't do anything in the
	 * database except read from it.
	 */
	if (auto_create) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Request to check existence of file '%s' failed, "
					     "the 'auto-create' variable can not be TRUE, "
					     "this feature is no longer supported.",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);

		return;
	}

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	file_id = tracker_db_file_get_id (iface, uri);
	exists = file_id > 0;

	dbus_g_method_return (context, exists);

	tracker_dbus_request_success (request_id);
}

void
tracker_files_create (TrackerFiles	     *object,
		      const gchar	     *uri,
		      gboolean		      is_directory,
		      const gchar	     *mime,
		      gint		      size,
		      gint		      mtime,
		      DBusGMethodInvocation  *context,
		      GError		    **error)
{
	TrackerFilesPrivate *priv;
	GFile		    *file;
	const gchar	    *module_name = "files";
	guint		     request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (mime != NULL, context);
	tracker_dbus_async_return_if_fail (size >= 0, context);
	tracker_dbus_async_return_if_fail (mtime >= 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to create file, "
				  "uri:'%s', is directory:%s, mime:'%s', "
				  "size:%d, mtime:%d",
				  uri,
				  is_directory ? "yes" : "no",
				  mime,
				  size,
				  mtime);

	priv = TRACKER_FILES_GET_PRIVATE (object);

	file = g_file_new_for_path (uri);
	tracker_processor_files_check (priv->processor, module_name, file, is_directory);
	g_object_unref (file);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_files_delete (TrackerFiles	     *object,
		      const gchar	     *uri,
		      DBusGMethodInvocation  *context,
		      GError		    **error)
{
	TrackerFilesPrivate *priv;
	GFile		    *file;
	const gchar	    *module_name = "files";
	guint		     request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to delete file, "
				  "uri:'%s'",
				  uri);

	priv = TRACKER_FILES_GET_PRIVATE (object);

	/* Check the file exists, if not delete, this is broken, we
	 * really don't know with the API if it is a file or
	 * directory we are dealing with so we check both.
	 */
	file = g_file_new_for_path (uri);
	tracker_processor_files_check (priv->processor, module_name, file, TRUE);
	tracker_processor_files_check (priv->processor, module_name, file, FALSE);
	g_object_unref (file);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_files_get_service_type (TrackerFiles	       *object,
				const gchar	       *uri,
				DBusGMethodInvocation  *context,
				GError		      **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint		    request_id;
	guint32		    file_id;
	gchar		   *file_id_str;
	gchar		   *value = NULL;
	const gchar	   *mime = NULL;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get service type ",
				  "uri:'%s'",
				  uri);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	/* FIXME why dont obtain the service type directly from the DB??? */

	file_id = tracker_db_file_get_id (iface, uri);

	if (file_id < 1) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "File '%s' was not found in the database",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Get mime */
	file_id_str = tracker_guint_to_string (file_id);

	mime = NULL;
	result_set = tracker_db_metadata_get (iface,
					      file_id_str,
					      "File:Mime");

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &mime, -1);
		g_object_unref (result_set);
	}

	g_free (file_id_str);

	if (!mime) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Metadata 'File:Mime' for '%s' doesn't exist",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Get service from mime */
	value = tracker_ontology_get_service_by_mime (mime);

	if (value) {

		tracker_dbus_request_comment (request_id,
					      "Info for file '%s', "
					      "id:%d, mime:'%s', service:'%s'",
					      uri,
					      file_id,
					      mime,
					      value);
		dbus_g_method_return (context, value);
		g_free (value);

		tracker_dbus_request_success (request_id);
	} else {

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Unable to find service to mime '%s'",
					     mime);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
	}

}

void
tracker_files_get_text_contents (TrackerFiles		*object,
				 const gchar		*uri,
				 gint			 offset,
				 gint			 max_length,
				 DBusGMethodInvocation	*context,
				 GError		       **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint		    request_id;
	gchar		   *service_id;
	gchar		   *offset_str;
	gchar		   *max_length_str;
	gchar		   *value;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (offset >= 0, context);
	tracker_dbus_async_return_if_fail (max_length >= 0, context);
	tracker_dbus_async_return_if_fail (value != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get text contents, "
				  "uri:'%s', offset:%d, max length:%d",
				  uri,
				  offset,
				  max_length);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	/* FIXME iface is already for "Files". Makes no sense to try Files and Emails */
	service_id = tracker_db_file_get_id_as_string (iface, "Files", uri);
	if (!service_id) {

		service_id = tracker_db_file_get_id_as_string (iface, "Emails", uri);

		if (!service_id) {
			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Unable to retrieve service ID for uri '%s'",
						     uri);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}
	}

	offset_str = tracker_gint_to_string (offset);
	max_length_str = tracker_gint_to_string (max_length);

	result_set = tracker_db_exec_proc (iface,
					   "GetFileContents",
					   offset_str,
					   max_length_str,
					   service_id,
					   NULL);

	g_free (max_length_str);
	g_free (offset_str);
	g_free (service_id);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &value, -1);
		g_object_unref (result_set);

		if (value == NULL) {
			value = g_strdup ("");
		}

		dbus_g_method_return (context, value);
		g_free (value);

		tracker_dbus_request_success (request_id);

	} else {

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "The contents of the uri '%s' are not stored",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
	}
}

void
tracker_files_search_text_contents (TrackerFiles	   *object,
				    const gchar		   *uri,
				    const gchar		   *text,
				    gint		    max_length,
				    DBusGMethodInvocation  *context,
				    GError		  **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;
	gchar		   *name = NULL;
	gchar		   *path = NULL;
	gchar		   *max_length_str;
	gchar		   *value = NULL;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (text != NULL, context);
	tracker_dbus_async_return_if_fail (max_length >= 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to search text contents, "
				  "in uri:'%s' for text:'%s' with max length:%d",
				  uri,
				  text,
				  max_length);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	tracker_file_get_path_and_name (uri, &path, &name);

	max_length_str = tracker_gint_to_string (max_length);

	/* result_set = tracker_exec_proc (iface, */
	/*				"SearchFileContents", */
	/*				4, */
	/*				path, */
	/*				name, */
	/*				text, */
	/*				max_length_str); */


	g_free (max_length_str);
	g_free (path);
	g_free (name);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, value, -1);
		g_object_unref (result_set);
	} else {
		value = g_strdup ("");
	}

	/* Fixme: when this is implemented, we should return TRUE and
	 * change this function to the success variant.
	 */
	tracker_dbus_request_failed (request_id,
				     &actual_error,
				     "%s not implemented yet",
				     __PRETTY_FUNCTION__);
	dbus_g_method_return_error (context, actual_error);
	g_error_free (actual_error);
}

void
tracker_files_get_by_service_type (TrackerFiles		  *object,
				   gint			   live_query_id,
				   const gchar		  *service,
				   gint			   offset,
				   gint			   max_hits,
				   DBusGMethodInvocation  *context,
				   GError		 **error)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	gchar		   **values = NULL;
	GError		    *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service != NULL, context);
	tracker_dbus_async_return_if_fail (offset >= 0, context);
	tracker_dbus_async_return_if_fail (max_hits >= 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get files by service type, "
				  "query id:%d, service:'%s', offset:%d, max hits:%d, ",
				  live_query_id,
				  service,
				  offset,
				  max_hits);

	if (!tracker_ontology_service_is_valid (service)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service '%s' is invalid or has not been implemented yet",
					     service);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	result_set = tracker_db_files_get_by_service (iface,
						      service,
						      offset,
						      max_hits);

	values = tracker_dbus_query_result_to_strv (result_set, 0, NULL);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, values);
	if (values) {
		g_strfreev (values);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_files_get_by_mime_type (TrackerFiles	       *object,
				gint			live_query_id,
				gchar		      **mime_types,
				gint			offset,
				gint			max_hits,
				DBusGMethodInvocation  *context,
				GError		      **error)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	gchar		   **values = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (mime_types != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (mime_types) > 0, context);
	tracker_dbus_async_return_if_fail (offset >= 0, context);
	tracker_dbus_async_return_if_fail (max_hits >= 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get files by mime types, "
				  "query id:%d, mime types:%d, offset:%d, max hits:%d, ",
				  live_query_id,
				  g_strv_length (mime_types),
				  offset,
				  max_hits);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	result_set = tracker_db_files_get_by_mime (iface,
						   mime_types,
						   g_strv_length (mime_types),
						   offset,
						   max_hits,
						   FALSE);

	values = tracker_dbus_query_result_to_strv (result_set, 0, NULL);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, values);
	if (values) {
		g_strfreev (values);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_files_get_by_mime_type_vfs (TrackerFiles	   *object,
				    gint		    live_query_id,
				    gchar		  **mime_types,
				    gint		    offset,
				    gint		    max_hits,
				    DBusGMethodInvocation  *context,
				    GError		  **error)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	gchar		   **values = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (mime_types != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (mime_types) > 0, context);
	tracker_dbus_async_return_if_fail (offset >= 0, context);
	tracker_dbus_async_return_if_fail (max_hits >= 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get files by mime types (VFS), "
				  "query id:%d, mime types:%d, offset:%d, max hits:%d, ",
				  live_query_id,
				  g_strv_length (mime_types),
				  offset,
				  max_hits);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	/* NOTE: The only difference between this function and the
	 * non-VFS version is the boolean in this function call:
	 */
	result_set = tracker_db_files_get_by_mime (iface,
						   mime_types,
						   g_strv_length (mime_types),
						   offset,
						   max_hits,
						   TRUE);

	values = tracker_dbus_query_result_to_strv (result_set, 0, NULL);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, values);
	if (values) {
		g_strfreev (values);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_files_get_mtime (TrackerFiles		*object,
			 const gchar		*uri,
			 DBusGMethodInvocation	*context,
			 GError		       **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint		    request_id;
	gchar		   *path = NULL;
	gchar		   *name = NULL;
	gint		    mtime;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request for mtime, "
				  "uri:'%s'",
				  uri);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	tracker_file_get_path_and_name (uri, &path, &name);

	result_set = tracker_db_exec_proc (iface,
					   "GetFileMTime",
					   path,
					   name,
					   NULL);
	g_free (path);
	g_free (name);

	if (!result_set) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "There is no file mtime in the database for '%s'",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	tracker_db_result_set_get (result_set, 0, &mtime, -1);
	g_object_unref (result_set);

	dbus_g_method_return (context, mtime);

	tracker_dbus_request_success (request_id);
}

void
tracker_files_get_metadata_for_files_in_folder (TrackerFiles	       *object,
						gint			live_query_id,
						const gchar	       *uri,
						gchar		      **fields,
						DBusGMethodInvocation  *context,
						GError		      **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	TrackerField	   *defs[255];
	guint		    request_id;
	guint		    i;
	gchar		   *uri_filtered;
	guint32		    file_id;
	GString		   *sql;
	gboolean	    needs_join[255];
	gchar		   *query;
	GPtrArray	   *values;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (fields != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (fields) > 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request for metadata for files in folder, "
				  "query id:%d, uri:'%s', fields:%d",
				  live_query_id,
				  uri,
				  g_strv_length (fields));


	/* Get fields for metadata list provided */
	for (i = 0; i < g_strv_length (fields); i++) {
		defs[i] = tracker_ontology_get_field_by_name (fields[i]);

		if (!defs[i]) {
			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Metadata field '%s' was not found",
						     fields[i]);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}

	}
	defs [g_strv_length (fields)] = NULL;


	if (g_str_has_suffix (uri, G_DIR_SEPARATOR_S)) {
		/* Remove trailing 'G_DIR_SEPARATOR' */
		uri_filtered = g_strndup (uri, strlen (uri) - 1);
	} else {
		uri_filtered = g_strdup (uri);
	}

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	/* Get file ID in database */
	file_id = tracker_db_file_get_id (iface, uri_filtered);
	if (file_id == 0) {
		g_free (uri_filtered);
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "File or directory was not in database, uri:'%s'",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Build SELECT clause */
	sql = g_string_new (" ");
	g_string_append_printf (sql,
				"SELECT (F.Path || '%s' || F.Name) as PathName ",
				G_DIR_SEPARATOR_S);

	for (i = 1; i <= g_strv_length (fields); i++) {
		gchar *field;

		field = tracker_db_get_field_name ("Files", fields[i-1]);

		if (field) {
			g_string_append_printf (sql, ", F.%s ", field);
			g_free (field);
			needs_join[i - 1] = FALSE;
		} else {
			gchar *display_field;

			display_field = tracker_ontology_field_get_display_name (defs[i-1]);
			g_string_append_printf (sql, ", M%d.%s ", i, display_field);
			g_free (display_field);
			needs_join[i - 1] = TRUE;
		}
	}

	/* Build FROM clause */
	g_string_append (sql,
			 " FROM Services F ");

	for (i = 0; i < g_strv_length (fields); i++) {
		const gchar *table;

		if (!needs_join[i]) {
			continue;
		}

		table = tracker_db_metadata_get_table (tracker_field_get_data_type (defs[i]));

		g_string_append_printf (sql,
					" LEFT OUTER JOIN %s M%d ON "
					"F.ID = M%d.ServiceID AND "
					"M%d.MetaDataID = %s ",
					table,
					i+1,
					i+1,
					i+1,
					tracker_field_get_id (defs[i]));
	}

	/* Build WHERE clause */
	g_string_append_printf (sql,
				" WHERE F.Path = '%s' ",
				uri_filtered);
	g_free (uri_filtered);

	query = g_string_free (sql, FALSE);
	result_set = tracker_db_interface_execute_query (iface, NULL, query);
	values = tracker_dbus_query_result_to_ptr_array (result_set);

	if (result_set) {
		g_object_unref (result_set);
	}

	g_free (query);

	dbus_g_method_return (context, values);

	tracker_dbus_results_ptr_array_free (&values);

	tracker_dbus_request_success (request_id);
}

void
tracker_files_search_by_text_and_mime (TrackerFiles	      *object,
				       const gchar	      *text,
				       gchar		     **mime_types,
				       DBusGMethodInvocation  *context,
				       GError		     **error)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	gchar		   **values = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (text != NULL, context);
	tracker_dbus_async_return_if_fail (mime_types != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (mime_types) > 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to search files by text & mime types, "
				  "text:'%s', mime types:%d",
				  text,
				  g_strv_length (mime_types));

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	result_set = tracker_db_search_text_and_mime (iface, text, mime_types);

	if (result_set) {
		gboolean  valid = TRUE;
		gchar	 *prefix, *name;
		gint	  row_count = 0;
		gint	  i = 0;

		row_count = tracker_db_result_set_get_n_rows (result_set);
		values = g_new0 (gchar *, row_count);

		while (valid) {
			tracker_db_result_set_get (result_set,
						   0, &prefix,
						   1, &name,
						   -1);

			values[i++] = g_build_filename (prefix, name, NULL);
			valid = tracker_db_result_set_iter_next (result_set);

			g_free (prefix);
			g_free (name);
		}

		g_object_unref (result_set);
	} else {
		values = g_new0 (gchar *, 1);
		values[0] = NULL;
	}

	dbus_g_method_return (context, values);

	g_strfreev (values);

	tracker_dbus_request_success (request_id);
}

void
tracker_files_search_by_text_and_location (TrackerFiles		  *object,
					   const gchar		  *text,
					   const gchar		  *uri,
					   DBusGMethodInvocation  *context,
					   GError		 **error)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	gchar		   **values = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (text != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to search files by text & location, "
				  "text:'%s', uri:'%s'",
				  text,
				  uri);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	result_set = tracker_db_search_text_and_location (iface, text, uri);

	if (result_set) {
		gboolean  valid = TRUE;
		gchar	 *prefix, *name;
		gint	  row_count;
		gint	  i = 0;

		row_count = tracker_db_result_set_get_n_rows (result_set);
		values = g_new0 (gchar *, row_count);

		while (valid) {
			tracker_db_result_set_get (result_set,
						   0, &prefix,
						   1, &name,
						   -1);

			values[i++] = g_build_filename (prefix, name, NULL);
			valid = tracker_db_result_set_iter_next (result_set);

			g_free (prefix);
			g_free (name);
		}

		g_object_unref (result_set);
	} else {
		values = g_new0 (gchar *, 1);
		values[0] = NULL;
	}

	dbus_g_method_return (context, values);

	g_strfreev (values);

	tracker_dbus_request_success (request_id);
}

void
tracker_files_search_by_text_and_mime_and_location (TrackerFiles	   *object,
						    const gchar		   *text,
						    gchar		  **mime_types,
						    const gchar		   *uri,
						    DBusGMethodInvocation  *context,
						    GError		  **error)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	gchar		   **values = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (text != NULL, context);
	tracker_dbus_async_return_if_fail (mime_types != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (mime_types) > 0, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to search files by text & mime types & location, "
				  "text:'%s', mime types:%d, uri:'%s'",
				  text,
				  g_strv_length (mime_types),
				  uri);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	result_set = tracker_db_search_text_and_mime_and_location (iface, text, mime_types, uri);

	if (result_set) {
		gboolean  valid = TRUE;
		gchar	 *prefix, *name;
		gint	  row_count;
		gint	  i = 0;

		row_count = tracker_db_result_set_get_n_rows (result_set);
		values = g_new0 (gchar *, row_count);

		while (valid) {
			tracker_db_result_set_get (result_set,
						   0, &prefix,
						   1, &name,
						   -1);

			values[i++] = g_build_filename (prefix, name, NULL);
			valid = tracker_db_result_set_iter_next (result_set);

			g_free (prefix);
			g_free (name);
		}

		g_object_unref (result_set);
	} else {
		values = g_new0 (gchar *, 1);
		values[0] = NULL;
	}

	dbus_g_method_return (context, values);

	g_strfreev (values);

	tracker_dbus_request_success (request_id);
}
