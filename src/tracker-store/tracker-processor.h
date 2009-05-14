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
 */

#ifndef __TRACKERD_PROCESSOR_H__
#define __TRACKERD_PROCESSOR_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-hal.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_PROCESSOR	       (tracker_processor_get_type())
#define TRACKER_PROCESSOR(o)	       (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PROCESSOR, TrackerProcessor))
#define TRACKER_PROCESSOR_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),	 TRACKER_TYPE_PROCESSOR, TrackerProcessorClass))
#define TRACKER_IS_PROCESSOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PROCESSOR))
#define TRACKER_IS_PROCESSOR_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),	 TRACKER_TYPE_PROCESSOR))
#define TRACKER_PROCESSOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_PROCESSOR, TrackerProcessorClass))

typedef struct TrackerProcessor        TrackerProcessor;
typedef struct TrackerProcessorClass   TrackerProcessorClass;
typedef struct TrackerProcessorPrivate TrackerProcessorPrivate;

struct TrackerProcessor {
	GObject			 parent;
	TrackerProcessorPrivate *private;
};

struct TrackerProcessorClass {
	GObjectClass		 parent;

	void (*finished) (TrackerProcessor *processor);
};

GType		  tracker_processor_get_type		    (void) G_GNUC_CONST;

TrackerProcessor *tracker_processor_new			    (TrackerConfig    *config,
							     TrackerHal       *hal);
void		  tracker_processor_start		    (TrackerProcessor *processor);
void		  tracker_processor_stop		    (TrackerProcessor *processor);

/* Required API for org.freedesktop.Tracker.Files */
void		  tracker_processor_files_check		    (TrackerProcessor *processor,
							     const gchar      *module_name,
							     GFile	      *file,
							     gboolean	       is_directory);
void		  tracker_processor_files_update	    (TrackerProcessor *processor,
							     const gchar      *module_name,
							     GFile	      *file,
							     gboolean	       is_directory);
void		  tracker_processor_files_delete	    (TrackerProcessor *processor,
							     const gchar      *module_name,
							     GFile	      *file,
							     gboolean	       is_directory);
void		  tracker_processor_files_move		    (TrackerProcessor *processor,
							     const gchar      *module_name,
							     GFile	      *file,
							     GFile	      *other_file,
							     gboolean	       is_directory);

/* Statistics */
guint		  tracker_processor_get_directories_found   (TrackerProcessor *processor);
guint		  tracker_processor_get_directories_ignored (TrackerProcessor *processor);
guint		  tracker_processor_get_directories_total   (TrackerProcessor *processor);
guint		  tracker_processor_get_files_found	    (TrackerProcessor *processor);
guint		  tracker_processor_get_files_ignored	    (TrackerProcessor *processor);
guint		  tracker_processor_get_files_total	    (TrackerProcessor *processor);
gdouble		  tracker_processor_get_seconds_elapsed     (TrackerProcessor *processor);

G_END_DECLS

#endif /* __TRACKERD_PROCESSOR_H__ */
