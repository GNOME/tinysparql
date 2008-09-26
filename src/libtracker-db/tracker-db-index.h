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

#ifndef __TRACKER_DB_INDEX_H__
#define __TRACKER_DB_INDEX_H__

G_BEGIN_DECLS

#include <glib-object.h>

#include <libtracker-db/tracker-db-index-item.h>


#define TRACKER_TYPE_DB_INDEX	      (tracker_db_index_get_type())
#define TRACKER_DB_INDEX(o)	      (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DB_INDEX, TrackerDBIndex))
#define TRACKER_DB_INDEX_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_DB_INDEX, TrackerDBIndexClass))
#define TRACKER_IS_DB_INDEX(o)	      (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DB_INDEX))
#define TRACKER_IS_DB_INDEX_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TRACKER_TYPE_DB_INDEX))
#define TRACKER_DB_INDEX_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_DB_INDEX, TrackerDBIndexClass))

typedef struct TrackerDBIndex		 TrackerDBIndex;
typedef struct TrackerDBIndexClass	 TrackerDBIndexClass;
typedef struct TrackerDBIndexWordDetails TrackerDBIndexWordDetails;

struct TrackerDBIndex {
	GObject parent;
};

struct TrackerDBIndexClass {
	GObjectClass parent_class;
};

GType		    tracker_db_index_get_type	     (void);
TrackerDBIndex *    tracker_db_index_new	     (const gchar    *filename,
						      gint	      min_bucket,
						      gint	      max_bucket,
						      gboolean	      readonly);
void		    tracker_db_index_set_filename    (TrackerDBIndex *index,
						      const gchar    *filename);
void		    tracker_db_index_set_min_bucket  (TrackerDBIndex *index,
						      gint	      min_bucket);
void		    tracker_db_index_set_max_bucket  (TrackerDBIndex *index,
						      gint	      max_bucket);
void		    tracker_db_index_set_reload      (TrackerDBIndex *index,
						      gboolean	      reload);
void		    tracker_db_index_set_readonly    (TrackerDBIndex *index,
						      gboolean	      readonly);
gboolean	    tracker_db_index_get_reload      (TrackerDBIndex *index);
gboolean	    tracker_db_index_get_readonly    (TrackerDBIndex *index);

void		    tracker_db_index_set_paused      (TrackerDBIndex *index,
						      gboolean paused);

/* Open/Close/Flush */
gboolean	    tracker_db_index_open	     (TrackerDBIndex *index);
gboolean	    tracker_db_index_close	     (TrackerDBIndex *index);
guint		    tracker_db_index_flush	     (TrackerDBIndex *index);

/* Using the index */
guint32		    tracker_db_index_get_size	     (TrackerDBIndex *index);
char *		    tracker_db_index_get_suggestion  (TrackerDBIndex *index,
						      const gchar    *term,
						      gint	      maxdist);
TrackerDBIndexItem *tracker_db_index_get_word_hits   (TrackerDBIndex *index,
						      const gchar    *word,
						      guint	     *count);
void		    tracker_db_index_add_word	     (TrackerDBIndex *index,
						      const gchar    *word,
						      guint32	      service_id,
						      gint	      service_type,
						      gint	      weight);
gboolean	    tracker_db_index_remove_dud_hits (TrackerDBIndex *index,
						      const gchar    *word,
						      GSList	     *dud_list);

G_END_DECLS

#endif /* __TRACKER_DB_INDEX_H__ */
