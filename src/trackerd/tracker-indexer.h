/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _TRACKER_INDEXER_H
#define _TRACKER_INDEXER_H

#include <depot.h>
#include <cabin.h>
#include <vista.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>

#include "tracker-db.h"


typedef struct {	
	CURIA *word_index;                  /* database handle for the word -> {serviceID, MetadataID, ServiceTypeID, Score}  */
	VILLA *blob_index;                  /* database handle for the docs contents or unique word list*/
	GMutex *word_mutex;
	GMutex *blob_mutex;
	DBConnection *db_con;
} Indexer;


typedef struct {                         	 /* type of structure for an element of search result */
	guint32 	service_id;              /* ServiceID of the document */
	guint32 	metadata_id;             /* Metadata Id of the document */
	guint32 	service_type_id;         /* Service type ID of the document */
	guint32 	score;            	 /* Score/rank */
} SearchHit;


typedef struct {                         	
	char		*word;			 /* word to be indexed */
	guint32 	service_id;              /* ServiceID of the document */
	guint32 	metadata_id;             /* Metadata Id of the document */
	guint32 	service_type_id;         /* Service type ID of the document */
	guint32 	score;            	 /* Score/rank */
} IndexWord;



Indexer * 	tracker_indexer_open 			(const char *name);
void		tracker_indexer_close 			(Indexer *indexer);
gboolean	tracker_indexer_optimize		(Indexer *indexer);


/* Indexing api */

gboolean	tracker_indexer_insert_word 			(Indexer *indexer, IndexWord *index_word);
gboolean	tracker_indexer_remove_word 			(Indexer *indexer, IndexWord *index_word);
gboolean	tracker_indexer_remove_word_for_id 		(Indexer *indexer, guint32 service_id, const char *word);
gboolean	tracker_indexer_remove_word_by_meta_ids		(Indexer *indexer, guint32 service_id, const char *word, GSList *meta_ids);

GSList *	tracker_indexer_get_hits			(Indexer *indexer, char **words, int service_type_min, int service_type_max, int metadata_type, int offset, int limit, int *total_count);
GHashTable *	tracker_indexer_get_hit_count_by_service	(Indexer *indexer, char **words,  int *total_count);

/* blob api */
gboolean	tracker_indexer_insert_blob		(Indexer *indexer, const char *text, guint32 id);
void		tracker_indexer_delete_blob		(Indexer *indexer, guint32 id);

char *		tracker_indexer_get_blob		(Indexer *indexer, guint32 id, int offset, int limit);
char *		tracker_indexer_get_blob_snippet	(Indexer *indexer, guint32 id, const char *word);

#endif
