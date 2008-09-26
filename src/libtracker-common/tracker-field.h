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

#ifndef __TRACKERD_FIELD_H__
#define __TRACKERD_FIELD_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_FIELD_TYPE (tracker_field_type_get_type ())

typedef enum {
	TRACKER_FIELD_TYPE_KEYWORD,
	TRACKER_FIELD_TYPE_INDEX,
	TRACKER_FIELD_TYPE_FULLTEXT,
	TRACKER_FIELD_TYPE_STRING,
	TRACKER_FIELD_TYPE_INTEGER,
	TRACKER_FIELD_TYPE_DOUBLE,
	TRACKER_FIELD_TYPE_DATE,
	TRACKER_FIELD_TYPE_BLOB,
	TRACKER_FIELD_TYPE_STRUCT,
	TRACKER_FIELD_TYPE_LINK,
} TrackerFieldType;

GType	     tracker_field_type_get_type  (void) G_GNUC_CONST;
const gchar *tracker_field_type_to_string (TrackerFieldType fieldtype);



#define TRACKER_TYPE_FIELD	   (tracker_field_get_type ())
#define TRACKER_TYPE_FIELD_TYPE    (tracker_field_type_get_type ())
#define TRACKER_FIELD(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_FIELD, TrackerField))
#define TRACKER_FIELD_CLASS(k)	   (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_FIELD, TrackerFieldClass))
#define TRACKER_IS_FIELD(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_FIELD))
#define TRACKER_IS_FIELD_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_FIELD))
#define TRACKER_FIELD_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_FIELD, TrackerFieldClass))

typedef struct _TrackerField	  TrackerField;
typedef struct _TrackerFieldClass TrackerFieldClass;

struct _TrackerField {
	GObject      parent;
};

struct _TrackerFieldClass {
	GObjectClass parent_class;
};

GType		 tracker_field_get_type		   (void) G_GNUC_CONST;

TrackerField *	 tracker_field_new		   (void);

const gchar *	 tracker_field_get_id		   (TrackerField     *field);
const gchar *	 tracker_field_get_name		   (TrackerField     *field);
TrackerFieldType tracker_field_get_data_type	   (TrackerField     *field);
const gchar *	 tracker_field_get_field_name	   (TrackerField     *field);
gint		 tracker_field_get_weight	   (TrackerField     *service);
gboolean	 tracker_field_get_embedded	   (TrackerField     *field);
gboolean	 tracker_field_get_multiple_values (TrackerField     *field);
gboolean	 tracker_field_get_delimited	   (TrackerField     *field);
gboolean	 tracker_field_get_filtered	   (TrackerField     *field);
gboolean	 tracker_field_get_store_metadata  (TrackerField     *field);
const GSList *	 tracker_field_get_child_ids	   (TrackerField     *field);

void		 tracker_field_set_id		   (TrackerField     *field,
						    const gchar      *value);
void		 tracker_field_set_name		   (TrackerField     *field,
						    const gchar      *value);
void		 tracker_field_set_data_type	   (TrackerField     *field,
						    TrackerFieldType  value);
void		 tracker_field_set_field_name	   (TrackerField     *field,
						    const gchar      *value);
void		 tracker_field_set_weight	   (TrackerField     *field,
						    gint	      value);
void		 tracker_field_set_embedded	   (TrackerField     *field,
						    gboolean	      value);
void		 tracker_field_set_multiple_values (TrackerField     *field,
						    gboolean	      value);
void		 tracker_field_set_delimited	   (TrackerField     *field,
						    gboolean	      value);
void		 tracker_field_set_filtered	   (TrackerField     *field,
						    gboolean	      value);
void		 tracker_field_set_store_metadata  (TrackerField     *field,
						    gboolean	      value);
void		 tracker_field_set_child_ids	   (TrackerField     *field,
						    const GSList     *value);
void		 tracker_field_append_child_id	   (TrackerField     *field,
						    const gchar      *id);

G_END_DECLS

#endif /* __TRACKERD_FIELD_H__ */

