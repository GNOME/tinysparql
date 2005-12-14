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

#define DEBUG_EXTRACT_ID3v23 0

#include "platform.h"
#include "extractor.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#ifndef MINGW
  #include <sys/mman.h>
#endif

#include "convert.h"

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
  { "COMM", EXTRACTOR_COMMENT },
  { "TCOP", EXTRACTOR_COPYRIGHT },
  { "TDAT", EXTRACTOR_DATE },
  { "TCON", EXTRACTOR_GENRE },
  { "TIT1", EXTRACTOR_GENRE },
  { "TENC", EXTRACTOR_PRODUCER },
  { "TEXT", EXTRACTOR_LYRICS },
  { "TOLY", EXTRACTOR_CONTRIBUTOR },
  { "TOPE", EXTRACTOR_CONTRIBUTOR },
  { "TOWN", EXTRACTOR_OWNER },
  { "TPE1", EXTRACTOR_ARTIST },
  { "TPE2", EXTRACTOR_ARTIST },
  { "TPE3", EXTRACTOR_CONDUCTOR },
  { "TPE4", EXTRACTOR_INTERPRET },
  { "TMED", EXTRACTOR_MEDIA_TYPE },
  { "TCOM", EXTRACTOR_CREATOR },
  { "TIME", EXTRACTOR_DATE },
  { "TOFN", EXTRACTOR_FILENAME },
  { "TOPE", EXTRACTOR_ARTIST },
  { "TPUB", EXTRACTOR_PUBLISHER },
  { "TRSN", EXTRACTOR_SOURCE },
  { "TRSO", EXTRACTOR_CREATED_FOR },
  { "TSRC", EXTRACTOR_RESOURCE_IDENTIFIER },
  { "TOAL", EXTRACTOR_ALBUM },
  { "TALB", EXTRACTOR_ALBUM },
  { "TLAN", EXTRACTOR_LANGUAGE },
  { "TIT2", EXTRACTOR_TITLE },
  { "TIT3", EXTRACTOR_DESCRIPTION },
  { "WCOM", EXTRACTOR_RELEASE },
  { "WCOP", EXTRACTOR_DISCLAIMER },
  { "", EXTRACTOR_KEYWORDS },
  { NULL, 0},
};


/* mimetype = audio/mpeg */
struct EXTRACTOR_Keywords *
libextractor_id3v23_extract(const char * filename,
			    const unsigned char * data,
			    const size_t size,
			    struct EXTRACTOR_Keywords * prev) {
  int unsync;
  int extendedHdr;
  int experimental;
  unsigned int tsize;
  unsigned int pos;
  unsigned int ehdrSize;
  unsigned int padding;

  if ( (size < 16) ||
       (data[0] != 0x49) ||
       (data[1] != 0x44) ||
       (data[2] != 0x33) ||
       (data[3] != 0x03) ||
       (data[4] != 0x00) )
    return prev;
  unsync = (data[5] & 0x80) > 0;
  extendedHdr = (data[5] & 0x40) > 0;
  experimental = (data[5] & 0x20) > 0;
  tsize = ( ( (data[6] & 0x7F) << 21 ) |
	    ( (data[7] & 0x7F) << 14 ) |
	    ( (data[8] & 0x7F) << 7 ) |
	    ( (data[9] & 0x7F) << 0 ) );
  if ( (tsize + 10 > size) || (experimental) )
    return prev;
  pos = 10;
  padding = 0;
  if (extendedHdr) {
    ehdrSize = ( ( (data[10]) << 24 ) |
		 ( (data[11]) << 16 ) |
		 ( (data[12]) << 8 ) |
		 ( (data[12]) << 0 ) );

    padding  = ( ( (data[15]) << 24 ) |
		 ( (data[16]) << 16 ) |
		 ( (data[17]) << 8 ) |
		 ( (data[18]) << 0 ) );
    pos += 4 + ehdrSize;
    if (padding < tsize)
      tsize -= padding;
    else
      return prev;
  }


  while (pos < tsize) {
    size_t csize;
    int i;
    unsigned short flags;

    if (pos + 10 > tsize)
      return prev;
    csize = (data[pos+4] << 24) + (data[pos+5] << 16) + (data[pos+6] << 8) + data[pos+7];
    if ( (pos + 10 + csize > tsize) ||
	 (csize > tsize) ||
	 (csize == 0) )
      break;
    flags = (data[pos+8]<<8) + data[pos+9];
    if ( ( (flags & 0x80) > 0) /* compressed, not yet supported */ ||
	 ( (flags & 0x40) > 0) /* encrypted, not supported */ ) {
      pos += 10 + csize;
      continue;
    }
    i = 0;
    while (tmap[i].text != NULL) {
      if (0 == strncmp(tmap[i].text,
		       (const char*) &data[pos],
		       4)) {
	char * word;
	if ( (flags & 0x20) > 0) {
	  /* "group" identifier, skip a byte */
	  pos++;
	  csize--;
	}
	csize--;
	/* this byte describes the encoding
	   try to convert strings to UTF-8
	   if it fails, then forget it */
	switch (data[pos+10]) {
	case 0x00 :
	  word = convertToUtf8((const char*) &data[pos+11],
			       csize,
			       "ISO-8859-1");
	  break;
	case 0x01 :
	  word = convertToUtf8((const char*) &data[pos+11],
			       csize,
			       "UCS-2");
	  break;
	default:
	  /* bad encoding byte,
	     try to convert from iso-8859-1 */
	  word = convertToUtf8((const char*) &data[pos+11],
			       csize,
			       "ISO-8859-1");
	  break;
	}
	pos++;
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
    pos += 10 + csize;
  }
  return prev;
}

/* end of id3v23extractor.c */
