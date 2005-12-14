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
#include "pack.h"

#define DEBUG_GIF 0
#if DEBUG_GIF
#define PRINT(a,b) fprintf(stderr,a,b)
#else
#define PRINT(a,b)
#endif

typedef struct {
  char gif[3];
  char version[3];
  unsigned short screen_width;
  unsigned short screen_height;
  unsigned char flags;
#define HEADER_FLAGS__SIZE_OF_GLOBAL_COLOR_TABLE 0x07
#define HEADER_FLAGS__SORT_FLAG 0x08
#define HEADER_FLAGS__COLOR_RESOLUTION 0x70
#define HEADER_FLAGS__GLOBAL_COLOR_TABLE_FLAG 0x80
  unsigned char background_color_index;
  unsigned char pixel_aspect_ratio;
} GIF_HEADER;

#define GIF_HEADER_SIZE 13
#define GIF_HEADER_SPEC "3b3bhhbbb"
#define GIF_HEADER_FIELDS(p) \
 &(p)->gif,\
 &(p)->version, \
 &(p)->screen_width, \
 &(p)->screen_height, \
 &(p)->flags, \
 &(p)->background_color_index, \
 &(p)->pixel_aspect_ratio

typedef struct {
  unsigned char image_separator;
  unsigned short image_left;
  unsigned short image_top;
  unsigned short image_width;
  unsigned short image_height;
  unsigned char flags;
#define DESCRIPTOR_FLAGS__PIXEL_SIZE 0x07
#define DESCRIPTOR_FLAGS__RESERVED 0x18
#define DESCRIPTOR_FLAGS__SORT_FLAG 0x20
#define DESCRIPTOR_FLAGS__INTERLACE_FLAG 0x40
#define DESCRIPTOR_FLAGS__LOCAL_COLOR_TABLE_FLAG 0x80
} GIF_DESCRIPTOR;
#define GIF_DESCRIPTOR_SIZE 10
#define GIF_DESCRIPTOR_SPEC "chhhhc"
#define GIF_DESCRIPTOR_FIELDS(p) \
 &(p)->image_separator, \
 &(p)->image_left, \
 &(p)->image_top, \
 &(p)->image_width, \
 &(p)->image_height, \
 &(p)->flags

typedef struct {
  unsigned char extension_introducer;
  unsigned char graphic_control_label;
} GIF_EXTENSION;

static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordType type,
					      char * keyword,
					      struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}

/**
 * Skip a data block.
 * @return the position after the block
 **/
static size_t skipDataBlock(const unsigned char * data,
			    size_t pos,
			    const size_t size) {
  while ( (pos < size) &&
	  (data[pos] != 0) )
    pos += data[pos]+1;
  return pos+1;
}

/**
 * skip an extention block
 * @return the position after the block
 **/
static size_t skipExtensionBlock(const unsigned char * data,
				 size_t pos,
				 const size_t size,
				 const GIF_EXTENSION * ext) {
  return skipDataBlock(data, pos+sizeof(GIF_EXTENSION), size);
}

/**
 * @return the offset after the global color map
 **/
static size_t skipGlobalColorMap(const unsigned char * data,
				 const size_t size,
				 const GIF_HEADER * header) {
  size_t gct_size;

  if ( (header->flags &  HEADER_FLAGS__GLOBAL_COLOR_TABLE_FLAG) > 0)
    gct_size = 3*(1 << ((header->flags & HEADER_FLAGS__SIZE_OF_GLOBAL_COLOR_TABLE)+1));
  else
    gct_size = 0;
  return GIF_HEADER_SIZE + gct_size;
}

/**
 * @return the offset after the local color map
 **/
static size_t skipLocalColorMap(const unsigned char * data,
				size_t pos,
				const size_t size,
				GIF_DESCRIPTOR * descriptor) {
  size_t lct_size;

  if (pos+GIF_DESCRIPTOR_SIZE > size)
    return size;
  if ( (descriptor->flags & DESCRIPTOR_FLAGS__LOCAL_COLOR_TABLE_FLAG) > 0)
    lct_size = 3*(1 << ((descriptor->flags & DESCRIPTOR_FLAGS__PIXEL_SIZE)+1));
  else
    lct_size = 0;
  return pos + GIF_DESCRIPTOR_SIZE + lct_size;
}

static struct EXTRACTOR_Keywords * parseComment(const unsigned char * data,
						size_t pos,
						const size_t size,
						struct EXTRACTOR_Keywords * prev) {
  size_t length = 0;
  size_t curr = pos;
  char * keyword;

  while ( (data[curr] != 0) &&
	  (curr < size) ) {
    length += data[curr];
    curr += data[curr] + 1;
  }
  keyword = malloc(length+1);
  curr = pos;
  length = 0;
  while ( (data[curr] != 0) &&
	  (curr < size) ) {
    length += data[curr];
    if (length >= size)
      break;
    memcpy(&keyword[length-data[curr]],
	   &data[curr]+1,
	   data[curr]);
    keyword[length] = 0;
    curr += data[curr] + 1;
  }
  return addKeyword(EXTRACTOR_COMMENT,
		    keyword,
		    prev);
}
				

struct EXTRACTOR_Keywords * libextractor_gif_extract(const char * filename,
                                                     const unsigned char * data,
                                                     const size_t size,
                                                     struct EXTRACTOR_Keywords * prev) {
  size_t pos;
  struct EXTRACTOR_Keywords * result;
  GIF_HEADER header;
  char * tmp;

  if (size < GIF_HEADER_SIZE)
    return prev;
  cat_unpack(data,
	     GIF_HEADER_SPEC,
	     GIF_HEADER_FIELDS(&header));
  if (0 != strncmp(&header.gif[0],"GIF",3))
    return prev;
  if (0 != strncmp(&header.version[0], "89a",3))
    return prev; /* only 89a has support for comments */
  result = prev;
  result = addKeyword(EXTRACTOR_MIMETYPE,
		      strdup("image/gif"),
		      result);
  tmp = malloc(128);
  snprintf(tmp,
	   128,
	   "%ux%u",
	   header.screen_width,
	   header.screen_height);
  result = addKeyword(EXTRACTOR_SIZE,
		      strdup(tmp),
		      result);
  free(tmp);
  pos = skipGlobalColorMap(data, size, &header);
  PRINT("global color map ends at %d\n",pos);
  while (pos < size) {
    GIF_DESCRIPTOR gd;

    switch (data[pos]) {
    case ',': /* image descriptor block */
      PRINT("skipping local color map %d\n", pos);
      cat_unpack(&data[pos],
		 GIF_DESCRIPTOR_SPEC,
		 GIF_DESCRIPTOR_FIELDS(&gd));
      pos = skipLocalColorMap(data, pos, size,
			      &gd);
      break;
    case '!': /* extension block */
      PRINT("skipping extension block %d\n",pos);
      if (data[pos+1] == (unsigned char)0xFE) {
	result = parseComment(data,
			      pos+2,
			      size,
			      result);
      }
      pos = skipExtensionBlock(data, pos, size,
			       (GIF_EXTENSION *) &data[pos]);
      break;
    case ';':
      PRINT("hit terminator at %d!\n",pos);
      return result; /* terminator! */
    default: /* raster data block */
      PRINT("skipping data block at %d\n",pos);
      pos = skipDataBlock(data, pos+1, size);
      break;
    }
  }
  PRINT("returning at %d\n",pos);
  return result;
}

