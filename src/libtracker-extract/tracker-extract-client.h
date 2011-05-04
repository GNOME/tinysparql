/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_EXTRACT_CLIENT_H__
#define __TRACKER_EXTRACT_CLIENT_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct TrackerExtractInfo TrackerExtractInfo;

void                 tracker_extract_client_get_metadata        (GFile               *file,
                                                                 const gchar         *mime_type,
                                                                 GCancellable        *cancellable,
                                                                 GAsyncReadyCallback  callback,
                                                                 gpointer             user_data);

TrackerExtractInfo * tracker_extract_client_get_metadata_finish (GFile               *uri,
                                                                 GAsyncResult        *res,
                                                                 GError             **error);

void                 tracker_extract_client_cancel_for_prefix   (GFile               *uri);

G_CONST_RETURN gchar * tracker_extract_info_get_preupdate    (TrackerExtractInfo *info);
G_CONST_RETURN gchar * tracker_extract_info_get_update       (TrackerExtractInfo *info);
G_CONST_RETURN gchar * tracker_extract_info_get_where_clause (TrackerExtractInfo *info);

G_END_DECLS

#endif /* __TRACKER_EXTRACT_CLIENT_H__ */
