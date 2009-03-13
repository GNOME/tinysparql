/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __LIBTRACKER_KMAIL_H__
#define __LIBTRACKER_KMAIL_H__

#if !defined (TRACKER_ENABLE_INTERNALS) && !defined (TRACKER_COMPILATION)
#error "TRACKER_ENABLE_INTERNALS not defined, this must be defined to use tracker's internal functions"
#endif

#include <glib.h>

#include <libtracker-common/tracker-common.h>

G_BEGIN_DECLS

#if !defined (TRACKER_ENABLE_INTERNALS) && !defined (TRACKER_COMPILATION)
#error "TRACKER_ENABLE_INTERNALS not defined, this must be defined to use tracker's internal functions"
#endif

#include <glib.h>
#include <dbus/dbus-glib-bindings.h>

#include <tracker-indexer/tracker-indexer.h>

#include "tracker-kmail-common.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_KMAIL_INDEXER          (tracker_kmail_indexer_get_type())
#define TRACKER_KMAIL_INDEXER(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_KMAIL_INDEXER, TrackerKMailIndexer))
#define TRACKER_KMAIL_INDEXER_CLASS(c)      (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_KMAIL_INDEXER, TrackerKMailIndexerClass))
#define TRACKER_KMAIL_INDEXER_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_KMAIL_INDEXER, TrackerKMailIndexerClass))

G_BEGIN_DECLS

typedef struct TrackerKMailIndexer TrackerKMailIndexer;
typedef struct TrackerKMailIndexerClass TrackerKMailIndexerClass;

struct TrackerKMailIndexer {
	GObject parent;
};

struct TrackerKMailIndexerClass {
	GObjectClass parent;
};

GType  tracker_kmail_indexer_get_type   (void);

void  tracker_kmail_indexer_set         (TrackerKMailIndexer *object, 
					 const gchar *subject, 
					 const GStrv predicates,
					 const GStrv values,
					 const guint modseq,
					 DBusGMethodInvocation *context,
					 GError *derror);
void  tracker_kmail_indexer_set_many    (TrackerKMailIndexer *object, 
					 const GStrv subjects, 
					 const GPtrArray *predicates,
					 const GPtrArray *values,
					 const guint modseq,
					 DBusGMethodInvocation *context,
					 GError *derror);
void  tracker_kmail_indexer_unset_many  (TrackerKMailIndexer *object, 
					 const GStrv subjects, 
					 const guint modseq,
					 DBusGMethodInvocation *context,
					 GError *derror);
void  tracker_kmail_indexer_unset       (TrackerKMailIndexer *object, 
					 const gchar *subject, 
					 const guint modseq,
					 DBusGMethodInvocation *context,
					 GError *derror);
void  tracker_kmail_indexer_cleanup     (TrackerKMailIndexer *object, 
					 const guint modseq,
					 DBusGMethodInvocation *context,
					 GError *derror);

G_END_DECLS

#endif /* __LIBTRACKER_KMAIL_H__ */
