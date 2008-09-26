/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKERD_STATUS_H__
#define __TRACKERD_STATUS_H__

#include <glib-object.h>

#include <libtracker-common/tracker-config.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_STATUS (tracker_status_get_type ())

typedef enum {
	TRACKER_STATUS_INITIALIZING,
	TRACKER_STATUS_WATCHING,
	TRACKER_STATUS_INDEXING,
	TRACKER_STATUS_PENDING,
	TRACKER_STATUS_OPTIMIZING,
	TRACKER_STATUS_IDLE,
	TRACKER_STATUS_SHUTDOWN
} TrackerStatus;


gboolean      tracker_status_init		     (TrackerConfig *config);
void	      tracker_status_shutdown		     (void);

GType	      tracker_status_get_type		     (void) G_GNUC_CONST;
const gchar * tracker_status_to_string		     (TrackerStatus  status);
TrackerStatus tracker_status_get		     (void);
const gchar * tracker_status_get_as_string	     (void);
void	      tracker_status_set		     (TrackerStatus  new_status);
void	      tracker_status_set_and_signal	     (TrackerStatus  new_status);
void	      tracker_status_signal		     (void);

gboolean      tracker_status_get_is_readonly	     (void);
void	      tracker_status_set_is_readonly	     (gboolean	     value);

gboolean      tracker_status_get_is_running	     (void);
void	      tracker_status_set_is_running	     (gboolean	     value);

void	      tracker_status_set_is_first_time_index (gboolean	     value);
gboolean      tracker_status_get_is_first_time_index (void);

gboolean      tracker_status_get_in_merge	     (void);
void	      tracker_status_set_in_merge	     (gboolean	     value);

gboolean      tracker_status_get_is_paused_manually  (void);
void	      tracker_status_set_is_paused_manually  (gboolean	     value);

gboolean      tracker_status_get_is_paused_for_io    (void);
void	      tracker_status_set_is_paused_for_io    (gboolean	     value);

G_END_DECLS

#endif /* __TRACKERD_STATUS_H__ */
