/*
     This file is part of libextractor.
     (C) 2004 Vidyut Samanta and Christian Grothoff

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

     This code was based on AVInfo 1.0 alpha 11
     (c) George Shuklin, gs]AT[shounen.ru, 2002-2004
     http://shounen.ru/soft/avinfo/

     and bitcollider 0.6.0
     (PD) 2004 The Bitzi Corporation
     http://bitzi.com/
 */

#include "platform.h"
#include "extractor.h"
#include <math.h>

static void addKeyword(struct EXTRACTOR_Keywords ** list,
		       char * keyword,
		       EXTRACTOR_KeywordType type) {
  EXTRACTOR_KeywordList * next;
  next = malloc(sizeof(EXTRACTOR_KeywordList));
  next->next = *list;
  next->keyword = keyword;
  next->keywordType = type;
  *list = next;
}


#ifdef FIXME
static struct EXTRACTOR_Keywords * riffparse_INFO(char * buffer,
						  size_t size,
						  struct EXTRACTOR_Keywords * prev) {
  size_t c = 0;
  char * word;

  if (size < 64)
    return prev;
  c = 8;
  while ( (c < size) && isprint(buffer[c]) )
    c++;
  if (c > 8) {
    word = malloc(c+1-8);
    memcpy(word,
	   &buffer[8],
	   c);
    word[c-8] = '\0';
    addKeyword(&prev,
	       strdup(buffer+c),
	       EXTRACTOR_UNKNOWN); /* eh, what exactly is it */
  }
  return prev;
}
#endif


/**
 * Read the specified number of bytes as a little-endian (least
 * significant byte first) integer.
 */
static unsigned int fread_le(char * data) {
  int x;
  unsigned int result = 0;

  for(x=0;x<4; x++)
    result |= ((unsigned char)data[x]) << (x*8);
  return result;
}

/* We implement our own rounding function, because the availability of
 * C99's round(), nearbyint(), rint(), etc. seems to be spotty, whereas
 * floor() is available in math.h on all C compilers.
 */
static double round_double(double num) {
   return floor(num + 0.5);
}

/* video/x-msvideo */
struct EXTRACTOR_Keywords * libextractor_riff_extract(char * filename,
						      char * xdata,
						      size_t xsize,
						      struct EXTRACTOR_Keywords * prev) {
  unsigned int blockLen;
  unsigned int fps;
  unsigned int duration;
  size_t pos;
  unsigned int width;
  unsigned int height;
  char codec[5];
  char * format;

  if (xsize < 32)
    return prev;

  if ( (memcmp(&xdata[0],
	       "RIFF", 4) !=0) ||
       (memcmp(&xdata[8],
	       "AVI ",
	       4) !=0) )
    return prev;

  if (memcmp(&xdata[12],
	     "LIST",
	     4) != 0)
    return prev;
  if (memcmp(&xdata[20],
	     "hdrlavih",
	     8) != 0)
    return prev;


  blockLen = fread_le(&xdata[28]);

  /* begin of AVI header at 32 */
  fps = (unsigned int) round_double((double) 1.0e6 / fread_le(&xdata[32]));
  duration = (unsigned int) round_double((double) fread_le(&xdata[48])
					 * 1000 / fps);
  width = fread_le(&xdata[64]);
  height = fread_le(&xdata[68]);


  /* pos: begin of video stream header */
  pos = blockLen + 32;

  if ( (pos < blockLen) ||
       (pos + 32 > xsize) ||
       (pos > xsize) )
    return prev;

  if (memcmp(&xdata[pos],
	     "LIST",
	     4) != 0)
    return prev;
  blockLen = fread_le(&xdata[pos+4]);
  if (memcmp(&xdata[pos+8],
	     "strlstrh",
	     8) != 0)
    return prev;
  if (memcmp(&xdata[pos+20],
	     "vids",
	     4) != 0)
    return prev;
  /* pos + 24: video stream header */
  memcpy(codec,
	 &xdata[pos+24],
	 4);
  codec[4] = '\0';

  format = malloc(256);
  snprintf(format,
	   256,
	   _("codec: %s, %u fps, %u ms"),
	   codec,
	   fps,
	   duration);
  addKeyword(&prev,
	     format,
	     EXTRACTOR_FORMAT);
  format = malloc(256);
  snprintf(format,
	   256,
	   "%ux%u",
	   width,
	   height);
  addKeyword(&prev,
	     format,
	     EXTRACTOR_SIZE);
  addKeyword(&prev,
	     strdup("video/x-msvideo"),
	     EXTRACTOR_MIMETYPE);
  return prev;
}

