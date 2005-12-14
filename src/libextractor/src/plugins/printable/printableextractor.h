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

#ifndef FILTER_NAME
#fail Oops.
#endif
#ifndef FILTER_INIT_NAME
#fail Oops.
#endif
#ifndef EXTRACT_NAME
#fail Oops.
#endif

#include "platform.h"
#include "extractor.h"
#include <string.h>
#include "bloomfilter.h"

/**
 * Checks if a bit is active in the bitArray
 *
 * @param bitArray memory area to set the bit in
 * @param bitIdx which bit to test
 * @return 1 if the bit is set, 0 if not.
 */
static int testBit(unsigned char * bitArray,
		   unsigned int bitIdx) {
  unsigned int slot;
  unsigned int targetBit;

  slot = bitIdx / 8;
  targetBit = (1L << (bitIdx % 8));
  return (bitArray[slot] & targetBit) != 0;
}


/**
 * Callback: test if all bits are set
 *
 * @param bf the filter
 * @param bit the bit to test
 * @param arg pointer set to NO if bit is not set
 */
static void testBitCallback(Bloomfilter * bf,
			    unsigned int bit,
			    void * cls) {
  int * arg = cls;
  if (! testBit(bf->bitArray,
		bit))
    *arg = 0;
}
/**
 * Test if an element is in the filter.
 *
 * @param e the element
 * @param bf the filter
 * @return 1 if the element is in the filter, 0 if not
 */
static int testBloomfilter(Bloomfilter * bf,
			   const HashCode160 * e) {
  int res;

  if (NULL == bf)
    return 1;
  res = 1;
  iterateBits(bf,
	      &testBitCallback,
	      &res,
	      e);
  return res;
}


extern Bloomfilter FILTER_NAME;

static char * xstrndup(const char * s, size_t n){
  char * d;

  d= malloc(n+1);
  memcpy(d,s,n);
  d[n]='\0';
  return d;
}

static struct EXTRACTOR_Keywords * addKeyword(struct EXTRACTOR_Keywords * list,
					      char * keyword) {
  EXTRACTOR_KeywordList * next;
  next = malloc(sizeof(EXTRACTOR_KeywordList));
  next->next = list;
  next->keyword = keyword;
  next->keywordType = EXTRACTOR_UNKNOWN;
  return next;
}


/**
 *
 * @param word (alphabetic characters without spaces)
 * @return 0 if it is no word, 1 if it is
 **/
static int wordTest(char * word,
		    double * strlenthreshold) {
  int i;
  HashCode160 hc;
  char * lower;

  if (strlen(word) <= (int) (*strlenthreshold)) {
    return 0;
  }
  for (i=strlen(word)-1;i>=0;i--)
    if (isdigit(word[i]))
      return 0;

  hash(word,
       strlen(word),
       &hc);
  i = testBloomfilter(&FILTER_NAME,
		      &hc);
  if (! i) {
    int count =0;
    for (i=strlen(word)-1;i>=0;i--){
      if (isupper(word[i]))
	count++;
    }
    if ( ((count==1) && (isupper(word[0]))) ||
	 (count == strlen(word)) ){
      lower = strdup(word);
      for (i=strlen(lower)-1;i>=0;i--)
	lower[i] = tolower(lower[i]);
      hash(lower,
	   strlen(lower),
	   &hc);
      i = testBloomfilter(&FILTER_NAME,
			  &hc);
      free(lower);
    } else
      i=0;
  }
  if (i) {
    switch(strlen(word)) {
    case 1:
      *strlenthreshold = 6 * (*strlenthreshold);
      break;
    case 2:
      *strlenthreshold = 3 * (*strlenthreshold);
      break;
    case 3:
      *strlenthreshold = 1 + (*strlenthreshold);
      break;
    case 4:
      break;
    case 5:
      *strlenthreshold = (*strlenthreshold) - 1;
      break;
    case 6:
      *strlenthreshold = (*strlenthreshold) / 3;
      break;
    case 7:
      *strlenthreshold = (*strlenthreshold) / 6;
      break;
    case 8:
      *strlenthreshold = (*strlenthreshold) / 10;
      break;
    default:
      *strlenthreshold = 0.25;
      break;
    }
    if (*strlenthreshold < 0.25)
      *strlenthreshold = 0.25;
  }

  return i;
}

static void addKeywordToList(char * keyword,
			     struct EXTRACTOR_Keywords ** head,
			     struct EXTRACTOR_Keywords ** tail) {

  if (*tail != NULL) {
    (*tail)->next = addKeyword(NULL, keyword);
    *tail = (*tail)->next;
  } else {
    *tail = addKeyword(NULL, keyword);
    *head = *tail;
  }
}


static int process(char * keyword,
		   double * thresh,
		   struct EXTRACTOR_Keywords ** head,
		   struct EXTRACTOR_Keywords ** tail) {
  int i;
  int max = 0;
  int len;
  int p;
  int skip;
  char * sxdup;

  max = 0;
  p = 0;
  sxdup = strdup(keyword);
  len = strlen(keyword);
  for (i=0;i<len;i++) {
    if (isprint(keyword[i])) {
      keyword[p++] = keyword[i];
      continue;
    }
    while ( (! isprint(keyword[i+1]))
	    && (i<len-1) )
      i++;
    keyword[p] = 0;
    if (wordTest(keyword, thresh))
      max = p;
  }
  if (wordTest(keyword, thresh))
    max = p;
  if (max == 0) {
    free(keyword);
    if (isprint(sxdup[0])) {
      i=0;
      while ( (! isprint(sxdup[i+1]))
	     && (i<len-1) )
	i++;
      free(sxdup);
      return i+1;
    } else {
      free(sxdup);
      return 1;
    }
  }
  addKeywordToList(xstrndup(keyword, max),
		   head,
		   tail);
  free(keyword);
  p=0;
  skip = 0;
  for (i=0;i<len;i++) {
    if (isprint(sxdup[i])) {
      p++;
      continue;
    }
    skip++;
    if (p == max)
      break;
  }
  free(sxdup);
  return max + skip;
}

#define MAXBLEN 20

static void testKeyword(size_t start,
			size_t end,
			char * data,
			double * thresh,
			struct EXTRACTOR_Keywords ** head,
			struct EXTRACTOR_Keywords ** tail) {
  char * keyword;
  int i;
  int len;

  len = end-start;
  keyword = malloc(len + 1);
  memcpy(keyword, &data[start], len);
  for (i=len-1; i>=0; i--)
    if  (keyword[i]==0) keyword[i]=1;
  keyword[len] = 0;
  if (wordTest(keyword, thresh)) {
    addKeywordToList(keyword,
		     head,
		     tail);
    return;
  }
  i = 0;
  while (len - i > MAXBLEN) {
    i += process(xstrndup(&keyword[i], MAXBLEN),
		 thresh,
		 head,
		 tail);		
  }
  process(strdup(&keyword[i]),
	  thresh,
	  head,
	  tail);
  free(keyword);
}

static int isEndOfSentence(char c) {
  return ( (c == '.') ||
	   (c == '!') ||
	   (c == '?') );
}

static void processSentences(struct EXTRACTOR_Keywords ** head,
			     struct EXTRACTOR_Keywords ** tail) {
  int numSentences = 0;
  int numWords = 0;
  struct EXTRACTOR_Keywords * pos;
  struct EXTRACTOR_Keywords * start;
  struct EXTRACTOR_Keywords * rpos;
  struct EXTRACTOR_Keywords * last;
  char * sentence;
  int i;

  start = NULL;
  last = NULL;
  pos = *head;
  while (pos != NULL) {
    if ( (strlen(pos->keyword) > 1) ||
	 (! isEndOfSentence(pos->keyword[0]) ) ) {
      last = pos;
      pos = pos->next;
      numWords++;
      continue;
    }
    /* found end of sentence! */
    if ( ( (numWords < 3) || (numWords > 30) ) &&
	 ( (numSentences > 12) || (numWords < 2) ) ) {
      /* found reasonable amount of text,
	 discard "non-sentences" */
      if (start == NULL) {
	rpos = *head;
	*head = pos->next;
	pos->next = NULL;
	EXTRACTOR_freeKeywords(rpos);
	last = NULL;
	pos = *head;
	numWords = 0;
	continue;
      } else {
	rpos = start->next;
	start->next = pos->next;
	pos->next = NULL;
	EXTRACTOR_freeKeywords(rpos);
	last = start;
	pos = start->next;
	numWords = 0;
	continue;
      }
    }

    /* found sentence! build & advance start! */
    if (start == NULL)
      rpos = *head;
    else
      rpos = start->next;
    i = 1;
    while (rpos != pos) {
      i += strlen(rpos->keyword) + 1;
      rpos = rpos->next;
    }
    sentence = malloc(i);
    sentence[0] = 0;
    if (start == NULL)
      rpos = *head;
    else
      rpos = start->next;
    while (rpos != pos) {
      strcat(sentence, rpos->keyword);
      strcat(sentence, " ");
      rpos = rpos->next;
    }
    sentence[strlen(sentence)-1] = pos->keyword[0];
    sentence[i-1] = 0;

    if (start == NULL)
      rpos = *head;
    else
      rpos = start->next;
    if (start == NULL)
      *head = addKeyword(pos->next,
			 sentence);
    else
      start->next = addKeyword(pos->next,
			       sentence);
    pos->next = NULL;
    EXTRACTOR_freeKeywords(rpos);
    if (start == NULL)
      last = start = *head;
    else
      last = start = start->next;
    pos = last->next;
    numSentences++;
    numWords = 0;
    continue;	
  }
  *tail = last;
}

/* which mime-types should not be subjected to
   the printable extractor (e.g. because we have
   a more specific extractor available)? */
static char * blacklist[] = {
  "image/jpeg",
  "image/gif",
  "image/png",
  "image/x-png",
  "audio/real",
  "audio/mp3",
  "audio/mpeg",
  "application/x-gzip",
  "application/x-dpkg",
  "application/bz2",
  "application/x-rpm",
  "application/x-rar",
  "application/x-zip",
  "application/x-arj",
  "application/x-compress",
  "application/x-executable",
  "application/x-sharedlib",
  "application/x-archive",
  "application/x-tar",
  "application/x-lha",
  "application/x-gtar",
  "application/x-dpkg",
  "application/ogg",
  "video/real",
  "video/asf",
  "video/mpeg",
  "video/quicktime",
  NULL,
};

/* "man strings" gives for now an adequate description of
   what we are doing here.  EXTRACT_FUNC_NAME is set by
   Makefile.am to reflect the library that this code module
   is getting compiled into. */
struct EXTRACTOR_Keywords * EXTRACT_NAME (char * filename,
					  char * data,
					  size_t size,
					  struct EXTRACTOR_Keywords * prev) {
  int i;
  size_t last;
  size_t pos;
  const char * mime;
  struct EXTRACTOR_Keywords * head = NULL;
  struct EXTRACTOR_Keywords * tail = NULL;
  double thresh = 2.0;

  /* if the mime-type of the file is blacklisted, don't
     run the printable extactor! */
  mime = EXTRACTOR_extractLast(EXTRACTOR_MIMETYPE,
			       prev);
  if (mime != NULL) {
    int j;
    j = 0;
    while (blacklist[j] != NULL) {
      if (0 == strcmp(blacklist[j], mime))
	return prev;
      j++;
    }
  }

  last = 0;
  pos = 0;
  while (last < size) {
    pos = last;
    while ( (last < size) &&
	    (!isspace(data[last])) )
      last++;
    if ( (last < size) && (!isspace(data[last])) )
      last++;
    if (last >= size)
      break;
    for (i=pos;i<last;i++) {
      if ( isEndOfSentence(data[i])) {
	testKeyword(pos, i, data, &thresh, &head, &tail);
	if ( (i < size-1) && (isspace(data[i+1])) ) {
	  addKeywordToList(xstrndup(&data[i++],1),
			   &head,
			   &tail);
	}
	pos = i+1;
      } else {
	if ( (data[i] == ',') ||
	     (data[i] == ';') ||
	     (data[i] == ':') ||
	     (data[i] == '"') ) {
	  testKeyword(pos, i, data, &thresh, &head, &tail);
	  pos = i+1;
	}
      }
    }
    if (pos > last)
      continue;
    testKeyword(pos,
		last,
		data,
		&thresh,
		&head,
		&tail);
    while ( (last < size) &&
	    (isspace(data[last])) )	
      last++;
  }
  processSentences(&head, &tail);

  if (tail != NULL) {
    tail->next = prev;
    prev = head;
  }
  return prev;
}


/* end of printableextractor.c */
