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

#ifndef __LIBTRACKER_COMMON_PROPERTY_H__
#define __LIBTRACKER_COMMON_PROPERTY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#define TRACKER_TYPE_PROPERTY_TYPE (tracker_property_type_get_type ())

typedef enum {
	TRACKER_PROPERTY_TYPE_KEYWORD,
	TRACKER_PROPERTY_TYPE_INDEX,
	TRACKER_PROPERTY_TYPE_FULLTEXT,
	TRACKER_PROPERTY_TYPE_STRING,
	TRACKER_PROPERTY_TYPE_INTEGER,
	TRACKER_PROPERTY_TYPE_DOUBLE,
	TRACKER_PROPERTY_TYPE_DATE,
	TRACKER_PROPERTY_TYPE_BLOB,
	TRACKER_PROPERTY_TYPE_STRUCT,
	TRACKER_PROPERTY_TYPE_LINK,
} TrackerPropertyType;

GType	     tracker_property_type_get_type  (void) G_GNUC_CONST;
const gchar *tracker_property_type_to_string (TrackerPropertyType fieldtype);



#define TRACKER_TYPE_PROPERTY	   (tracker_property_get_type ())
#define TRACKER_TYPE_PROPERTY_TYPE    (tracker_property_type_get_type ())
#define TRACKER_PROPERTY(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PROPERTY, TrackerProperty))
#define TRACKER_PROPERTY_CLASS(k)	   (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_PROPERTY, TrackerPropertyClass))
#define TRACKER_IS_PROPERTY(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PROPERTY))
#define TRACKER_IS_PROPERTY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_PROPERTY))
#define TRACKER_PROPERTY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_PROPERTY, TrackerPropertyClass))

typedef struct _TrackerProperty	  TrackerProperty;
typedef struct _TrackerPropertyClass TrackerPropertyClass;

struct _TrackerProperty {
	GObject      parent;
};

struct _TrackerPropertyClass {
	GObjectClass parent_class;
};

GType		 tracker_property_get_type		   (void) G_GNUC_CONST;

TrackerProperty *	 tracker_property_new		   (void);

const gchar *	 tracker_property_get_id		   (TrackerProperty     *field);
const gchar *	 tracker_property_get_name		   (TrackerProperty     *field);
TrackerPropertyType tracker_property_get_data_type	   (TrackerProperty     *field);
const gchar *	 tracker_property_get_field_name	   (TrackerProperty     *field);
gint		 tracker_property_get_weight	   (TrackerProperty     *service);
gboolean	 tracker_property_get_embedded	   (TrackerProperty     *field);
gboolean	 tracker_property_get_multiple_values (TrackerProperty     *field);
gboolean	 tracker_property_get_delimited	   (TrackerProperty     *field);
gboolean	 tracker_property_get_filtered	   (TrackerProperty     *field);
gboolean	 tracker_property_get_store_metadata  (TrackerProperty     *field);
const GSList *	 tracker_property_get_child_ids	   (TrackerProperty     *field);

void		 tracker_property_set_id		   (TrackerProperty     *field,
						    const gchar      *value);
void		 tracker_property_set_name		   (TrackerProperty     *field,
						    const gchar      *value);
void		 tracker_property_set_data_type	   (TrackerProperty     *field,
						    TrackerPropertyType  value);
void		 tracker_property_set_field_name	   (TrackerProperty     *field,
						    const gchar      *value);
void		 tracker_property_set_weight	   (TrackerProperty     *field,
						    gint	      value);
void		 tracker_property_set_embedded	   (TrackerProperty     *field,
						    gboolean	      value);
void		 tracker_property_set_multiple_values (TrackerProperty     *field,
						    gboolean	      value);
void		 tracker_property_set_delimited	   (TrackerProperty     *field,
						    gboolean	      value);
void		 tracker_property_set_filtered	   (TrackerProperty     *field,
						    gboolean	      value);
void		 tracker_property_set_store_metadata  (TrackerProperty     *field,
						    gboolean	      value);
void		 tracker_property_set_child_ids	   (TrackerProperty     *field,
						    const GSList     *value);
void		 tracker_property_append_child_id	   (TrackerProperty     *field,
						    const gchar      *id);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_PROPERTY_H__ */

