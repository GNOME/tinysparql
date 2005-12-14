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

typedef struct {
  char * text;
  EXTRACTOR_KeywordType type;
} Matches;

static Matches tmap[] = {
  { "/Title (", EXTRACTOR_TITLE },
  { "/Subject (", EXTRACTOR_SUBJECT },
  { "/Author (", EXTRACTOR_AUTHOR },
  { "/Keywords (", EXTRACTOR_KEYWORDS },
  { "/Creator (", EXTRACTOR_CREATOR },
  { "/Producer (", EXTRACTOR_PRODUCER },
  { NULL, 0 },
};

static struct EXTRACTOR_Keywords * parseZZZ(const char * data,
					    size_t pos,
					    size_t len,
					    struct EXTRACTOR_Keywords * prev) {
  size_t slen;
  size_t end;
  int i;
  char * value;

  end = pos + len;
  slen = strlen("ps:SDict begin [");
  if (len <= slen)
    return prev;
  if (0 != strncmp("ps:SDict begin [ ",
		   &data[pos],
		   slen))
    return prev;
  pos += slen;
  while (pos < end) {
    i = 0;
    while (tmap[i].text != NULL) {
      slen = strlen(tmap[i].text);
      if (pos + slen < end) {
	if (0 == strncmp(&data[pos],
			 tmap[i].text,
			 slen)) {
	  pos += slen;
	  slen = pos;
	  while ( (slen < end) && (data[slen] != ')') )
	    slen++;
	  slen = slen - pos;
	  value = malloc(slen+1);
	  value[slen] = '\0';
	  memcpy(value,
		 &data[pos],
		 slen);
	  prev = addKeyword(tmap[i].type,
			    value,
			    prev);	
	  pos += slen + 1;
	}
      }
      i++;
    }
    pos++;
  }
  return prev;
}

static unsigned int getIntAt(const void * data) {
  char p[4];

  memcpy(p, data, 4); /* ensure alignment! */
  return *(unsigned int*)&p[0];
}

static unsigned int getShortAt(const void * data) {
  char p[2];

  memcpy(p, data, 2); /* ensure alignment! */
  return *(unsigned short*)&p[0];
}

struct EXTRACTOR_Keywords * libextractor_dvi_extract(const char * filename,
						     const unsigned char * data,
						     size_t size,
						     struct EXTRACTOR_Keywords * prev) {
  unsigned int klen;
  char * comment;
  unsigned int pos;
  unsigned int opos;
  unsigned int len;
  unsigned int pageCount;
  char * pages;

  if (size < 40)
    return prev;
  if ( (data[0] != 247) || (data[1] != 2) )
    return prev; /* cannot be dvi or unsupported version */
  klen = data[14];

  pos = size-1;
  while ( (data[pos] == 223) && (pos > 0) )
    pos--;
  if ( (data[pos] != 2) || (pos < 40) )
    return prev;
  pos--;
  pos -= 4;
  /* assert pos at 'post_post tag' */
  if (data[pos] != 249)
    return prev;
  opos = pos;
  pos = ntohl(getIntAt(&data[opos+1]));
  if (pos+25 > size)
    return prev;
  /* assert pos at 'post' command */
  if (data[pos] != 248)
    return prev;
  pageCount = 0;
  opos = pos;
  pos = ntohl(getIntAt(&data[opos+1]));
  while (1) {
    if (pos == (unsigned int)-1)
      break;
    if (pos+45 > size)
      return prev;
    if (data[pos] != 139) /* expect 'bop' */
      return prev;
    pageCount++;
    opos = pos;
    pos = ntohl(getIntAt(&data[opos+41]));
    if (pos == (unsigned int)-1)
      break;
    if (pos >= opos)
      return prev; /* invalid! */
  }
  /* ok, now we believe it's a dvi... */
  pages = malloc(16);
  snprintf(pages,
	   16,
	   "%u",
	   pageCount);
  comment = malloc(klen+1);
  comment[klen] = '\0';
  memcpy(comment,
	 &data[15],
	 klen);
  prev = addKeyword(EXTRACTOR_MIMETYPE,
		    strdup("application/x-dvi"),
		    prev);
  prev = addKeyword(EXTRACTOR_COMMENT,
		    comment,
		    prev);
  prev = addKeyword(EXTRACTOR_PAGE_COUNT,
		    pages,
		    prev);
  /* try to find PDF/ps special */
  pos = opos;
  while (pos < size - 100) {
    switch (data[pos]) {
    case 139: /* begin page 'bop', we typically have to skip that one to
		 find the zzz's */
      pos += 45; /* skip bop */
      break;
    case 239: /* zzz1 */
      len = data[pos+1];
      if (pos + 2 + len < size)
	prev = parseZZZ((const char*) data,
			pos+2,
			len,
			prev);
      pos += len+2;
      break;
    case 240: /* zzz2 */
      len = ntohs(getShortAt(&data[pos+1]));
      if (pos + 3 + len < size)
	prev = parseZZZ((const char*) data,
			pos+3,
			len,
			prev);
      pos += len+3;
      break;
    case 241: /* zzz3, who uses that? */
      len = (ntohs(getShortAt(&data[pos+1]))) + 65536 * data[pos+3];
      if (pos + 4 + len < size)
	prev = parseZZZ((const char*) data,
			pos+4,
			len,
			prev);
      pos += len+4;
      break;
    case 242: /* zzz4, hurray! */
      len = ntohl(getIntAt(&data[pos+1]));
      if (pos + 1 + len < size)
	prev = parseZZZ((const char*) data,
			pos+5,
			len,
			prev);
      pos += len+5;
      break;
    default: /* unsupported opcode, abort scan */
      return prev;
    }
  }
  return prev;
}

