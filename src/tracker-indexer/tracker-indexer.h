/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#ifndef __TRACKER_INDEXER_H__
#define __TRACKER_INDEXER_H__

#include <glib-object.h>
#include <dbus/dbus-glib.h>

#define TRACKER_DAEMON_SERVICE	     "org.freedesktop.Tracker"
#define TRACKER_INDEXER_SERVICE      "org.freedesktop.Tracker.Indexer"
#define TRACKER_INDEXER_PATH	     "/org/freedesktop/Tracker/Indexer"
#define TRACKER_INDEXER_INTERFACE    "org.freedesktop.Tracker.Indexer"

G_BEGIN_DECLS

#define TRACKER_TYPE_INDEXER	     (tracker_indexer_get_type())
#define TRACKER_INDEXER(o)	     (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_INDEXER, TrackerIndexer))
#define TRACKER_INDEXER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_INDEXER, TrackerIndexerClass))
#define TRACKER_IS_INDEXER(o)	     (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_INDEXER))
#define TRACKER_IS_INDEXER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_INDEXER))
#define TRACKER_INDEXER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_INDEXER, TrackerIndexerClass))

typedef struct TrackerIndexer	     TrackerIndexer;
typedef struct TrackerIndexerClass   TrackerIndexerClass;
typedef struct TrackerIndexerPrivate TrackerIndexerPrivate;

struct TrackerIndexer {
	GObject parent_instance;

	/* Private struct */
	TrackerIndexerPrivate *private;
};

struct TrackerIndexerClass {
	GObjectClass parent_class;

	void (*status)		(TrackerIndexer *indexer,
				 gdouble	 seconds_elapsed,
				 const gchar	*current_module_name,
				 guint		 items_indexed,
				 guint		 items_remaining);
	void (*started)		(TrackerIndexer *indexer);
	void (*paused)		(TrackerIndexer *indexer);
	void (*continued)	(TrackerIndexer *indexer);
	void (*finished)	(TrackerIndexer *indexer,
				 gdouble	 seconds_elapsed,
				 guint		 items_indexed);
	void (*module_started)	(TrackerIndexer *indexer,
				 const gchar	*module_name);
	void (*module_finished) (TrackerIndexer *indexer,
				 const gchar	*module_name);
};

GType		tracker_indexer_get_type	   (void) G_GNUC_CONST;

TrackerIndexer *tracker_indexer_new		   (void);
gboolean	tracker_indexer_get_running	   (TrackerIndexer	   *indexer);
void		tracker_indexer_set_running	   (TrackerIndexer	   *indexer,
						    gboolean		    running);
void		tracker_indexer_stop		   (TrackerIndexer	   *indexer);
void		tracker_indexer_process_all	   (TrackerIndexer	   *indexer);

/* DBus methods */
void		tracker_indexer_pause		   (TrackerIndexer	   *indexer,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_pause_for_duration (TrackerIndexer	   *indexer,
						    guint		    seconds,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_continue	   (TrackerIndexer	   *indexer,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_files_check	   (TrackerIndexer	   *indexer,
						    const gchar		   *module,
						    GStrv		    files,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_files_update	   (TrackerIndexer	   *indexer,
						    const gchar		   *module,
						    GStrv		    files,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_files_delete	   (TrackerIndexer	   *indexer,
						    const gchar		   *module,
						    GStrv		    files,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_file_move	   (TrackerIndexer	   *indexer,
						    const gchar		   *module_name,
						    gchar		   *from,
						    gchar		   *to,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_property_set	   (TrackerIndexer	   *indexer,
						    const gchar		   *service_type,
						    const gchar		   *uri,
						    const gchar		   *property,
						    GStrv		    values,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_property_remove    (TrackerIndexer	   *indexer,
						    const gchar		   *service_type,
						    const gchar		   *uri,
						    const gchar		   *property,
						    GStrv		    values,
						    DBusGMethodInvocation  *context,
						    GError		  **error);
void		tracker_indexer_shutdown	   (TrackerIndexer	   *indexer,
						    DBusGMethodInvocation  *context,
						    GError		  **error);

G_END_DECLS

#endif /* __TRACKER_INDEXER_H__ */
