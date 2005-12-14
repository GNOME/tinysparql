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
#ifndef MINGW
  #include <sys/mman.h>
#endif
#include "convert.h"

#define DEBUG_EXTRACT_ID3v2 0


static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordList *oldhead,
					      char *phrase,
					      EXTRACTOR_KeywordType type) {
  EXTRACTOR_KeywordList * keyword;

  keyword = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
  keyword->next = oldhead;
  keyword->keyword = phrase;
  keyword->keywordType = type;
  return keyword;
}

typedef struct {
  char * text;
  EXTRACTOR_KeywordType type;
} Matches;

static Matches tmap[] = {
  { "TAL", EXTRACTOR_TITLE },
  { "TT1", EXTRACTOR_GROUP },
  { "TT2", EXTRACTOR_TITLE },
  { "TT3", EXTRACTOR_TITLE },
  { "TXT", EXTRACTOR_DESCRIPTION },
  { "TPB", EXTRACTOR_PUBLISHER },
  { "WAF", EXTRACTOR_LOCATION },
  { "WAR", EXTRACTOR_LOCATION },
  { "WAS", EXTRACTOR_LOCATION },
  { "WCP", EXTRACTOR_COPYRIGHT },
  { "WAF", EXTRACTOR_LOCATION },
  { "WCM", EXTRACTOR_DISCLAIMER },
  { "TSS", EXTRACTOR_FORMAT },
  { "TYE", EXTRACTOR_DATE },
  { "TLA", EXTRACTOR_LANGUAGE },
  { "TP1", EXTRACTOR_ARTIST },
  { "TP2", EXTRACTOR_ARTIST },
  { "TP3", EXTRACTOR_CONDUCTOR },
  { "TP4", EXTRACTOR_INTERPRET },
  { "IPL", EXTRACTOR_CONTRIBUTOR },
  { "TOF", EXTRACTOR_FILENAME },
  { "TEN", EXTRACTOR_PRODUCER },
  { "TCO", EXTRACTOR_SUBJECT },
  { "TCR", EXTRACTOR_COPYRIGHT },
  { "SLT", EXTRACTOR_LYRICS },
  { "TOA", EXTRACTOR_ARTIST },
  { "TRC", EXTRACTOR_RESOURCE_IDENTIFIER },
  { "TCM", EXTRACTOR_CREATOR },
  { "TOT", EXTRACTOR_ALBUM },
  { "TOL", EXTRACTOR_AUTHOR },
  { "COM", EXTRACTOR_COMMENT },
  { "", EXTRACTOR_KEYWORDS },
  { NULL, 0},
};


/* mimetype = audio/mpeg */
struct EXTRACTOR_Keywords * 
libextractor_id3v2_extract(const char * filename,
			   const unsigned char * data,
			   size_t size,
			   struct EXTRACTOR_Keywords * prev) {
  int unsync;
  unsigned int tsize;
  unsigned int pos;

  if ( (size < 16) ||
       (data[0] != 0x49) ||
       (data[1] != 0x44) ||
       (data[2] != 0x33) ||
       (data[3] != 0x02) ||
       (data[4] != 0x00) )
    return prev;
  unsync = (data[5] & 0x80) > 0;
  tsize = ( ( (data[6] & 0x7F) << 21 ) |
	    ( (data[7] & 0x7F) << 14 ) |
	    ( (data[8] & 0x7F) << 07 ) |
	    ( (data[9] & 0x7F) << 00 ) );

  if (tsize + 10 > size)
    return prev;
  pos = 10;
  while (pos < tsize) {
    size_t csize;
    int i;

    if (pos + 6 > tsize)
      return prev;
    csize = (data[pos+3] << 16) + (data[pos+4] << 8) + data[pos+5];
    if ( (pos + 6 + csize > tsize) ||
	 (csize > tsize) ||
	 (csize == 0) )
      break;
    i = 0;
    while (tmap[i].text != NULL) {
      if (0 == strncmp(tmap[i].text,
		       (const char*) &data[pos],
		       3)) {
	char * word;
	/* this byte describes the encoding
	   try to convert strings to UTF-8
	   if it fails, then forget it */
	switch (data[pos+6]) {
	case 0x00:
	  word = convertToUtf8((const char*) &data[pos+7],
			       csize,
			       "ISO-8859-1");
	  break;
	case 0x01:
	  word = convertToUtf8((const char*) &data[pos+7],
			       csize,
			       "UCS-2");
	  break;
	default:
	  /* bad encoding byte,
	     try to convert from iso-8859-1 */
	  word = convertToUtf8((const char*) &data[pos+7],
			       csize,
			       "ISO-8859-1");
	  break;
	}
	pos++;
	csize--;
	if ( (word != NULL) &&
	     (strlen(word) > 0) ) {
	  prev = addKeyword(prev,
			    word,
			    tmap[i].type);	
	} else {
	  free(word);
	}
	break;
      }
      i++;
    }
    pos += 6 + csize;
  }
  return prev;
}

/* end of id3v2extractor.c */
