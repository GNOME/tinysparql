/*
     This file is part of libextractor.
     (C) 2002, 2003 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */

#include "platform.h"
#include "extractor.h"

static void addKeyword(struct EXTRACTOR_Keywords ** list,
		       char * keyword,
	       EXTRACTOR_KeywordType type) {
  EXTRACTOR_KeywordList * next;
  next = malloc(sizeof(EXTRACTOR_KeywordList));
  next->next = *list;
  next->keyword = strdup(keyword);
  next->keywordType = type;
  *list = next;
}

/* convert other keywords to lower case */
struct EXTRACTOR_Keywords * libextractor_lower_extract(char * filename,
						       char * data,
						       size_t size,
						       struct EXTRACTOR_Keywords * prev) {
  struct EXTRACTOR_Keywords * pos;
  char *lower;
  unsigned int mem, needed, i;

  pos = prev;
  lower = 0;
  mem = 0;

  while (pos != NULL)
  {
    needed = strlen(pos->keyword) + 1;
    if (needed > mem)
    {
     lower = (char *) (lower) ? realloc(lower, needed) : malloc(needed);
     mem = needed;
    }

    for(i = 0; i < needed; i++)
    {
     lower[i] = tolower(pos->keyword[i]);
    }

    if(strcmp(pos->keyword, lower))
    {
     addKeyword(&prev, lower, EXTRACTOR_UNKNOWN);
    }
    pos = pos->next;
  }
  free(lower);

  return prev;
}

