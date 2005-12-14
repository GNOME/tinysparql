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


#define M_SOI   0xD8		/* Start Of Image (beginning of datastream) */
#define M_EOI   0xD9		/* End Of Image (end of datastream) */
#define M_SOS   0xDA		/* Start Of Scan (begins compressed data) */
#define M_APP12	0xEC		
#define M_COM   0xFE		/* COMment */
#define M_APP0  0xE0

static EXTRACTOR_KeywordList * addKeyword(EXTRACTOR_KeywordType type,
					  char * keyword,
					  EXTRACTOR_KeywordList * next) {
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
 * Get the next character in the sequence and advance
 * the pointer *data to the next location in the sequence.
 * If we're at the end, return -1.
 */
#define NEXTC(data,end) ((*(data)<(end))?*((*(data))++):-1)

/* The macro does:
unsigned int NEXTC(unsigned char ** data, char *  end) {
  if (*data < end) {
    char result = **data;
    (*data)++;
    return result;
  } else
    return -1;
}
*/

/**
 * Read length, convert to unsigned int.
 * All 2-byte quantities in JPEG markers are MSB first
 * @return -1 on error
 */
static int readLength(unsigned char ** data,
		      unsigned char * end) {
  int c1;
  int c2;

  c1 = NEXTC(data, end);
  if (c1 == -1)
    return -1;
  c2 = NEXTC(data, end);
  if (c2 == -1)
    return -1;
  return ((((unsigned int) c1) << 8) + ((unsigned int) c2))-2;
}

/**
 * @return the next marker or -1 on error.
 */
static int next_marker(unsigned char ** data,
		       unsigned char * end) {
  int c;
  c = NEXTC(data, end);
  while ( (c != 0xFF) && (c != -1) )
    c = NEXTC(data, end);
  do {
    c = NEXTC(data, end);
  } while ( (c == 0xFF) && (c != -1) );
  return c;
}

static void skip_variable(unsigned char ** data,
			  unsigned char * end) {
  int length;

  length = readLength(data, end);
  if (length < 0) {
    (*data) = end; /* skip to the end */
    return;
  }
  /* Skip over length bytes */
  (*data) += length;
}

static char * process_COM(unsigned char ** data,
			  unsigned char * end) {
  unsigned int length;
  int ch;
  int pos;
  char * comment;

  length = readLength(data, end);
  if (length <= 0)
    return NULL;
  comment = malloc(length+1);
  pos = 0;
  while (length > 0) {
    ch = NEXTC(data, end);
    if ( (ch == '\r')  ||
	 (ch == '\n') )
      comment[pos++] = '\n';
    else if (isprint(ch))
      comment[pos++] = ch;
    length--;
  }
  comment[pos] = '\0';
  return comment;
}

struct EXTRACTOR_Keywords * libextractor_jpeg_extract(const char * filename,
                                                      unsigned char * data,
                                                      size_t size,
                                                      struct EXTRACTOR_Keywords * prev) {
  int c1;
  int c2;
  int marker;
  unsigned char * end;
  struct EXTRACTOR_Keywords * result;

  if (size < 0x12)
    return prev;
  result = prev;
  end = &data[size];
  c1 = NEXTC(&data, end);
  c2 = NEXTC(&data, end);
  if ( (c1 != 0xFF) || (c2 != M_SOI) )
    return result; /* not a JPEG */
  result = addKeyword(EXTRACTOR_MIMETYPE,
		      strdup("image/jpeg"),
		      result);
  while(1) {
    marker = next_marker(&data, end);
    switch (marker) {
    case -1: /* end of file */
    case M_SOS:
    case M_EOI:
      goto RETURN; /* this used to be "return result", but this
		      makes certain compilers unhappy...*/
    case M_APP0: {
      int len = readLength(&data, end);
      if (len < 0x8)
	goto RETURN;
      if (0 == strncmp((char*)data,
		       "JFIF",
		       4)) {
	char * val;

	switch (data[0x4]) {
	case 1: /* dots per inch */
	  val = malloc(128);
	  snprintf(val, 128,
		   _("%ux%u dots per inch"),
		   (data[0x8] << 8) + data[0x9],
		   (data[0xA] << 8) + data[0xB]);
	  result = addKeyword(EXTRACTOR_RESOLUTION,
			      val,
			      result);
	  break;
	case 2: /* dots per cm */
	  val = malloc(128);
	  snprintf(val, 128,
		   _("%ux%u dots per cm"),
		   (data[0x8] << 8) + data[0x9],
		   (data[0xA] << 8) + data[0xB]);
	  result = addKeyword(EXTRACTOR_RESOLUTION,
			      val,
			      result);
	  break;
	case 0: /* no unit given */
	  val = malloc(128);
	  snprintf(val, 128,
		   _("%ux%u dots per inch?"),
		   (data[0x8] << 8) + data[0x9],
		   (data[0xA] << 8) + data[0xB]);
	  result = addKeyword(EXTRACTOR_RESOLUTION,
			      val,
			      result);
	  break;
	default: /* unknown unit */
	  break;
	}
      }
      data = &data[len];
      break;
    }
    case 0xC0: {
      char * val;
      int len = readLength(&data, end);
      if (len < 0x9)
	goto RETURN;
      val = malloc(128);
      snprintf(val, 128,
	       "%ux%u",
	       (data[0x3] << 8) + data[0x4],
	       (data[0x1] << 8) + data[0x2]);
      result = addKeyword(EXTRACTOR_SIZE,
			  val,
			  result);
      data = &data[len];
      break;
    }
    case M_COM:
    case M_APP12:
      result = addKeyword(EXTRACTOR_COMMENT,
			  process_COM(&data, end),
			  result);
      break;
    default:			
      skip_variable(&data, end);		
      break;
    }
  }
 RETURN:
  return result;
}

