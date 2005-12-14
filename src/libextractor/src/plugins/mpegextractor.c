
/*
     This file is part of libextractor.
     (C) 2004, 2005 Vidyut Samanta and Christian Grothoff

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
#include "pack.h"
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

/* We implement our own rounding function, because the availability of
 * C99's round(), nearbyint(), rint(), etc. seems to be spotty, whereas
 * floor() is available in math.h on all C compilers.
 */
static double round_double(double num) {
   return floor(num + 0.5);
}

/* read big-endian number (most significant byte first) */
static unsigned int fread_be(unsigned char * data) {
  int x;
  unsigned int result = 0;

  for (x=3;x>=0;x--)
    result |= data[4-x] << (x*8);
  return result;
}

/* video/mpeg */
struct EXTRACTOR_Keywords * libextractor_mpeg_extract(char * filename,
						      unsigned char * xdata,
						      size_t xsize,
						      struct EXTRACTOR_Keywords * prev) {
  unsigned int version = 0;
  unsigned int bitrate = 0;
  unsigned int height = 0;
  unsigned int width = 0;
  unsigned int temp;
  unsigned int pos;
  char * format;
  int mixed = 0;

  if (xsize < 16)
    return prev;

  if ( ! ( (xdata[0]==0x00) &&
	   (xdata[1]==0x00) &&
	   (xdata[2]==0x01) &&
	   ( (xdata[3]==0xB3) || (xdata[3]==0xBA) ) ) )
    return prev;

  if (xdata[3] == 0xBA) {
    /* multiplexed audio/video */
    mixed = 1;

    if ((xdata[4] & 0xF0) == 0x20)		/* binary 0010 xxxx */
      version = 1;
    else if((xdata[4] & 0xC0) == 0x40)	/* binary 01xx xxxx */
      version = 2;
    else
      return prev; /* unsupported mpeg version */

    if (version == 1) {
      bitrate = round_double((double)((fread_be(&xdata[8]) & 0x7FFFFE) >> 1) * 0.4);
      pos = 12;
    } else {
      bitrate = round_double((double)((fread_be(&xdata[9]) & 0xFFFFFC) >> 2) * 0.4);
      temp = xdata[13] & 0x7;
      pos = 14 + temp;
    }
    if (pos + 4 >= xsize)
      return prev;
    temp = fread_be(&xdata[pos]);
    while ( (temp != 0x000001BA) && (temp != 0x000001E0) ) {
      if (temp == 0x00000000) {
	while ((temp & 0xFFFFFF00) != 0x00000100) {
	  pos++;
	  if (pos + 4 >= xsize)
	    return prev;
	  temp = fread_be(&xdata[pos]);
	}
      } else {
	if (pos + 4 >= xsize)
	  return prev;
	temp = fread_be(&xdata[pos]) & 0xFFFF;
	pos += temp + 2;
	if (pos + 4 >= xsize)
	  return prev;
	temp = fread_be(&xdata[pos]);	
      }
    }
    pos += 4;

    if (pos + 4 >= xsize)
      return prev;
    /* Now read byte by byte until we find the 0x000001B3 instead of actually
     * parsing (due to too many variations).  Theoretically this could mean
     * we find 0x000001B3 as data inside another packet, but that's extremely
     * unlikely, especially since the sequence header should not be far */
    temp = fread_be(&xdata[pos]);
    pos += 4;
    while (temp != 0x000001B3) {
      temp <<= 8;
      if (pos == xsize)
	return prev;
      temp |= xdata[pos++];
    }
  } else
    pos = 4; /* video only */

  if (pos + 16 >= xsize)
    return prev;
  width = (xdata[pos] << 4) + (xdata[pos+1] & 0xF);
  height = ((xdata[pos+1] & 0xF0) << 4) + xdata[pos+2];

  addKeyword(&prev,
	     strdup("video/mpeg"),
	     EXTRACTOR_MIMETYPE);
  format = malloc(256);
  snprintf(format,
	   256,
	   "MPEG%d (%s)",
	   version,
	   mixed ? "audio/video" : "video only");
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
  return prev;
}

