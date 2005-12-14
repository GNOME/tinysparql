/*
     This file is part of libextractor.
     (C) 2002, 2003, 2005 Vidyut Samanta and Christian Grothoff

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

static char * TOKENIZERS = "._ ,%@-\n_[](){}";
static int MINIMUM_KEYWORD_LENGTH = 4;

static void addKeyword(struct EXTRACTOR_Keywords ** list,
																							char * keyword,
																							EXTRACTOR_KeywordType type) {
  EXTRACTOR_KeywordList * next;
  next = malloc(sizeof(EXTRACTOR_KeywordList));
  next->next = *list;
  next->keyword = strdup(keyword);
  next->keywordType = type;
  *list = next;
}

static int token(char letter,
																	const char * options) {
  int i;

		if (options == NULL)
				options = TOKENIZERS;
  for (i=0;i<strlen(TOKENIZERS);i++)
    if (letter == TOKENIZERS[i])
      return 1;
  return 0;
}

static void splitKeywords(char * keyword,
																										EXTRACTOR_KeywordType type,
																										struct EXTRACTOR_Keywords ** list,
																										const char * options) {
  char * dp;
  int pos;
  int last;
  int len;

  dp = strdup(keyword);
  len = strlen(dp);
  pos = 0;
  last = 0;
  while (pos < len) {
    while ((!token(dp[pos],
																			options)) && (pos < len))
      pos++;
    dp[pos++] = 0;
    if (strlen(&dp[last]) >= MINIMUM_KEYWORD_LENGTH) {
      addKeyword(list, &dp[last], type);
    }
    last = pos;
  }
  free(dp);
}

/* split other keywords into multiple keywords */
struct EXTRACTOR_Keywords *
libextractor_split_extract(char * filename,
																											char * data,
																											size_t size,
																											struct EXTRACTOR_Keywords * prev,
																											const char * options) {
  struct EXTRACTOR_Keywords * pos;

  pos = prev;
  while (pos != NULL) {
    splitKeywords(pos->keyword,
																		EXTRACTOR_UNKNOWN,
																		&prev,
																		options);
    pos = pos->next;
  }
  return prev;
}
