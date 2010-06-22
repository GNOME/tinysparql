/* 
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2010, Codeminded BVBA
 *                     FD passing by Adrien Bustany <abustany@gnome.org>
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

#ifndef __TRACKER_CLIENT_H__
#define __TRACKER_CLIENT_H__

#if !defined (__LIBTRACKER_CLIENT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-client/tracker-client.h> must be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_DBUS_SERVICE              "org.freedesktop.Tracker1"
#define TRACKER_DBUS_OBJECT               "/org/freedesktop/Tracker1"
#define TRACKER_DBUS_INTERFACE_RESOURCES  "org.freedesktop.Tracker1.Resources"
#define TRACKER_DBUS_INTERFACE_STATISTICS "org.freedesktop.Tracker1.Statistics"

#define TRACKER_TYPE_CLIENT         (tracker_client_get_type())
#define TRACKER_CLIENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CLIENT, TrackerClient))
#define TRACKER_CLIENT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_CLIENT, TrackerClientClass))
#define TRACKER_IS_CLIENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CLIENT))
#define TRACKER_IS_CLIENT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_CLIENT))
#define TRACKER_CLIENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_CLIENT, TrackerClientClass))

typedef struct {
	GObject parent;
} TrackerClient;

typedef struct {
	GObjectClass parent;
} TrackerClientClass;

typedef struct TrackerResultIterator TrackerResultIterator;

/**
 * TrackerClientFlags:
 * @TRACKER_CLIENT_ENABLE_WARNINGS: If supplied warnings will be
 * produced upon erronous situations. This is usually turned off for
 * applications that want to provide their own error reporting.
 */
typedef enum {
	TRACKER_CLIENT_ENABLE_WARNINGS      = 1 << 0
} TrackerClientFlags;

#define TRACKER_CLIENT_ERROR        tracker_client_error_quark ()
#define TRACKER_CLIENT_ERROR_DOMAIN "TrackerClient"

typedef enum {
	TRACKER_CLIENT_ERROR_UNSUPPORTED,
	TRACKER_CLIENT_ERROR_BROKEN_PIPE
} TrackerClientError;

/**
 * TrackerReplyGPtrArray:
 * @result: a #GPtrArray with the results of the query.
 * @error: a #GError.
 * @user_data: a #gpointer for user data.
 *
 * The @result is returned as a #GPtrArray containing an array of
 * #GStrv with the results from the query unless there is an error. If
 * there is an error the @error is populated with the details. The
 * @user_data is provided in the callback.
 **/
typedef void (*TrackerReplyGPtrArray) (GPtrArray *result,
                                       GError    *error,
                                       gpointer   user_data);

/**
 * TrackerReplyVoid:
 * @error: a GError.
 * @user_data: a gpointer for user data.
 *
 * The @user_data is returned when the query has completed. If there
 * is an error the @error is populated with the details.
 **/
typedef void (*TrackerReplyVoid)      (GError    *error,
                                       gpointer   user_data);

typedef void (*TrackerReplyIterator)  (TrackerResultIterator *iterator,
                                       GError                *error,
                                       gpointer               user_data);

/**
 * TrackerWritebackCallback:
 * @resources: a hash table where each key is the uri of a resources which
 *             was modified. To each key is associated an array of strings,
 *             which are the various RDF classes the uri belongs to.
 *
 * The callback is called everytime a property annotated with tracker:writeback
 * is modified in the store.
 */
typedef void (*TrackerWritebackCallback) (const GHashTable *resources,
                                          gpointer          user_data);

GType          tracker_client_get_type                     (void) G_GNUC_CONST;
GQuark         tracker_client_error_quark                  (void);
TrackerClient *tracker_client_new                          (TrackerClientFlags      flags,
                                                            gint                    timeout);

gboolean       tracker_cancel_call                         (TrackerClient          *client,
                                                            guint                   call_id);
gboolean       tracker_cancel_last_call                    (TrackerClient          *client);

/* Utilities */
gchar *        tracker_sparql_escape                       (const gchar            *str);

gchar *        tracker_uri_vprintf_escaped                 (const gchar            *format,
                                                            va_list                 args);
gchar *        tracker_uri_printf_escaped                  (const gchar            *format, 
                                                            ...);

/* Synchronous API */
GPtrArray *    tracker_statistics_get                      (TrackerClient          *client,
                                                            GError                **error);
void           tracker_resources_load                      (TrackerClient          *client,
                                                            const gchar            *uri,
                                                            GError                **error);
GPtrArray *    tracker_resources_sparql_query              (TrackerClient          *client,
                                                            const gchar            *query,
                                                            GError                **error);
TrackerResultIterator *
               tracker_resources_sparql_query_iterate      (TrackerClient          *client,
                                                            const gchar            *query,
                                                            GError                **error);
void           tracker_result_iterator_free                (TrackerResultIterator  *iterator);
guint          tracker_result_iterator_n_columns           (TrackerResultIterator  *iterator);
gboolean       tracker_result_iterator_next                (TrackerResultIterator  *iterator);
const gchar *  tracker_result_iterator_value               (TrackerResultIterator  *iterator,
                                                            guint                   column);
void           tracker_resources_sparql_update             (TrackerClient          *client,
                                                            const gchar            *query,
                                                            GError                **error);
GPtrArray *    tracker_resources_sparql_update_blank       (TrackerClient          *client,
                                                            const gchar            *query,
                                                            GError                **error);
void           tracker_resources_batch_sparql_update       (TrackerClient          *client,
                                                            const gchar            *query,
                                                            GError                **error);
void           tracker_resources_batch_commit              (TrackerClient          *client,
                                                            GError                **error);
/* Asynchronous API */
guint          tracker_statistics_get_async                (TrackerClient          *client,
                                                            TrackerReplyGPtrArray   callback,
                                                            gpointer                user_data);
guint          tracker_resources_load_async                (TrackerClient          *client,
                                                            const gchar            *uri,
                                                            TrackerReplyVoid        callback,
                                                            gpointer                user_data);
guint          tracker_resources_sparql_query_async        (TrackerClient          *client,
                                                            const gchar            *query,
                                                            TrackerReplyGPtrArray   callback,
                                                            gpointer                user_data);
guint         tracker_resources_sparql_query_iterate_async (TrackerClient          *client,
                                                            const gchar            *query,
                                                            TrackerReplyIterator    callback,
                                                            gpointer                user_data);
guint          tracker_resources_sparql_update_async       (TrackerClient          *client,
                                                            const gchar            *query,
                                                            TrackerReplyVoid        callback,
                                                            gpointer                user_data);
guint          tracker_resources_sparql_update_blank_async (TrackerClient          *client,
                                                            const gchar            *query,
                                                            TrackerReplyGPtrArray   callback,
                                                            gpointer                user_data);
guint          tracker_resources_batch_sparql_update_async (TrackerClient          *client,
                                                            const gchar            *query,
                                                            TrackerReplyVoid        callback,
                                                            gpointer                user_data);
guint          tracker_resources_batch_commit_async        (TrackerClient          *client,
                                                            TrackerReplyVoid        callback,
                                                            gpointer                user_data);

/* Store signals */
guint          tracker_resources_writeback_connect         (TrackerClient            *client,
                                                            TrackerWritebackCallback  callback,
                                                            gpointer                  user_data);
void           tracker_resources_writeback_disconnect      (TrackerClient            *client,
                                                            guint handle);

#ifndef TRACKER_DISABLE_DEPRECATED

/* Deprecated APIs */
/**
 * TrackerReplyArray:
 * @result: a gchar ** with the results of the query.
 * @error: a GError.
 * @user_data: a gpointer for user data.
 *
 * This is used by the 0.6 Tracker APIs:
 *   tracker_search_metadata_by_text_async()
 *   tracker_search_metadata_by_text_and_location_async()
 *   tracker_search_metadata_by_text_and_mime_async()
 *   tracker_search_metadata_by_text_and_mime_and_location_async()
 *
 * Deprecated: 0.8: Use #TrackerReplyVoid and #TrackerReplyGPtrArray
 * with tracker_resources_sparql_query() instead.
 */
typedef void (*TrackerReplyArray)     (gchar    **result,
                                       GError    *error,
                                       gpointer   user_data);

TrackerClient *
      tracker_connect                                             (gboolean            enable_warnings,
                                                                   gint                timeout)   G_GNUC_DEPRECATED;
void  tracker_disconnect                                          (TrackerClient      *client)    G_GNUC_DEPRECATED;

guint tracker_search_metadata_by_text_async                       (TrackerClient      *client,
                                                                   const gchar        *query,
                                                                   TrackerReplyArray   callback,
                                                                   gpointer            user_data) G_GNUC_DEPRECATED;
guint tracker_search_metadata_by_text_and_location_async          (TrackerClient      *client,
                                                                   const gchar        *query,
                                                                   const gchar        *location,
                                                                   TrackerReplyArray   callback,
                                                                   gpointer            user_data) G_GNUC_DEPRECATED;
guint tracker_search_metadata_by_text_and_mime_async              (TrackerClient      *client,
                                                                   const gchar        *query,
                                                                   const gchar       **mimes,
                                                                   TrackerReplyArray   callback,
                                                                   gpointer            user_data) G_GNUC_DEPRECATED;
guint tracker_search_metadata_by_text_and_mime_and_location_async (TrackerClient      *client,
                                                                   const gchar        *query,
                                                                   const gchar       **mimes,
                                                                   const gchar        *location,
                                                                   TrackerReplyArray   callback,
                                                                   gpointer            user_data) G_GNUC_DEPRECATED;

#endif /* TRACKER_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __TRACKER_CLIENT_H__ */
