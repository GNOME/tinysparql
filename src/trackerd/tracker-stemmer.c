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


