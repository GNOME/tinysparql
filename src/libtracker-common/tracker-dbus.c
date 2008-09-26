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

#include "tracker-dbus.h"

#include <gio/gio.h>

struct TrackerDBusRequestHandler {
	TrackerDBusRequestFunc new;
	TrackerDBusRequestFunc done;
	gpointer	       user_data;
};

static GSList *hooks;

static void
request_handler_call_for_new (guint request_id)
{
	GSList *l;

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

	for (l = hooks; l; l = l->next) {
		TrackerDBusRequestHandler *handler;

		handler = l->data;

		if (handler->done) {
			(handler->done)(request_id, handler->user_data);
		}
	}
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
	GSList	*l;
	gchar  **strv;
	gint	 i = 0;

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
tracker_dbus_queue_str_to_strv (GQueue *queue,
				gint	max)
{
	gchar **strv;
	gchar  *str;
	gint	i, j;
	gint	length;

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
				  gint	  max)
{
	gchar **strv;
	gchar  *str;
	GFile  *file;
	gint	i, j;
	gint	length;

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
			       gpointer		      user_data)
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
tracker_dbus_request_new (gint		request_id,
			  const gchar  *format,
			  ...)
{
	gchar	*str;
	va_list  args;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_message ("<--- [%d] %s",
		   request_id,
		   str);

	g_free (str);

	request_handler_call_for_new (request_id);
}

void
tracker_dbus_request_success (gint request_id)
{
	request_handler_call_for_done (request_id);

	g_message ("---> [%d] Success, no error given",
		   request_id);
}

void
tracker_dbus_request_failed (gint	   request_id,
			     GError	 **error,
			     const gchar  *format,
			     ...)
{
	gchar	*str;
	va_list  args;

	request_handler_call_for_done (request_id);

	if (format) {
		va_start (args, format);
		str = g_strdup_vprintf (format, args);
		va_end (args);

		g_set_error (error, TRACKER_DBUS_ERROR, 0, str);
	} else if (*error != NULL) {
		str = g_strdup ((*error)->message);
	} else {
		str = g_strdup (_("No error given"));
		g_warning ("Unset error and no error message.");
	}

	g_message ("---> [%d] Failed, %s",
		   request_id,
		   str);
	g_free (str);
}

void
tracker_dbus_request_comment (gint	   request_id,
			      const gchar *format,
			      ...)
{
	gchar	*str;
	va_list  args;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_message ("---- [%d] %s",
		   request_id,
		   str);
	g_free (str);
}
