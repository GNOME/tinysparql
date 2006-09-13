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
	DEPOT *word_index;                  /* database handle for the inverted word index */
	DEPOT *service_index;		    /* database handle for the serviceID -> uri table */
	VILLA *blob_index;                  /* database handle for the docs contents or unique word list*/
	
	GMutex *mutex;
	GMutex *search_waiting_mutex;
	DBConnection *db_con;
} Indexer;





typedef struct {                         /* type of structure for an element of search result */
	unsigned int 	id;              /* Index ID number of the document's metadata */
	int 		score;           /* Score of the word in the document's metadata */
} SearchHit;


Indexer * 	tracker_indexer_open 			(const char *name);
void		tracker_indexer_close 			(Indexer *indexer);
void		tracker_indexer_sweep			(Indexer *indexer);
gboolean	tracker_indexer_optimize		(Indexer *indexer);


/* Indexing api */

gboolean	tracker_indexer_insert_word 		(Indexer *indexer, unsigned int id, const char *word, int score);
SearchHit *	tracker_indexer_get_hits		(Indexer *indexer, const char *word, int offset, int limit, int *count);


/* blob api */
gboolean	tracker_indexer_insert_blob		(Indexer *indexer, const char *text, unsigned int id);
void		tracker_indexer_delete_blob		(Indexer *indexer, unsigned int id);
char *		tracker_indexer_get_blob		(Indexer *indexer, unsigned int id);

#endif
