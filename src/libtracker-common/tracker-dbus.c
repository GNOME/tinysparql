/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <gio/gio.h>

#include <dbus/dbus-glib-bindings.h>

#include "tracker-dbus.h"
#include "tracker-log.h"

/* How long clients can exist since last D-Bus call before being
 * cleaned up.
 */
#define CLIENT_CLEAN_UP_TIME  300

/* How often we check for stale client cache (in seconds) */
#define CLIENT_CLEAN_UP_CHECK 60

struct TrackerDBusRequestHandler {
	TrackerDBusRequestFunc new;
	TrackerDBusRequestFunc done;
	gpointer               user_data;
};

typedef struct {
	gchar *sender;
	gchar *binary;
	gulong pid;
	GTimeVal last_time;
} ClientData;

static GSList *hooks;
static gboolean block_hooks;

static gboolean client_lookup_enabled;
static DBusGConnection *freedesktop_connection;
static DBusGProxy *freedesktop_proxy;
static GHashTable *clients;
static guint clients_clean_up_id;

static void     client_data_free    (gpointer data);
static gboolean clients_clean_up_cb (gpointer data);

static void
request_handler_call_for_new (guint request_id)
{
	GSList *l;

	if (block_hooks) {
		return;
	}

	for (l = hooks; l; l = l->next) {
		TrackerDBusRequestHandler *handler;

		handler = l->data;

		if (handler->new) {
			(handler->new)(request_id, handler->user_data);
		}
	}
}

static void
request_handler_call_for_done (guint request_id)
{
	GSList *l;

	if (block_hooks) {
		return;
	}

	for (l = hooks; l; l = l->next) {
		TrackerDBusRequestHandler *handler;

		handler = l->data;

		if (handler->done) {
			(handler->done)(request_id, handler->user_data);
		}
	}
}

static gboolean
clients_init (void)
{
	GError *error = NULL;
	DBusGConnection *conn;

	conn = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!conn) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_error_free (error);
		return FALSE;
	}

	freedesktop_connection = dbus_g_connection_ref (conn);

	freedesktop_proxy =
		dbus_g_proxy_new_for_name (freedesktop_connection,
		                           DBUS_SERVICE_DBUS,
		                           DBUS_PATH_DBUS,
		                           DBUS_INTERFACE_DBUS);

	if (!freedesktop_proxy) {
		g_critical ("Could not create a proxy for the Freedesktop service, %s",
		            error ? error->message : "no error given.");
		g_error_free (error);
		return FALSE;
	}

	clients = g_hash_table_new_full (g_str_hash,
	                                 g_str_equal,
	                                 NULL,
	                                 client_data_free);
	clients_clean_up_id =
		g_timeout_add_seconds (CLIENT_CLEAN_UP_CHECK, clients_clean_up_cb, NULL);

	return TRUE;
}

static gboolean
clients_shutdown (void)
{
	if (freedesktop_proxy) {
		g_object_unref (freedesktop_proxy);
		freedesktop_proxy = NULL;
	}

	if (freedesktop_connection) {
		dbus_g_connection_unref (freedesktop_connection);
		freedesktop_connection = NULL;
	}

	if (clients_clean_up_id != 0) {
		g_source_remove (clients_clean_up_id);
		clients_clean_up_id = 0;
	}

	if (clients) {
		g_hash_table_unref (clients);
		clients = NULL;
	}

	return TRUE;
}

static void
client_data_free (gpointer data)
{
	ClientData *cd = data;

	if (!cd) {
		return;
	}

	g_free (cd->sender);
	g_free (cd->binary);

	g_slice_free (ClientData, cd);
}

static ClientData *
client_data_new (gchar *sender)
{
	ClientData *cd;
	GError *error = NULL;
	guint pid;
	gboolean get_binary = TRUE;

	cd = g_slice_new0 (ClientData);
	cd->sender = sender;

	if (org_freedesktop_DBus_get_connection_unix_process_id (freedesktop_proxy,
	                                                         sender,
	                                                         &pid,
	                                                         &error)) {
		cd->pid = pid;
	}

	if (get_binary) {
		gchar *filename;
		gchar *pid_str;
		gchar *contents = NULL;
		GError *error = NULL;
		gchar **strv;

		pid_str = g_strdup_printf ("%ld", cd->pid);
		filename = g_build_filename (G_DIR_SEPARATOR_S,
		                             "proc",
		                             pid_str,
		                             "cmdline",
		                             NULL);
		g_free (pid_str);

		if (!g_file_get_contents (filename, &contents, NULL, &error)) {
			g_warning ("Could not get process name from id %ld, %s",
			           cd->pid,
			           error ? error->message : "no error given");
			g_clear_error (&error);
			g_free (filename);
			return cd;
		}

		g_free (filename);

		strv = g_strsplit (contents, "^@", 2);
		if (strv && strv[0]) {
			cd->binary = g_path_get_basename (strv[0]);
		}

		g_strfreev (strv);
		g_free (contents);
	}

	return cd;
}

static gboolean
clients_clean_up_cb (gpointer data)
{
	GTimeVal now;
	GHashTableIter iter;
	gpointer key, value;

	g_get_current_time (&now);

	g_hash_table_iter_init (&iter, clients);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ClientData *cd;
		glong diff;

		cd = value;

		diff = now.tv_sec - cd->last_time.tv_sec;

		/* 5 Minutes */
		if (diff >= CLIENT_CLEAN_UP_TIME) {
			g_debug ("Removing D-Bus client data for '%s' with id:'%s'",
			         cd->binary, cd->sender);
			g_hash_table_iter_remove (&iter);
		}
	}

	if (g_hash_table_size (clients) < 1) {
		/* This must be before the clients_shutdown which will
		 * attempt to clean up the the timeout too.
		 */
		clients_clean_up_id = 0;

		/* Clean everything else up. */
		clients_shutdown ();

		return FALSE;
	}

	return TRUE;
}

static ClientData *
client_get_for_context (DBusGMethodInvocation *context)
{
	ClientData *cd;
	gchar *sender;

	if (!client_lookup_enabled) {
		return NULL;
	}

	/* Only really done with tracker-extract where we use
	 * functions from the command line with dbus code in them.
	 */
	if (!context) {
		return NULL;
	}

	/* Shame we have to allocate memory in any condition here,
	 * sucky glib D-Bus API is to blame here :/
	 */
	sender = dbus_g_method_get_sender (context);

	if (G_UNLIKELY (!clients)) {
		clients_init ();

		cd = client_data_new (sender);
		g_hash_table_insert (clients, sender, cd);
	} else {
		cd = g_hash_table_lookup (clients, sender);

		if (G_UNLIKELY (!cd)) {
			cd = client_data_new (sender);
			g_hash_table_insert (clients, sender, cd);
		} else {
			g_get_current_time (&cd->last_time);
			g_free (sender);
		}
	}

	return cd;
}

GValue *
tracker_dbus_gvalue_slice_new (GType type)
{
	GValue *value;

	value = g_slice_new0 (GValue);
	g_value_init (value, type);

	return value;
}

void
tracker_dbus_gvalue_slice_free (GValue *value)
{
	g_value_unset (value);
	g_slice_free (GValue, value);
}

GQuark
tracker_dbus_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_DBUS_ERROR_DOMAIN);
}

TrackerDBusData *
tracker_dbus_data_new (const gpointer arg1,
                       const gpointer arg2)
{
	TrackerDBusData *data;

	data = g_new0 (TrackerDBusData, 1);

	data->id = tracker_dbus_get_next_request_id ();

	data->data1 = arg1;
	data->data2 = arg2;

	return data;
}

gchar **
tracker_dbus_slist_to_strv (GSList *list)
{
	GSList  *l;
	gchar  **strv;
	gint     i = 0;

	strv = g_new0 (gchar*, g_slist_length (list) + 1);

	for (l = list; l != NULL; l = l->next) {
		if (!g_utf8_validate (l->data, -1, NULL)) {
			g_message ("Could not add string:'%s' to GStrv, invalid UTF-8",
			           (gchar*) l->data);
			continue;
		}

		strv[i++] = g_strdup (l->data);
	}

	strv[i] = NULL;

	return strv;
}

gchar **
tracker_dbus_str_to_strv (const gchar *str)
{
	gchar **strv;

	strv = g_new (gchar*, 2);
	strv[0] = g_strdup (str);
	strv[1] = NULL;

	return strv;
}

gchar **
tracker_dbus_queue_str_to_strv (GQueue *queue,
                                gint    max)
{
	gchar **strv;
	gchar  *str;
	gint    i, j;
	gint    length;

	length = g_queue_get_length (queue);

	if (max > 0) {
		length = MIN (max, length);
	}

	strv = g_new0 (gchar*, length + 1);

	for (i = 0, j = 0; i < length; i++) {
		str = g_queue_pop_head (queue);

		if (!str) {
			break;
		}

		if (!g_utf8_validate (str, -1, NULL)) {
			g_message ("Could not add string:'%s' to GStrv, invalid UTF-8", str);
			g_free (str);
			continue;
		}

		strv[j++] = str;
	}

	strv[j] = NULL;

	return strv;
}

gchar **
tracker_dbus_queue_gfile_to_strv (GQueue *queue,
                                  gint    max)
{
	gchar **strv;
	gchar  *str;
	GFile  *file;
	gint    i, j;
	gint    length;

	length = g_queue_get_length (queue);

	if (max > 0) {
		length = MIN (max, length);
	}

	strv = g_new0 (gchar*, length + 1);

	for (i = 0, j = 0; i < length; i++) {
		file = g_queue_pop_head (queue);

		if (!file) {
			break;
		}

		str = g_file_get_path (file);
		g_object_unref (file);

		if (!g_utf8_validate (str, -1, NULL)) {
			g_message ("Could not add string:'%s' to GStrv, invalid UTF-8", str);
			g_free (str);
			continue;
		}

		strv[j++] = str;
	}

	strv[j] = NULL;

	return strv;
}

void
tracker_dbus_results_ptr_array_free (GPtrArray **ptr_array)
{
	if (!ptr_array || !(*ptr_array)) {
		return;
	}

	g_ptr_array_foreach (*ptr_array, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (*ptr_array, TRUE);
	*ptr_array = NULL;
}

guint
tracker_dbus_get_next_request_id (void)
{
	static guint request_id = 1;

	return request_id++;
}

TrackerDBusRequestHandler *
tracker_dbus_request_add_hook (TrackerDBusRequestFunc new,
                               TrackerDBusRequestFunc done,
                               gpointer                       user_data)
{
	TrackerDBusRequestHandler *handler;

	handler = g_slice_new0 (TrackerDBusRequestHandler);
	handler->new = new;
	handler->done = done;
	handler->user_data = user_data;

	hooks = g_slist_append (hooks, handler);

	return handler;
}

void
tracker_dbus_request_remove_hook (TrackerDBusRequestHandler *handler)
{
	g_return_if_fail (handler != NULL);

	hooks = g_slist_remove (hooks, handler);
	g_slice_free (TrackerDBusRequestHandler, handler);
}

void
tracker_dbus_request_new (gint                   request_id,
                          DBusGMethodInvocation *context,
                          const gchar           *format,
                          ...)
{
	ClientData *cd;
	gchar *str;
	va_list args;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	cd = client_get_for_context (context);

	g_debug ("<--- [%d%s%s] %s",
	         request_id,
	         cd ? "|" : "",
	         cd ? cd->binary : "",
	         str);

	g_free (str);

	request_handler_call_for_new (request_id);
}

void
tracker_dbus_request_success (gint                   request_id,
                              DBusGMethodInvocation *context)
{
	ClientData *cd;

	request_handler_call_for_done (request_id);

	cd = client_get_for_context (context);

	g_debug ("---> [%d%s%s] Success, no error given",
	         request_id,
	         cd ? "|" : "",
	         cd ? cd->binary : "");
}

void
tracker_dbus_request_failed (gint                    request_id,
                             DBusGMethodInvocation  *context,
                             GError                **error,
                             const gchar            *format,
                             ...)
{
	ClientData *cd;
	gchar *str;
	va_list args;

	request_handler_call_for_done (request_id);

	if (format) {
		va_start (args, format);
		str = g_strdup_vprintf (format, args);
		va_end (args);

		g_set_error (error, TRACKER_DBUS_ERROR, 0, "%s", str);
	} else if (*error != NULL) {
		str = g_strdup ((*error)->message);
	} else {
		str = g_strdup (_("No error given"));
		g_warning ("Unset error and no error message.");
	}

	cd = client_get_for_context (context);

	g_message ("---> [%d%s%s] Failed, %s",
	           request_id,
	           cd ? "|" : "",
	           cd ? cd->binary : "",
	           str);
	g_free (str);
}

void
tracker_dbus_request_info (gint                   request_id,
                           DBusGMethodInvocation *context,
                           const gchar           *format,
                           ...)
{
	ClientData *cd;
	gchar *str;
	va_list args;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	cd = client_get_for_context (context);

	tracker_info ("---- [%d%s%s] %s",
	              request_id,
	              cd ? "|" : "",
	              cd ? cd->binary : "",
	              str);
	g_free (str);
}

void
tracker_dbus_request_comment (gint                   request_id,
                              DBusGMethodInvocation *context,
                              const gchar           *format,
                              ...)
{
	ClientData *cd;
	gchar *str;
	va_list args;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	cd = client_get_for_context (context);

	g_message ("---- [%d%s%s] %s",
	           request_id,
	           cd ? "|" : "",
	           cd ? cd->binary : "",
	           str);
	g_free (str);
}

void
tracker_dbus_request_debug (gint                   request_id,
                            DBusGMethodInvocation *context,
                            const gchar           *format,
                            ...)
{
	ClientData *cd;
	gchar *str;
	va_list args;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	cd = client_get_for_context (context);

	g_debug ("---- [%d%s%s] %s",
	         request_id,
	         cd ? "|" : "",
	         cd ? cd->binary : "",
	         str);
	g_free (str);
}

void
tracker_dbus_request_block_hooks (void)
{
	block_hooks = TRUE;
}

void
tracker_dbus_request_unblock_hooks (void)
{
	block_hooks = FALSE;
}

void
tracker_dbus_enable_client_lookup (gboolean enabled)
{
	/* If this changed and we disabled everything, simply shut it
	 * all down.
	 */
	if (client_lookup_enabled != enabled && !enabled) {
		clients_shutdown ();
	}

	client_lookup_enabled = enabled;
}
