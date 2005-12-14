/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005 Vidyut Samanta and Christian Grothoff

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
#include "convert.h"

static char * stndup(const char * str,
		     size_t n) {
  char * tmp;
  tmp = malloc(n+1);
  tmp[n] = '\0';
  memcpy(tmp, str, n);
  return tmp;
}

/**
 * strnlen is GNU specific, let's redo it here to be
 * POSIX compliant.
 */
static size_t stnlen(const char * str,
		     size_t maxlen) {
  size_t ret;
  ret = 0;
  while ( (ret < maxlen) &&
	  (str[ret] != '\0') )
    ret++;
  return ret;
}

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

static int getIntAt(const void * pos) {
  char p[4];

  memcpy(p, pos, 4); /* ensure alignment! */
  return *(int*)&p[0];
}


static struct {
  char * name;
  EXTRACTOR_KeywordType type;
} tagmap[] = {
   { "Author" , EXTRACTOR_AUTHOR},
   { "Description" , EXTRACTOR_DESCRIPTION},
   { "Comment", EXTRACTOR_COMMENT},
   { "Copyright", EXTRACTOR_COPYRIGHT},
   { "Source", EXTRACTOR_SOURCE},
   { "Creation Time", EXTRACTOR_DATE},
   { "Title", EXTRACTOR_TITLE},
   { "Software", EXTRACTOR_SOFTWARE},
   { "Disclaimer", EXTRACTOR_DISCLAIMER},
   { "Warning", EXTRACTOR_WARNING},
   { "Signature", EXTRACTOR_RESOURCE_IDENTIFIER},
   { NULL, EXTRACTOR_UNKNOWN},
};

static struct EXTRACTOR_Keywords * processtEXt(const char * data,
					       unsigned int length,
					       struct EXTRACTOR_Keywords * prev) {
  char * keyword;
  unsigned int off;
  int i;

  data += 4;
  off = stnlen(data, length) + 1;
  if (off >= length)
    return prev; /* failed to find '\0' */
  keyword = convertToUtf8(&data[off],
			  length-off,
			  "ISO-8859-1");
  i = 0;
  while (tagmap[i].name != NULL) {
    if (0 == strcmp(tagmap[i].name, data))
      return addKeyword(tagmap[i].type,
			keyword,
			prev);

    i++;
  }
  return addKeyword(EXTRACTOR_UNKNOWN,
		    keyword,
		    prev);
}

static struct EXTRACTOR_Keywords * processiTXt(const char * data,
					       unsigned int length,
					       struct EXTRACTOR_Keywords * prev) {
  unsigned int pos;
  char * keyword;
  const char * language;
  const char * translated;
  int i;
  int compressed;
  char * buf;
  uLongf bufLen;
  int ret;

  pos = stnlen(data, length)+1;
  if (pos+3 >= length)
    return prev;
  compressed = data[pos++];
  if (compressed && (data[pos++] != 0))
    return prev; /* bad compression method */
  language = &data[pos];
  if (stnlen(language, length-pos) > 0)
    prev = addKeyword(EXTRACTOR_LANGUAGE,
		      stndup(language, length-pos),
		      prev);
  pos += stnlen(language, length-pos)+1;
  if (pos+1 >= length)
    return prev;
  translated = &data[pos]; /* already in utf-8! */
  if (stnlen(translated, length-pos) > 0)
    prev = addKeyword(EXTRACTOR_TRANSLATED,
		      stndup(translated, length-pos),
		      prev);
  pos += stnlen(translated, length-pos)+1;
  if (pos >= length)
    return prev;

  if (compressed) {
    bufLen = 1024 + 2 * (length - pos);
    while (1) {
      if (bufLen * 2 < bufLen)
	return prev;
      bufLen *= 2;
      if (bufLen > 50 * (length - pos)) {
	/* printf("zlib problem"); */
	return prev;
      }
      buf = malloc(bufLen);
      if (buf == NULL) {
	/* printf("out of memory"); */
	return prev; /* out of memory */
      }
      ret = uncompress((Bytef*) buf,
		       &bufLen,
		       (const Bytef*) &data[pos],
		       length - pos);
      if (ret == Z_OK) {
	/* printf("zlib ok"); */
	break;
      }
      free(buf);
      if (ret != Z_BUF_ERROR)
	return prev; /* unknown error, abort */
    }
    keyword = stndup(buf, bufLen);
    free(buf);
  } else {
    keyword = stndup(&data[pos], length - pos);
  }
  i = 0;
  while (tagmap[i].name != NULL) {
    if (0 == strcmp(tagmap[i].name,
		    data))
      return addKeyword(tagmap[i].type,
			keyword, /* already in utf-8 */
			prev);
    i++;
  }
  return addKeyword(EXTRACTOR_UNKNOWN,
		    keyword,
		    prev);
}

static struct EXTRACTOR_Keywords * processIHDR(const char * data,
					       unsigned int length,
					       struct EXTRACTOR_Keywords * prev) {
  char * tmp;

  if (length < 12)
    return prev;

  tmp = malloc(128);
  snprintf(tmp,
	   128,
	   "%ux%u",
	   htonl(getIntAt(&data[4])),
	   htonl(getIntAt(&data[8])));
  return addKeyword(EXTRACTOR_SIZE,
		    tmp,
		    prev);
}

/* not supported... */
static struct EXTRACTOR_Keywords * processzTXt(const char * data,
					       unsigned int length,
					       struct EXTRACTOR_Keywords * prev) {
  char * keyword;
  unsigned int off;
  int i;
  char * buf;
  uLongf bufLen;
  int ret;

  data += 4;
  off = stnlen(data, length) + 1;
  if (off >= length)
    return prev; /* failed to find '\0' */
  if (data[off] != 0)
    return prev; /* compression method must be 0 */
  off++;

  bufLen = 1024 + 2 * (length - off);
  while (1) {
    if (bufLen * 2 < bufLen)
      return prev;
    bufLen *= 2;
    if (bufLen > 50 * (length - off)) {
      /* printf("zlib problem"); */
      return prev;
    }
    buf = malloc(bufLen);
    if (buf == NULL) {
      /* printf("out of memory"); */
      return prev; /* out of memory */
    }
    ret = uncompress((Bytef*) buf,
		     &bufLen,
		     (const Bytef*) &data[off],
		     length - off);
    if (ret == Z_OK) {
      /* printf("zlib ok"); */
      break;
    }
    free(buf);
    if (ret != Z_BUF_ERROR)
      return prev; /* unknown error, abort */
  }
  keyword = convertToUtf8(buf,
			  bufLen,
			  "ISO-8859-1");
  free(buf);
  i = 0;
  while (tagmap[i].name != NULL) {
    if (0 == strcmp(tagmap[i].name, data))
      return addKeyword(tagmap[i].type,
			keyword,
			prev);

    i++;
  }
  return addKeyword(EXTRACTOR_UNKNOWN,
		    keyword,
		    prev);
}

#define PNG_HEADER "\211PNG\r\n\032\n"



struct EXTRACTOR_Keywords * libextractor_png_extract(char * filename,
                                                     const char * data,
                                                     size_t size,
                                                     struct EXTRACTOR_Keywords * prev) {
  const char * pos;
  const char * end;
  struct EXTRACTOR_Keywords * result;
  unsigned int length;

  if (size < strlen(PNG_HEADER))
    return prev;
  if (0 != strncmp(data, PNG_HEADER, strlen(PNG_HEADER)))
    return prev;
  result = prev;
  end = &data[size];
  pos = &data[strlen(PNG_HEADER)];
  result = addKeyword(EXTRACTOR_MIMETYPE,
		      strdup("image/png"),
		      result);
  while(1) {
    if (pos+12 >= end)
      break;
    length = htonl(getIntAt(pos));  pos+=4;
    /* printf("Length: %u, pos %u\n", length, pos - data); */
    if ( (pos+4+length+4 > end) ||
	 (pos+4+length+4 < pos + 8) )
      break;

    if (0 == strncmp(pos, "IHDR", 4))
      result = processIHDR(pos, length, result);
    if (0 == strncmp(pos, "iTXt", 4))
      result = processiTXt(pos, length, result);
    if (0 == strncmp(pos, "tEXt", 4))
      result = processtEXt(pos, length, result);
    if (0 == strncmp(pos, "zTXt", 4))
      result = processzTXt(pos, length, result);
    pos += 4+length+4; /* Chunk type, data, crc */
  }
  return result;
}

