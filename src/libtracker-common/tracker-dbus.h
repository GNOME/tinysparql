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

#ifndef __LIBTRACKER_COMMON_DBUS_H__
#define __LIBTRACKER_COMMON_DBUS_H__

#include <glib/gi18n.h>

#include <gio/gio.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#define TRACKER_TYPE_STR_STRV_MAP (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_STRV))

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#define TRACKER_DBUS_ERROR_DOMAIN "TrackerDBus"
#define TRACKER_DBUS_ERROR        tracker_dbus_error_quark()

#define TRACKER_TYPE_EVENT_ARRAY	  \
	dbus_g_type_get_collection ("GPtrArray", \
	                            dbus_g_type_get_struct ("GValueArray", \
	                                                    G_TYPE_STRING, \
	                                                    G_TYPE_STRING, \
	                                                    G_TYPE_INT, \
	                                                    G_TYPE_INVALID))
#define TRACKER_TYPE_G_STRV_ARRAY	  \
	dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV)

#define tracker_dbus_async_return_if_fail(expr,context)	  \
	G_STMT_START { \
		if G_LIKELY(expr) { } else { \
			GError *assert_error = NULL; \
	  \
			g_set_error (&assert_error, \
			             TRACKER_DBUS_ERROR, \
			             TRACKER_DBUS_ERROR_ASSERTION_FAILED, \
			             _("Assertion `%s' failed"), \
			             #expr); \
	  \
			dbus_g_method_return_error (context, assert_error); \
			g_clear_error (&assert_error); \
	  \
			return; \
		}; \
	} G_STMT_END

#define tracker_dbus_return_val_if_fail(expr,val,error)	  \
	G_STMT_START { \
		if G_LIKELY(expr) { } else { \
			g_set_error (error, \
			             TRACKER_DBUS_ERROR, \
			             TRACKER_DBUS_ERROR_ASSERTION_FAILED, \
			             _("Assertion `%s' failed"), \
			             #expr); \
	  \
			return val; \
		}; \
	} G_STMT_END

/* Size of buffers used when sending data over a pipe, using DBus FD passing */
#define TRACKER_DBUS_PIPE_BUFFER_SIZE 65536

#define TRACKER_DBUS_SERVICE_EXTRACT   "org.freedesktop.Tracker1.Extract"
#define TRACKER_DBUS_PATH_EXTRACT      "/org/freedesktop/Tracker1/Extract"
#define TRACKER_DBUS_INTERFACE_EXTRACT "org.freedesktop.Tracker1.Extract"

typedef struct TrackerDBusRequestHandler TrackerDBusRequestHandler;

typedef void (*TrackerDBusRequestFunc) (guint    request_id,
                                        gpointer user_data);

typedef void (*TrackerDBusSendAndSpliceCallback) (void     *buffer,
                                                  gssize    buffer_size,
                                                  GError   *error,
                                                  gpointer  user_data);

typedef struct {
	guint    id;
	gpointer data1;
	gpointer data2;
} TrackerDBusData;

typedef enum {
	TRACKER_DBUS_EVENTS_TYPE_ADD,
	TRACKER_DBUS_EVENTS_TYPE_UPDATE,
	TRACKER_DBUS_EVENTS_TYPE_DELETE
} TrackerDBusEventsType;

typedef enum {
	TRACKER_DBUS_ERROR_ASSERTION_FAILED,
	TRACKER_DBUS_ERROR_UNSUPPORTED,
	TRACKER_DBUS_ERROR_BROKEN_PIPE
} TrackerDBusError;

GQuark           tracker_dbus_error_quark            (void);
TrackerDBusData *tracker_dbus_data_new               (const gpointer              arg1,
                                                      const gpointer              arg2);

/* Utils */
GValue *         tracker_dbus_gvalue_slice_new       (GType                       type);
void             tracker_dbus_gvalue_slice_free      (GValue                     *value);
gchar **         tracker_dbus_slist_to_strv          (GSList                     *list);
gchar **         tracker_dbus_str_to_strv            (const gchar                *str);
gchar **         tracker_dbus_queue_str_to_strv      (GQueue                     *queue,
                                                      gint                        max);
gchar **         tracker_dbus_queue_gfile_to_strv    (GQueue                     *queue,
                                                      gint                        max);
void             tracker_dbus_results_ptr_array_free (GPtrArray                 **ptr_array);

/* Requests */
guint            tracker_dbus_get_next_request_id    (void);

TrackerDBusRequestHandler *
                 tracker_dbus_request_add_hook       (TrackerDBusRequestFunc      new,
                                                      TrackerDBusRequestFunc      done,
                                                      gpointer                    user_data);
void             tracker_dbus_request_remove_hook    (TrackerDBusRequestHandler  *handler);

void             tracker_dbus_request_new            (gint                        request_id,
                                                      DBusGMethodInvocation      *context,
                                                      const gchar                *format,
                                                      ...);
void             tracker_dbus_request_success        (gint                        request_id,
                                                      DBusGMethodInvocation      *context);
void             tracker_dbus_request_failed         (gint                        request_id,
                                                      DBusGMethodInvocation      *context,
                                                      GError                    **error,
                                                      const gchar                *format,
                                                      ...);
void             tracker_dbus_request_comment        (gint                        request_id,
                                                      DBusGMethodInvocation      *context,
                                                      const gchar                *format,
                                                      ...);
void             tracker_dbus_request_info           (gint                        request_id,
                                                      DBusGMethodInvocation      *context,
                                                      const gchar                *format,
                                                      ...);
void             tracker_dbus_request_debug          (gint                        request_id,
                                                      DBusGMethodInvocation      *context,
                                                      const gchar                *format,
                                                      ...);
void             tracker_dbus_request_block_hooks    (void);
void             tracker_dbus_request_unblock_hooks  (void);

void             tracker_dbus_enable_client_lookup   (gboolean                    enable);

/* File descriptor convenience API */
gboolean         tracker_dbus_send_and_splice        (DBusConnection             *connection,
                                                      DBusMessage                *message,
                                                      int                         fd,
                                                      GCancellable               *cancellable,
                                                      void                      **dest_buffer,
                                                      gssize                     *dest_buffer_size,
                                                      GError                    **error);

gboolean         tracker_dbus_send_and_splice_async  (DBusConnection             *connection,
                                                      DBusMessage                *message,
                                                      int                         fd,
                                                      GCancellable               *cancellable,
                                                      TrackerDBusSendAndSpliceCallback callback,
                                                      gpointer                    user_data);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_DBUS_H__ */
