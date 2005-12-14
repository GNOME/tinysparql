/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004 Vidyut Samanta and Christian Grothoff

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
#include <zlib.h>

static char * stndup(const char * str,
                     size_t n) {
  char * tmp;
  tmp = malloc(n+1);
  tmp[n] = '\0';
  memcpy(tmp, str, n);
  return tmp;
}

static EXTRACTOR_KeywordList * addKeyword(EXTRACTOR_KeywordType type,
					  char * keyword,
					  EXTRACTOR_KeywordList * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  if (strlen(keyword) == 0) {
    free(keyword);
    return next;
  }
  if ( (keyword[0] == '\"') && (keyword[strlen(keyword)-1] == '\"') ) {
    char * tmp;

    keyword[strlen(keyword)-1] = '\0';
    tmp = strdup(&keyword[1]);
    free(keyword);
    keyword = tmp;
  }
  if (strlen(keyword) == 0) {
    free(keyword);
    return next;
  }
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}

static void NEXT(size_t * end,
		 const char * buf,
		 const size_t size) {
  int quot;

  quot = 0;
  while ( (*end < size) &&
	  ( ((quot & 1) != 0) ||
	    ( (buf[*end] != ' ') ) ) ) {
    if (buf[*end] == '\"')
      quot++;
    (*end)++;
  }
  if ((quot & 1) == 1)
    (*end) = size+1;
}

/**
 * How many bytes do we actually try to scan? (from the beginning
 * of the file).
 */
#define MAX_READ (16 * 1024)



struct EXTRACTOR_Keywords * 
libextractor_man_extract(const char * filename,
			 const char * buf,
			 size_t size,
			 struct EXTRACTOR_Keywords * prev) {
  int pos;
  size_t xsize;
  const size_t xlen = strlen(".TH ");

  if (size > MAX_READ)
    size = MAX_READ;
  pos = 0;
  if (size < xlen)
    return prev;
  while ( (pos < size - xlen) &&
	  ( (0 != strncmp(".TH ",
			  &buf[pos],
			  xlen)) ||
	    ( (pos != 0) &&
	      (buf[pos-1] != '\n') ) ) )
    pos++;
  xsize = pos;
  while ( (xsize < size) &&
	  (buf[xsize] != '\n') )
    xsize++;
  size = xsize;

  if (0 == strncmp(".TH ",
		   &buf[pos],
		   xlen)) {
    size_t end;

    pos += xlen;
    end = pos;
    NEXT(&end, buf, size); 
    if (end > size) 
      return prev;
    if (end - pos > 0) {
      prev = addKeyword(EXTRACTOR_TITLE,
			stndup(&buf[pos],
			       end - pos),
			prev);
      pos = end + 1;
    }
    if (pos >= size) return prev;
    end = pos;
    NEXT(&end, buf, size); if (end > size) return prev;
    if (buf[pos] == '\"')
      pos++;
    if ( (end-pos >= 1) && (end - pos <= 4) ) {
      switch (buf[pos]) {
      case '1':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("Commands")),
			  prev);
	break;
      case '2':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("System calls")),
			  prev);
	break;
      case '3':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("Library calls")),
			  prev);
	break;
      case '4':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("Special files")),
			  prev);
	break;
      case '5':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("File formats and conventions")),
			  prev);
	break;
      case '6':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("Games")),
			  prev);
	break;
      case '7':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("Conventions and miscellaneous")),
			  prev);
	break;
      case '8':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("System management commands")),
			  prev);
	break;
      case '9':
	prev = addKeyword(EXTRACTOR_CATEGORY,
			  strdup(_("Kernel routines")),
			  prev);
	break;
      }
      pos = end + 1;
    }
    end = pos;
    NEXT(&end, buf, size); if (end > size) return prev;
    if (end - pos > 0) {
      prev = addKeyword(EXTRACTOR_DATE,
			stndup(&buf[pos],
			       end - pos),
			prev);
      pos = end + 1;
    }
    end = pos;
    NEXT(&end, buf, size); if (end > size) return prev;
    if (end - pos > 0) {
      prev = addKeyword(EXTRACTOR_SOURCE,
			stndup(&buf[pos],
			       end - pos),
			prev);
      pos = end + 1;
    }
    end = pos;
    NEXT(&end, buf, size); if (end > size) return prev;
    if (end - pos > 0) {
      prev = addKeyword(EXTRACTOR_BOOKTITLE,
			stndup(&buf[pos],
			       end - pos),
			prev);
      pos = end + 1;
    }
  }

  return prev;
}

/* end of manextractor.c */
