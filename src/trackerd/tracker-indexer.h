/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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



#ifndef _TRACKER_INDEXER_H
#define _TRACKER_INDEXER_H


#include <stdlib.h>
#include <glib.h>
#include <depot.h>

#include "tracker-utils.h"

typedef struct {                         	 
	guint32 	service_id;              /* Service ID of the document */
	guint32 	service_type_id;         /* Service type ID of the document */
	guint32 	score;            	 /* Ranking score */
} SearchHit;


typedef enum {
	WordNormal,
	WordWildCard,
	WordExactPhrase
} WordType;


typedef struct {                        
	gchar	 	*word;    
	gint		hit_count;
	gfloat		idf;
	WordType	word_type;
} SearchWord;


typedef struct {
	DEPOT  		*word_index;	/* file hashtable handle for the word -> {serviceID, ServiceTypeID, Score}  */
	GMutex 		*word_mutex;
	char   		*name;
	gpointer  	emails; /* pointer to email indexer */
	gpointer  	data; /* pointer to file indexer */
	gboolean	main_index;
	gboolean	needs_merge; /* should new stuff be added directly or merged later on from a new index */
} Indexer;



typedef struct {                        
	Indexer 	*indexer;
	gint 		*service_array;    
	gint 		service_array_count;
	gint 		hit_count;
	GSList	        *hits;
	GSList		*words;
	GSList		*duds;
	gint		offset;
	gint		limit;
} SearchQuery;


typedef enum {
	BoolAnd,
	BoolOr,
	BoolNot
} BoolOp;


typedef enum 
{
	INDEX_TYPE_FILES,
	INDEX_TYPE_EMAILS,
	INDEX_TYPE_FILE_UPDATE
} IndexType;


SearchQuery * 	tracker_create_query 			(Indexer *indexer, gint *service_array, gint service_array_count, gint offset, gint limit);
void		tracker_free_query 			(SearchQuery *query);

void		tracker_add_query_word 			(SearchQuery *query, const gchar *word, WordType word_type);

guint32		tracker_indexer_calc_amalgamated 	(gint service, gint score);
void		tracker_index_free_hit_list		(GSList *hit_list);

Indexer * 	tracker_indexer_open 			(const gchar *name);
void		tracker_indexer_close 			(Indexer *indexer);
void		tracker_indexer_free 			(Indexer *indexer, gboolean remove_file);
gboolean	tracker_indexer_has_merge_index 	(Indexer *indexer, gboolean update);

guint32		tracker_indexer_size 			(Indexer *indexer);
gboolean	tracker_indexer_optimize		(Indexer *indexer);
void		tracker_indexer_sync 			(Indexer *indexer);

void		tracker_indexer_apply_changes 		(Indexer *dest, Indexer *src,  gboolean update);
void		tracker_indexer_merge_indexes 		(IndexType type);
gboolean	tracker_indexer_has_merge_files 	(IndexType type);

/* Indexing api */
gboolean	tracker_indexer_append_word 		(Indexer *indexer, const gchar *word, guint32 id, gint service, gint score);
gboolean	tracker_indexer_append_word_chunk 	(Indexer *indexer, const gchar *word, WordDetails *details, gint word_detail_count);
gint		tracker_indexer_append_word_list 	(Indexer *indexer, const gchar *word, GSList *list);

gboolean	tracker_indexer_update_word 		(Indexer *indexer, const gchar *word, guint32 id, gint service, gint score, gboolean remove_word);
gboolean	tracker_indexer_update_word_chunk	(Indexer *indexer, const gchar *word, WordDetails *details, gint word_detail_count);
gboolean	tracker_indexer_update_word_list 	(Indexer *indexer, const gchar *word, GSList *update_list);


gboolean	tracker_indexer_get_hits 		(SearchQuery *query);
gchar ***	tracker_get_hit_counts 			(SearchQuery *query);
gint		tracker_get_hit_count 			(SearchQuery *query);

gchar ***	tracker_get_words_starting_with 	(Indexer *indexer, const gchar *word);


#endif
