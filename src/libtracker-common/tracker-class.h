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

#ifndef __LIBTRACKER_COMMON_CLASS_H__
#define __LIBTRACKER_COMMON_CLASS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#define TRACKER_TYPE_DB_TYPE (tracker_db_type_get_type ())

typedef enum {
	TRACKER_DB_TYPE_UNKNOWN,
	TRACKER_DB_TYPE_DATA,
	TRACKER_DB_TYPE_INDEX,
	TRACKER_DB_TYPE_COMMON,
	TRACKER_DB_TYPE_CONTENT,
	TRACKER_DB_TYPE_EMAIL,
	TRACKER_DB_TYPE_FILES,
	TRACKER_DB_TYPE_CACHE,
	TRACKER_DB_TYPE_USER
} TrackerDBType;

GType tracker_db_type_get_type (void) G_GNUC_CONST;


#define TRACKER_TYPE_CLASS	     (tracker_class_get_type ())
#define TRACKER_CLASS(o)	     (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CLASS, TrackerClass))
#define TRACKER_CLASS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_CLASS, TrackerClassClass))
#define TRACKER_IS_CLASS(o)	     (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CLASS))
#define TRACKER_IS_CLASS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_CLASS))
#define TRACKER_CLASS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_CLASS, TrackerClassClass))

typedef struct _TrackerClass	    TrackerClass;
typedef struct _TrackerClassClass TrackerClassClass;

struct _TrackerClass {
	GObject      parent;
};

struct _TrackerClassClass {
	GObjectClass parent_class;
};

GType		tracker_class_get_type		     (void) G_GNUC_CONST;

TrackerClass *tracker_class_new			     (void);

gint		tracker_class_get_id			     (TrackerClass *service);
const gchar *	tracker_class_get_name		     (TrackerClass *service);
const gchar *	tracker_class_get_parent		     (TrackerClass *service);
const gchar *	tracker_class_get_property_prefix	     (TrackerClass *service);
const gchar *	tracker_class_get_content_metadata	     (TrackerClass *service);
const GSList *	tracker_class_get_key_metadata	     (TrackerClass *service);
TrackerDBType	tracker_class_get_db_type		     (TrackerClass *service);
gboolean	tracker_class_get_enabled		     (TrackerClass *service);
gboolean	tracker_class_get_embedded		     (TrackerClass *service);
gboolean	tracker_class_get_has_metadata	     (TrackerClass *service);
gboolean	tracker_class_get_has_full_text	     (TrackerClass *service);
gboolean	tracker_class_get_has_thumbs		     (TrackerClass *service);
gboolean	tracker_class_get_show_service_files	     (TrackerClass *service);
gboolean	tracker_class_get_show_service_directories (TrackerClass *service);

void		tracker_class_set_id			     (TrackerClass *service,
							      gint	      value);
void		tracker_class_set_name		     (TrackerClass *service,
							      const gchar    *value);
void		tracker_class_set_parent		     (TrackerClass *service,
							      const gchar    *value);
void		tracker_class_set_property_prefix	     (TrackerClass *service,
							      const gchar    *value);
void		tracker_class_set_content_metadata	     (TrackerClass *service,
							      const gchar    *value);
void		tracker_class_set_key_metadata	     (TrackerClass *service,
							      const GSList   *value);
void		tracker_class_set_db_type		     (TrackerClass *service,
							      TrackerDBType   value);
void		tracker_class_set_enabled		     (TrackerClass *service,
							      gboolean	      value);
void		tracker_class_set_embedded		     (TrackerClass *service,
							      gboolean	      value);
void		tracker_class_set_has_metadata	     (TrackerClass *service,
							      gboolean	      value);
void		tracker_class_set_has_full_text	     (TrackerClass *service,
							      gboolean	      value);
void		tracker_class_set_has_thumbs		     (TrackerClass *service,
							      gboolean	      value);
void		tracker_class_set_show_service_files	     (TrackerClass *service,
							      gboolean	      value);
void		tracker_class_set_show_service_directories (TrackerClass *service,
							      gboolean	      value);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_CLASS_H__ */

