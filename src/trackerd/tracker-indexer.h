


#ifndef _TRACKER_INDEXER_H                         /* duplication check */
#define _TRACKER_INDEXER_H


#include <depot.h>
#include <curia.h>
#include <cabin.h>
#include <vista.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>

#include "tracker-db.h"


typedef struct {                         /* type of structure for a database handle */
	CURIA *word_index;                  /* database handle for the inverted index */
	VILLA *blob_index;                   /* database handle for the docs contents */
	GMutex *mutex;
	GMutex *search_waiting_mutex;
	DBConnection *db_con;
} Indexer;


typedef struct {                         /* type of structure for an element of search result */
	unsigned int 	id;              /* Index ID number of the document's metadata */
	int 		score;           /* Score of the word in the document's metadata */
} WordPair;

typedef struct {                         /* type of structure for an element of search result */
	unsigned int 	id;              /* Index ID number of the document's metadata */
	int 		score;           /* Score of the word in the document's metadata */
} SearchHit;

Indexer * 	tracker_indexer_open 			(const char *name);
void		tracker_indexer_close 			(Indexer *indexer);
void		tracker_indexer_sweep			(Indexer *indexer);	
gboolean	tracker_indexer_optimize		(Indexer *indexer);

/* Indexing api */

gboolean	tracker_indexer_insert_word 		(Indexer *indexer, unsigned int id, const char *word,  int score);
WordPair *	tracker_indexer_get_hits		(Indexer *indexer, const char *word, int offset, int limit,  int *count);


/* blob api */
gboolean	tracker_indexer_insert_blob		(Indexer *indexer,  const char *text,  unsigned int id);
void		tracker_indexer_delete_blob		(Indexer *indexer,  unsigned int id);
char *		tracker_indexer_get_blob		(Indexer *indexer,  unsigned int id);





#endif                                   /* duplication check */


/* END OF FILE */
