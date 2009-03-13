/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2007 Neil Jagdish Patel <njpatel@gmail.com>
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
 *
 * Author : Neil Jagdish Patel <njpatel@gmail.com>
 */

#ifndef TRACKER_TAG_BAR_H
#define TRACKER_TAG_BAR_H

#include <gtk/gtk.h>
#include <tracker.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_TAG_BAR		(tracker_tag_bar_get_type ())
#define TRACKER_TAG_BAR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_TAG_BAR, TrackerTagBar))
#define TRACKER_TAG_BAR_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), TRACKER_TAG_BAR, TrackerTagBarClass))
#define TRACKER_IS_TAG_BAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_TAG_BAR))
#define TRACKER_IS_TAG_BAR_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), TRACKER_TYPE_TAG_BAR))
#define TRACKER_TAG_BAR_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_TAG_BAR, TrackerTagBarClass))

typedef struct _TrackerTagBar		TrackerTagBar;
typedef struct _TrackerTagBarClass	TrackerTagBarClass;

struct _TrackerTagBar
{
	GtkHBox parent;
};

struct _TrackerTagBarClass
{
	GtkHBoxClass parent_class;
};

GtkWidget *tracker_tag_bar_new (void);

/*
uri has to be a local uri i.e.
'/home/john/doe.mp3' not 'file:///home/john/doe.mp3'
*/
void	   tracker_tag_bar_set_uri (TrackerTagBar		*bar,
				    ServiceType			type,
				    const gchar			*uri
				   );

GType tracker_tag_bar_get_type (void);

G_END_DECLS

#endif /* TRACKER_TAG_BAR_H */
