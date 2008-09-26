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

#ifndef __TRACKER_INDEXER_DB_H__
#define __TRACKER_INDEXER_DB_H__

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-db/tracker-db-interface.h>
#include "tracker-metadata.h"

G_BEGIN_DECLS
guint32		 tracker_db_get_new_service_id	  (TrackerDBInterface *iface);
void		 tracker_db_increment_stats	  (TrackerDBInterface *iface,
						   TrackerService     *service);
void		 tracker_db_decrement_stats	  (TrackerDBInterface *iface,
						   TrackerService     *service);

/* Using path */
gboolean	 tracker_db_check_service	  (TrackerService     *service,
						   const gchar	      *dirname,
						   const gchar	      *basename,
						   guint32	      *id,
						   time_t	      *mtime);
guint		 tracker_db_get_service_type	  (const gchar	      *dirname,
						   const gchar	      *basename);


/* Services  */
gboolean	 tracker_db_create_service	  (TrackerService     *service,
						   guint32	       id,
						   const gchar	      *dirname,
						   const gchar	      *basename,
						   TrackerMetadata    *metadata);
void		 tracker_db_delete_service	  (TrackerService     *service,
						   guint32	       id);
void		 tracker_db_move_service	  (TrackerService     *service,
						   const gchar	      *from,
						   const gchar	      *to);


/* Metadata */
void		 tracker_db_set_metadata	  (TrackerService     *service,
						   guint32	       id,
						   TrackerField       *field,
						   const gchar	      *value,
						   const gchar	      *parsed_value);
TrackerMetadata *tracker_db_get_all_metadata	  (TrackerService     *service,
						   guint32	       id,
						   gboolean	       only_embedded);
gchar	*	 tracker_db_get_parsed_metadata   (TrackerService     *service,
						   guint32	       id);
gchar	*	 tracker_db_get_unparsed_metadata (TrackerService     *service,
						   guint32	       id);
gchar  **	 tracker_db_get_property_values   (TrackerService     *service_def,
						   guint32	       id,
						   TrackerField       *field_def);
void		 tracker_db_delete_all_metadata   (TrackerService     *service,
						   guint32	       id);
void		 tracker_db_delete_metadata	  (TrackerService     *service,
						   guint32	       id,
						   TrackerField       *field,
						   const gchar	      *value);

/* Contents */
void		 tracker_db_set_text		  (TrackerService     *service,
						   guint32	       id,
						   const gchar	      *text);
gchar	*	 tracker_db_get_text		  (TrackerService     *service,
						   guint32	       id);
void		 tracker_db_delete_text		  (TrackerService     *service,
						   guint32	       id);



/* Events */
void		 tracker_db_create_event	  (TrackerDBInterface *iface,
						   guint32	       service_id,
						   const gchar	      *type);




G_END_DECLS

#endif /* __TRACKER_INDEXER_DB_H__ */
