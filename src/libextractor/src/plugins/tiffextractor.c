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
 */

#include "platform.h"
#include "extractor.h"
#include "pack.h"

#define DEBUG 0

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

typedef struct {
  unsigned short byteorder;
  unsigned short fourty_two;
  unsigned int ifd_offset;
} TIFF_HEADER;
#define TIFF_HEADER_SIZE 8
#define TIFF_HEADER_FIELDS(p) \
  &(p)->byteorder,	      \
    &(p)->fourty_two,	      \
    &(p)->ifd_offset
static char * TIFF_HEADER_SPECS[] = {
  "hhw",
  "HHW",
};

typedef struct {
  unsigned short tag;
  unsigned short type;
  unsigned int count;
  unsigned int value_or_offset;
} DIRECTORY_ENTRY;
#define DIRECTORY_ENTRY_SIZE 12
#define DIRECTORY_ENTRY_FIELDS(p)		\
  &(p)->tag,					\
    &(p)->type,					\
    &(p)->count,				\
    &(p)->value_or_offset
static char * DIRECTORY_ENTRY_SPECS[] = {
  "hhww",
  "HHWW"
};

#define TAG_LENGTH 0x101
#define TAG_WIDTH 0x100
#define TAG_SOFTWARE 0x131
#define TAG_DAYTIME 0x132
#define TAG_ARTIST 0x315
#define TAG_COPYRIGHT 0x8298
#define TAG_DESCRIPTION 0x10E
#define TAG_DOCUMENT_NAME 0x10D
#define TAG_HOST 0x13C
#define TAG_SCANNER 0x110
#define TAG_ORIENTATION 0x112

#define TYPE_BYTE 1
#define TYPE_ASCII 2
#define TYPE_SHORT 3
#define TYPE_LONG 4
#define TYPE_RATIONAL 5

static void addASCII(struct EXTRACTOR_Keywords ** prev,
		     char * data,
		     size_t size,
		     DIRECTORY_ENTRY * entry,
		     EXTRACTOR_KeywordType type) {
  if (entry->count > size)
    return; /* invalid! */
  if (entry->type != TYPE_ASCII)
    return; /* huh? */
  if (entry->count+entry->value_or_offset > size)
    return;
  if (data[entry->value_or_offset+entry->count-1] != 0)
    return;
  addKeyword(prev,
	     strdup(&data[entry->value_or_offset]),
	     EXTRACTOR_SOFTWARE);
}


struct EXTRACTOR_Keywords * libextractor_tiff_extract(char * filename,
						      char * data,
						      size_t size,
						      struct EXTRACTOR_Keywords * prev) {
  TIFF_HEADER hdr;
  int byteOrder; /* 0: do not convert;
		    1: do convert */
  int current_ifd;
  long long length = -1;
  long long width = -1;

  if (size < TIFF_HEADER_SIZE)
    return prev; /*  can not be tiff */
  if ( (data[0] == 0x49) &&
       (data[1] == 0x49) )
    byteOrder = 0;
  else if ( (data[0] == 0x4D) &&
	    (data[1] == 0x4D) )
    byteOrder = 1;
  else
    return prev; /* can not be tiff */
#if __BYTE_ORDER == __BIG_ENDIAN
  byteOrder = 1-byteOrder;
#endif
  cat_unpack(data,
	     TIFF_HEADER_SPECS[byteOrder],
	     TIFF_HEADER_FIELDS(&hdr));
  if (hdr.fourty_two != 42)
    return prev; /* can not be tiff */
  if (hdr.ifd_offset + 6 > size)
    return prev; /* malformed tiff */
  addKeyword(&prev,
	     strdup("image/tiff"),
	     EXTRACTOR_MIMETYPE);
  current_ifd = hdr.ifd_offset;
  while (current_ifd != 0) {
    unsigned short len;
    unsigned int off;
    int i;
    if (current_ifd + 6 > size)
      return prev;
    if (byteOrder == 0)
      len = data[current_ifd+1] << 8 | data[current_ifd];
    else
      len = data[current_ifd] << 8 | data[current_ifd+1];
    if (len * DIRECTORY_ENTRY_SIZE + 2 + 4 > size) {
#if DEBUG
      printf("WARNING: malformed tiff\n");
#endif
      return prev;
    }
    for (i=0;i<len;i++) {
      DIRECTORY_ENTRY entry;
      off = current_ifd + 2 + DIRECTORY_ENTRY_SIZE*i;

      cat_unpack(&data[off],
		 DIRECTORY_ENTRY_SPECS[byteOrder],
		 DIRECTORY_ENTRY_FIELDS(&entry));
      switch (entry.tag) {
      case TAG_LENGTH:
	if ( (entry.type == TYPE_SHORT) &&
	     (byteOrder == 1) ) {
	  length = entry.value_or_offset >> 16;
	} else {
	  length = entry.value_or_offset;
	}
	if (width != -1) {
	  char * tmp;
	  tmp = malloc(128);
	  sprintf(tmp, "%ux%u",
		  (unsigned int) width,
		  (unsigned int) length);
	  addKeyword(&prev,
		     strdup(tmp),
		     EXTRACTOR_SIZE);
	  free(tmp);
	}
	break;
      case TAG_WIDTH:
	if ( (entry.type == TYPE_SHORT) &&
	     (byteOrder == 1) )
	  width = entry.value_or_offset >> 16;
	else
	  width = entry.value_or_offset;
	if (length != -1) {
	  char * tmp;
	  tmp = malloc(128);
	  sprintf(tmp, "%ux%u",
		  (unsigned int) width,
		  (unsigned int) length);
	  addKeyword(&prev,
		     strdup(tmp),
		     EXTRACTOR_SIZE);
	  free(tmp);
	}
	break;
      case TAG_SOFTWARE:
	addASCII(&prev,
		 data, size,
		 &entry,
		 EXTRACTOR_SOFTWARE);
	break;
      case TAG_ARTIST:
	addASCII(&prev,
		 data, size,
		 &entry,
		 EXTRACTOR_ARTIST);
	break;
      case TAG_DOCUMENT_NAME:
	addASCII(&prev,
		 data, size,
		 &entry,
		 EXTRACTOR_TITLE);
	break;
      case TAG_COPYRIGHT:
	addASCII(&prev,
		 data, size,
		 &entry,
		 EXTRACTOR_COPYRIGHT);
	break;
      case TAG_DESCRIPTION:
	addASCII(&prev,
		 data, size,
		 &entry,
		 EXTRACTOR_DESCRIPTION);
	break;
      case TAG_HOST:
	addASCII(&prev,
		 data, size,
		 &entry,
		 EXTRACTOR_BUILDHOST);
	break;
      case TAG_SCANNER:
	addASCII(&prev,
		 data, size,
		 &entry,
		 EXTRACTOR_SOURCE);
	break;
      case TAG_DAYTIME:
	addASCII(&prev,
		 data, size,
		 &entry,
		 EXTRACTOR_CREATION_DATE);
	break;
      }
    }

    off = current_ifd + 2 + DIRECTORY_ENTRY_SIZE * len;
    if (byteOrder == 0)
      current_ifd = data[off+3]<<24|data[off+2]<<16|data[off+1]<<8|data[off];
    else
      current_ifd = data[off]<<24|data[off+1]<<16|data[off+2]<<8|data[off+3];
  }
  return prev;
}

