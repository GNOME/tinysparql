#include <stdlib.h> 
#include <string.h> 
#include <glib.h> 
#include "tracker-stemmer.h"
#include "tracker-utils.h"

extern Tracker *tracker;


char * 
tracker_stem (const char *word) 
{
	char *stem_word;

	int word_length = strlen (word);
	
	g_mutex_lock (tracker->stemmer_mutex);
	stem_word = (char *) sb_stemmer_stem (tracker->stemmer, (unsigned char*) word, word_length);
	g_mutex_unlock (tracker->stemmer_mutex);

	return g_strdup (stem_word);
}


