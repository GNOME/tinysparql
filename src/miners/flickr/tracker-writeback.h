/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
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

#ifndef __TRACKER_WRITEBACK_H__
#define __TRACKER_WRITEBACK_H__

#include <glib-object.h>

G_BEGIN_DECLS

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

void  tracker_writeback_init       (void);
void  tracker_writeback_shutdown   (void);
guint tracker_writeback_connect    (TrackerWritebackCallback callback,
				    gpointer                 user_data);
void  tracker_writeback_disconnect (guint                    handle);

G_END_DECLS

#endif /* __TRACKER_WRITEBACK_H__ */
