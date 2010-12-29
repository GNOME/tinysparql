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

#include <gio/gio.h>

#ifndef NO_LIBDBUS
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#endif

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#define TRACKER_DBUS_ERROR_DOMAIN "TrackerDBus"
#define TRACKER_DBUS_ERROR        tracker_dbus_error_quark()


#define TRACKER_TYPE_INT_ARRAY_MAP	\
	dbus_g_type_get_map ("GHashTable", G_TYPE_INT, DBUS_TYPE_G_INT_ARRAY)

#define TRACKER_TYPE_FOUR_INT_ARRAY	\
	dbus_g_type_get_collection ("GPtrArray", \
	                            dbus_g_type_get_struct("GValueArray", \
	                                                    G_TYPE_INT, \
	                                                    G_TYPE_INT, \
	                                                    G_TYPE_INT, \
	                                                    G_TYPE_INT, \
	                                                    G_TYPE_INVALID))

#define TRACKER_TYPE_EVENT_ARRAY	\
	dbus_g_type_get_collection ("GPtrArray", \
	                            dbus_g_type_get_struct ("GValueArray", \
	                                                    G_TYPE_STRING, \
	                                                    G_TYPE_STRING, \
	                                                    G_TYPE_INT, \
	                                                    G_TYPE_INVALID))
#define TRACKER_TYPE_G_STRV_ARRAY	\
	dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV)

#define tracker_dbus_async_return_if_fail(expr,context)	\
	G_STMT_START { \
		if G_LIKELY(expr) { } else { \
			GError *assert_error = NULL; \
	  \
			g_set_error (&assert_error, \
			             TRACKER_DBUS_ERROR, \
			             TRACKER_DBUS_ERROR_ASSERTION_FAILED, \
			             "Assertion `%s' failed", \
			             #expr); \
	  \
			dbus_g_method_return_error (context, assert_error); \
			g_clear_error (&assert_error); \
	  \
			return; \
		}; \
	} G_STMT_END


#define tracker_gdbus_async_return_if_fail(expr,invocation)	\
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
			g_dbus_method_invocation_return_gerror (invocation, assert_error); \
			g_clear_error (&assert_error); \
	  \
			return; \
		}; \
	} G_STMT_END

#define tracker_dbus_return_val_if_fail(expr,val,error)	\
	G_STMT_START { \
		if G_LIKELY(expr) { } else { \
			g_set_error (error, \
			             TRACKER_DBUS_ERROR, \
			             TRACKER_DBUS_ERROR_ASSERTION_FAILED, \
			             "Assertion `%s' failed", \
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

typedef void (*TrackerDBusSendAndSpliceCallback) (void     *buffer,
                                                  gssize    buffer_size,
                                                  GStrv     variable_names,
                                                  GError   *error,
                                                  gpointer  user_data);

typedef struct _TrackerDBusRequest TrackerDBusRequest;

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

GQuark              tracker_dbus_error_quark           (void);

/* Utils */
gchar **            tracker_dbus_slist_to_strv         (GSList                     *list);

/* Requests */
TrackerDBusRequest *tracker_dbus_request_begin         (const gchar                *sender,
                                                        const gchar                *format,
                                                        ...);
void                tracker_dbus_request_end           (TrackerDBusRequest         *request,
                                                        GError                     *error);
void                tracker_dbus_request_comment       (TrackerDBusRequest         *request,
                                                        const gchar                *format,
                                                        ...);
void                tracker_dbus_request_info          (TrackerDBusRequest         *request,
                                                        const gchar                *format,
                                                        ...);
void                tracker_dbus_request_debug         (TrackerDBusRequest         *request,
                                                        const gchar                *format,
                                                        ...);

void                tracker_dbus_enable_client_lookup  (gboolean                    enable);

#ifndef NO_LIBDBUS
/* GDBus convenience API */
TrackerDBusRequest *tracker_g_dbus_request_begin       (GDBusMethodInvocation      *invocation,
                                                        const gchar                *format,
                                                        ...);

/* dbus-glib convenience API */
TrackerDBusRequest *tracker_dbus_g_request_begin       (DBusGMethodInvocation      *context,
                                                        const gchar                *format,
                                                        ...);

/* File descriptor convenience API */
gboolean            tracker_dbus_send_and_splice       (DBusConnection             *connection,
                                                        DBusMessage                *message,
                                                        int                         fd,
                                                        GCancellable               *cancellable,
                                                        void                      **dest_buffer,
                                                        gssize                     *dest_buffer_size,
                                                        GStrv                      *variable_names,
                                                        GError                    **error);

gboolean            tracker_dbus_send_and_splice_async (DBusConnection             *connection,
                                                        DBusMessage                *message,
                                                        int                         fd,
                                                        gboolean                    expect_variable_names,
                                                        GCancellable               *cancellable,
                                                        TrackerDBusSendAndSpliceCallback callback,
                                                        gpointer                    user_data);
#endif

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_DBUS_H__ */
