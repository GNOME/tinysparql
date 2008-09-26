/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#ifndef __TRACKERD_FIELD_DATA_H__
#define __TRACKERD_FIELD_DATA_H__

#include <glib-object.h>

#include "tracker-field.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_FIELD_DATA		(tracker_field_data_get_type ())
#define TRACKER_FIELD_DATA(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_FIELD_DATA, TrackerFieldData))
#define TRACKER_FIELD_DATA_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_FIELD_DATA, TrackerFieldDataClass))
#define TRACKER_IS_FIELD_DATA(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_FIELD_DATA))
#define TRACKER_IS_FIELD_DATA_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_FIELD_DATA))
#define TRACKER_FIELD_DATA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_FIELD_DATA, TrackerFieldDataClass))

typedef struct _TrackerFieldData      TrackerFieldData;
typedef struct _TrackerFieldDataClass TrackerFieldDataClass;

struct _TrackerFieldData {
	GObject      parent;
};

struct _TrackerFieldDataClass {
	GObjectClass parent_class;
};

GType		  tracker_field_data_get_type		 (void) G_GNUC_CONST;

TrackerFieldData *tracker_field_data_new		 (void);

const gchar *	  tracker_field_data_get_alias		 (TrackerFieldData *field_data);
const gchar *	  tracker_field_data_get_table_name	 (TrackerFieldData *field_data);
const gchar *	  tracker_field_data_get_field_name	 (TrackerFieldData *field_data);
const gchar *	  tracker_field_data_get_select_field	 (TrackerFieldData *field_data);
const gchar *	  tracker_field_data_get_where_field	 (TrackerFieldData *field_data);
const gchar *	  tracker_field_data_get_id_field	 (TrackerFieldData *field_data);
TrackerFieldType  tracker_field_data_get_data_type	 (TrackerFieldData *field_data);
gboolean	  tracker_field_data_get_multiple_values (TrackerFieldData *field_data);
gboolean	  tracker_field_data_get_is_select	 (TrackerFieldData *field_data);
gboolean	  tracker_field_data_get_is_condition	 (TrackerFieldData *field_data);
gboolean	  tracker_field_data_get_needs_join	 (TrackerFieldData *field_data);

void		  tracker_field_data_set_alias		 (TrackerFieldData *field_data,
							  const gchar	   *value);
void		  tracker_field_data_set_table_name	 (TrackerFieldData *field_data,
							  const gchar	   *value);
void		  tracker_field_data_set_field_name	 (TrackerFieldData *field_data,
							  const gchar	   *value);
void		  tracker_field_data_set_select_field	 (TrackerFieldData *field_data,
							  const gchar	   *value);
void		  tracker_field_data_set_where_field	 (TrackerFieldData *field_data,
							  const gchar	   *value);
void		  tracker_field_data_set_id_field	 (TrackerFieldData *field_data,
							  const gchar	   *value);
void		  tracker_field_data_set_data_type	 (TrackerFieldData *field_data,
							  TrackerFieldType  value);
void		  tracker_field_data_set_multiple_values (TrackerFieldData *field_data,
							  gboolean	    value);
void		  tracker_field_data_set_is_select	 (TrackerFieldData *field_data,
							  gboolean	    value);
void		  tracker_field_data_set_is_condition	 (TrackerFieldData *field_data,
							  gboolean	    value);
void		  tracker_field_data_set_needs_join	 (TrackerFieldData *field_data,
							  gboolean	    value);

G_END_DECLS

#endif /* __TRACKERD_FIELD_DATA_H__ */

