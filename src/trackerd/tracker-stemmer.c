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
	
	stem_word = (char *) sb_stemmer_stem (tracker->stemmer, (unsigned char*) word, word_length);

	return g_strdup (stem_word);
}


