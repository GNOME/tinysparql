/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef TRACKER_H
#define TRACKER_H

#include <dbus/dbus-glib-bindings.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_CLIENT         (tracker_client_get_type())
#define TRACKER_CLIENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CLIENT, TrackerClient))
#define TRACKER_CLIENT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_CLIENT, TrackerClientClass))
#define TRACKER_IS_CLIENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CLIENT))
#define TRACKER_IS_CLIENT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_CLIENT))
#define TRACKER_CLIENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_CLIENT, TrackerClientClass))

/**
 * TrackerClient:
 * @proxy_statistics: a #DBusGProxy for the connection to Tracker's
 * statistics D-Bus service.
 * @proxy_resources: a #DBusGProxy for the connection to Tracker's
 * resources D-Bus service.
 * @pending_calls: a #GHashTable with the D-Bus calls currently
 * pending (used for the tracker_cancel_call() API).
 * @last_call: a #guint representing the last API call with this
 * #TrackerClient.
 *
 * This structure is used by tracker_connect() and tracker_disconnect().
 */
typedef struct {
	GObject parent;

	DBusGProxy *proxy_statistics;
	DBusGProxy *proxy_resources;

	GHashTable *pending_calls;
	guint last_call;

} TrackerClient;

typedef struct {
	GObjectClass parent;
} TrackerClientClass;

/**
 * TrackerReplyArray:
 * @result: a gchar ** with the results of the query.
 * @error: a GError.
 * @user_data: a gpointer for user data.
 *
 * This is used by the old tracker_search_* API and is deprecated.
 */
typedef void (*TrackerReplyArray)     (gchar    **result,
                                       GError    *error,
                                       gpointer   user_data);

/**
 * TrackerReplyGPtrArray:
 * @result: a #GPtrArray with the results of the query.
 * @error: a #GError.
 * @user_data: a #gpointer for user data.
 *
 * The returned #GPtrArray contains an array of #GStrv with the
 * results from the query unless there is an error in the query. If
 * there is an error the @error is populated with the details.
 */
typedef void (*TrackerReplyGPtrArray) (GPtrArray *result,
                                       GError    *error,
                                       gpointer   user_data);

/**
 * TrackerReplyVoid:
 * @error: a GError.
 * @user_data: a gpointer for user data.
 *
 * If there is an error the @error is populated with the details.
 */
typedef void (*TrackerReplyVoid)      (GError    *error,
                                       gpointer   user_data);

GType          tracker_client_get_type                     (void) G_GNUC_CONST;

gboolean       tracker_cancel_call                         (TrackerClient          *client,
                                                            guint                   call_id);
gboolean       tracker_cancel_last_call                    (TrackerClient          *client);

gchar *        tracker_sparql_escape                       (const gchar            *str);

TrackerClient *tracker_connect                             (gboolean                enable_warnings,
                                                            gint                    timeout);
TrackerClient *tracker_connect_no_service_start            (gboolean                enable_warnings,
                                                            gint                    timeout);
void           tracker_disconnect                          (TrackerClient          *client);

/* Synchronous API */
GPtrArray *    tracker_statistics_get                      (TrackerClient          *client,
                                                            GError                **error);
void           tracker_resources_load                      (TrackerClient          *client,
                                                            const gchar            *uri,
                                                            GError                **error);
GPtrArray *    tracker_resources_sparql_query              (TrackerClient          *client,
                                                            const gchar            *query,
                                                            GError                **error);
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
guint          tracker_search_metadata_by_text_async       (TrackerClient          *client,
                                                            const gchar            *query,
                                                            TrackerReplyArray       callback,
                                                            gpointer                user_data);
guint          tracker_search_metadata_by_text_and_location_async (TrackerClient   *client,
                                                                   const gchar            *query,
                                                                   const gchar            *location,
                                                                   TrackerReplyArray       callback,
                                                                   gpointer                user_data);
guint          tracker_search_metadata_by_text_and_mime_async (TrackerClient   *client,
                                                               const gchar            *query,
                                                               const gchar           **mimes,
                                                               TrackerReplyArray       callback,
                                                               gpointer                user_data);
guint          tracker_search_metadata_by_text_and_mime_and_location_async (TrackerClient   *client,
                                                                            const gchar            *query,
                                                                            const gchar           **mimes,
                                                                            const gchar            *location,
                                                                            TrackerReplyArray       callback,
                                                                            gpointer                user_data);

G_END_DECLS

#endif /* TRACKER_H */
