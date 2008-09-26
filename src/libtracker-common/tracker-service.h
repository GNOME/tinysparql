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

#ifndef __TRACKERD_SERVICES_H__
#define __TRACKERD_SERVICES_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_DB_TYPE (tracker_db_type_get_type ())

typedef enum {
	TRACKER_DB_TYPE_UNKNOWN,
	TRACKER_DB_TYPE_DATA,
	TRACKER_DB_TYPE_INDEX,
	TRACKER_DB_TYPE_COMMON,
	TRACKER_DB_TYPE_CONTENT,
	TRACKER_DB_TYPE_EMAIL,
	TRACKER_DB_TYPE_FILES,
	TRACKER_DB_TYPE_XESAM,
	TRACKER_DB_TYPE_CACHE,
	TRACKER_DB_TYPE_USER
} TrackerDBType;

GType tracker_db_type_get_type (void) G_GNUC_CONST;


#define TRACKER_TYPE_SERVICE	     (tracker_service_get_type ())
#define TRACKER_SERVICE(o)	     (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_SERVICE, TrackerService))
#define TRACKER_SERVICE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_SERVICE, TrackerServiceClass))
#define TRACKER_IS_SERVICE(o)	     (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_SERVICE))
#define TRACKER_IS_SERVICE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_SERVICE))
#define TRACKER_SERVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_SERVICE, TrackerServiceClass))

typedef struct _TrackerService	    TrackerService;
typedef struct _TrackerServiceClass TrackerServiceClass;

struct _TrackerService {
	GObject      parent;
};

struct _TrackerServiceClass {
	GObjectClass parent_class;
};

GType		tracker_service_get_type		     (void) G_GNUC_CONST;

TrackerService *tracker_service_new			     (void);

gint		tracker_service_get_id			     (TrackerService *service);
const gchar *	tracker_service_get_name		     (TrackerService *service);
const gchar *	tracker_service_get_parent		     (TrackerService *service);
const gchar *	tracker_service_get_property_prefix	     (TrackerService *service);
const gchar *	tracker_service_get_content_metadata	     (TrackerService *service);
const GSList *	tracker_service_get_key_metadata	     (TrackerService *service);
TrackerDBType	tracker_service_get_db_type		     (TrackerService *service);
gboolean	tracker_service_get_enabled		     (TrackerService *service);
gboolean	tracker_service_get_embedded		     (TrackerService *service);
gboolean	tracker_service_get_has_metadata	     (TrackerService *service);
gboolean	tracker_service_get_has_full_text	     (TrackerService *service);
gboolean	tracker_service_get_has_thumbs		     (TrackerService *service);
gboolean	tracker_service_get_show_service_files	     (TrackerService *service);
gboolean	tracker_service_get_show_service_directories (TrackerService *service);

void		tracker_service_set_id			     (TrackerService *service,
							      gint	      value);
void		tracker_service_set_name		     (TrackerService *service,
							      const gchar    *value);
void		tracker_service_set_parent		     (TrackerService *service,
							      const gchar    *value);
void		tracker_service_set_property_prefix	     (TrackerService *service,
							      const gchar    *value);
void		tracker_service_set_content_metadata	     (TrackerService *service,
							      const gchar    *value);
void		tracker_service_set_key_metadata	     (TrackerService *service,
							      const GSList   *value);
void		tracker_service_set_db_type		     (TrackerService *service,
							      TrackerDBType   value);
void		tracker_service_set_enabled		     (TrackerService *service,
							      gboolean	      value);
void		tracker_service_set_embedded		     (TrackerService *service,
							      gboolean	      value);
void		tracker_service_set_has_metadata	     (TrackerService *service,
							      gboolean	      value);
void		tracker_service_set_has_full_text	     (TrackerService *service,
							      gboolean	      value);
void		tracker_service_set_has_thumbs		     (TrackerService *service,
							      gboolean	      value);
void		tracker_service_set_show_service_files	     (TrackerService *service,
							      gboolean	      value);
void		tracker_service_set_show_service_directories (TrackerService *service,
							      gboolean	      value);

G_END_DECLS

#endif /* __TRACKERD_SERVICE_H__ */

