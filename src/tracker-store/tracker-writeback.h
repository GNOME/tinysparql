/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_STORE_WRITEBACK_H__
#define __TRACKER_STORE_WRITEBACK_H__

#include <libtracker-common/tracker-dbus.h>

G_BEGIN_DECLS

typedef GStrv (*TrackerWritebackGetPredicatesFunc) (void);

void        tracker_writeback_init        (TrackerWritebackGetPredicatesFunc callback);
void        tracker_writeback_shutdown    (void);
void        tracker_writeback_check       (gint         graph_id,
                                           gint         subject_id,
                                           const gchar *subject,
                                           gint         pred_id,
                                           gint         object_id,
                                           const gchar *object,
                                           GPtrArray   *rdf_types);
GHashTable* tracker_writeback_get_pending (void);
void        tracker_writeback_reset       (void);

G_END_DECLS

#endif /* __TRACKER_STORE_WRITEBACK_H__ */
