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
 **/

#include "platform.h"
#include "extractor.h"

static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordType type,
					      char * keyword,
					      struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = strdup(keyword);
  result->keywordType = type;
  return result;
}

static char * readline(char * data,
		       size_t size,
		       size_t pos) {
  size_t end;
  char * res;

  while ( ( pos < size) &&
	  ( (data[pos] == (char)0x0d) ||
	              (data[pos] == (char)0x0a) ) )
    pos++;
		
  if (pos >= size)
    return NULL; /* end of file */
  end = pos;
  while ( (end < size) &&
	  (data[end] != (char)0x0d) &&
	  (data[end] != (char)0x0a) )
    end++;
  res = malloc(end-pos+1);
  memcpy(res,
	 &data[pos],
	 end-pos);
  res[end-pos] = '\0';
		
  return res;
}

static struct EXTRACTOR_Keywords * testmeta(char * line,
					    const char * match,
					    EXTRACTOR_KeywordType type,
					    struct EXTRACTOR_Keywords * prev) {
  if ( (strncmp(line, match, strlen(match)) == 0) &&
       (strlen(line) > strlen(match) ) ) {
    char * key;

    if ( (line[strlen(line)-1] == ')') &&
	 (line[strlen(match)] == '(') ) {
      key = &line[strlen(match)+1];
      key[strlen(key)-1] = '\0'; /* remove ")" */
    } else {
      key = &line[strlen(match)];
    }
    prev = addKeyword(type,
		      key,
		      prev);
  }
  return prev;
}

typedef struct {
  char * prefix;
  EXTRACTOR_KeywordType type;
} Matches;

static Matches tests[] = {
  { "%%Title: ", EXTRACTOR_TITLE },
  { "%%Version: ", EXTRACTOR_VERSIONNUMBER },
  { "%%Creator: ", EXTRACTOR_CREATOR },
  { "%%CreationDate: ", EXTRACTOR_CREATION_DATE },
  { "%%Pages: ", EXTRACTOR_PAGE_COUNT },
  { "%%Orientation: ", EXTRACTOR_UNKNOWN },
  { "%%DocumentPaperSizes: ", EXTRACTOR_UNKNOWN },
  { "%%DocumentFonts: ", EXTRACTOR_UNKNOWN },
  { "%%PageOrder: ", EXTRACTOR_UNKNOWN },
  { "%%For: ", EXTRACTOR_UNKNOWN },
  { "%%Magnification: ", EXTRACTOR_UNKNOWN },

  /* Also widely used but not supported since they
     probably make no sense:
  "%%BoundingBox: ",
  "%%DocumentNeededResources: ",
  "%%DocumentSuppliedResources: ",
  "%%DocumentProcSets: ",
  "%%DocumentData: ", */

  { NULL, 0 },
};

/* which mime-types should not be subjected to
   the PostScript extractor (no use trying) */
static char * blacklist[] = {
  "image/jpeg",
  "image/gif",
  "image/png",
  "image/x-png",
  "audio/real",
  "audio/mpeg",
  "application/x-gzip",
  "application/x-dpkg",
  "application/bz2",
  "application/x-rpm",
  "application/x-rar",
  "application/x-zip",
  "application/x-arj",
  "application/x-compress",
  "application/x-tar",
  "application/x-lha",
  "application/x-gtar",
  "application/x-dpkg",
  "application/ogg",
  "video/real",
  "video/asf",
  "video/quicktime",
  NULL,
};

/* mimetype = application/postscript */
struct EXTRACTOR_Keywords * libextractor_ps_extract(const char * filename,
                                                    char * data,
                                                    size_t size,
                                                    struct EXTRACTOR_Keywords * prev) {
  size_t pos;
  char * psheader = "%!PS-Adobe";
  char * line;
  int i;
  int lastLine;
  const char * mime;

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


  pos = 0;
  while ( (pos < size) &&
	  (pos < strlen(psheader)) &&
	  (data[pos] == psheader[pos]) )
    pos++;
  if (pos != strlen(psheader)) {
    return prev; /* no ps */
  }

  prev = addKeyword(EXTRACTOR_MIMETYPE,
		    "application/postscript",
		    prev);

  /* skip rest of first line */
  while ( (pos<size) && (data[pos] != '\n') )
    pos++;

  lastLine = -1;
  line = strdup(psheader);

  /* while Windows-PostScript does not seem to (always?) put
     "%%EndComments", this should allow us to not read through most of
     the file for all the sane applications... For Windows-generated
     PS files, we will bail out at the end of the file. */
  while (0 != strncmp("%%EndComments",
		      line,
		      strlen("%%EndComments"))) {
    free(line);
    line = readline(data, size, pos);
    if (line == NULL)
      break;
    i=0;
    while (tests[i].prefix != NULL) {
      prev = testmeta(line,
		      tests[i].prefix,
		      tests[i].type,
		      prev);
      i++;
    }

    /* %%+ continues previous meta-data type... */
    if ( (lastLine != -1) &&
	 (0 == strncmp(line, "%%+ ", strlen("%%+ "))) ) {
      prev = testmeta(line,
		      "%%+ ",
		      tests[lastLine].type,
		      prev);
    } else {
      /* update "previous" type */
      if (tests[i].prefix == NULL)
	lastLine = -1;
      else
	lastLine = i;
    }
    pos += strlen(line)+1; /* skip newline, too; guarantee progress! */
  }
  free(line);

  return prev;
}

/* end of psextractor.c */
