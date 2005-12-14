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

#define DEBUG_EXTRACT_OGG 0
#define OGG_HEADER 0x4f676753

#if HAVE_VORBIS_VORBISFILE_H
#include <vorbis/vorbisfile.h>
#else
#error You must install the libvorbis header files!
#endif

static char* get_comment(vorbis_comment *vc, char *label) {
  char *tag;
  if (vc && (tag = vorbis_comment_query(vc, label, 0)) != NULL)
    return tag;
  else
    return NULL;
}

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

static size_t readError(void * ptr,
			size_t size,
			size_t nmemb,
			void * datasource) {
  return -1;
}

static int seekError(void * datasource,
		     int64_t offset,
		     int whence) {
  return -1;
}

static int closeOk(void * datasource) {
  return 0;
}

static long tellError(void * datasource) {
  return -1;
}

/* mimetype = application/ogg */
struct EXTRACTOR_Keywords * libextractor_ogg_extract(const char * filename,
                                                     char * data,
                                                     size_t size,
                                                     struct EXTRACTOR_Keywords * prev) {
  OggVorbis_File vf;
  vorbis_comment * comments;
  ov_callbacks callbacks;

  if (size < 2*sizeof(int)) {
    return prev;
  }
  if (OGG_HEADER !=  ntohl(*(int*)data)) {
    return prev;
  }

  callbacks.read_func = &readError;
  callbacks.seek_func = &seekError;
  callbacks.close_func = &closeOk;
  callbacks.tell_func = &tellError;
  if (0 != ov_open_callbacks(NULL, &vf, data, size, callbacks)) {
    ov_clear(&vf);
    return prev;
  }
  comments = ov_comment(&vf, -1);

  if (NULL == comments) {
    ov_clear(&vf);
    return prev;
  }
  if ( (comments->vendor != NULL) && (strlen(comments->vendor) > 0) )
    prev = addKeyword(EXTRACTOR_PUBLISHER, comments->vendor, prev);

  prev = addKeyword(EXTRACTOR_TITLE, get_comment(comments, "title"), prev);
  prev = addKeyword(EXTRACTOR_ARTIST, get_comment(comments, "artist"), prev);
  prev = addKeyword(EXTRACTOR_INTERPRET, get_comment(comments, "performer"), prev);
  prev = addKeyword(EXTRACTOR_ALBUM, get_comment(comments, "album"), prev);
  prev = addKeyword(EXTRACTOR_CONTACT, get_comment(comments, "contact"), prev);
  prev = addKeyword(EXTRACTOR_GENRE, get_comment(comments, "genre"), prev);
  prev = addKeyword(EXTRACTOR_DATE, get_comment(comments, "date"), prev);
  prev = addKeyword(EXTRACTOR_COMMENT, get_comment(comments, ""), prev);
  prev = addKeyword(EXTRACTOR_LOCATION,get_comment(comments, "location"), prev);
  prev = addKeyword(EXTRACTOR_DESCRIPTION, get_comment(comments, "description"), prev);
  prev = addKeyword(EXTRACTOR_VERSIONNUMBER, get_comment(comments, "version"), prev);
  prev = addKeyword(EXTRACTOR_RESOURCE_IDENTIFIER, get_comment(comments, "isrc"), prev);
  prev = addKeyword(EXTRACTOR_ORGANIZATION, get_comment(comments, "organization"), prev);
  prev = addKeyword(EXTRACTOR_COPYRIGHT, get_comment(comments, "copyright"), prev);
  /* we have determined for sure that this is an
     ogg-vorbis stream, we should add this as a keyword, too */
  prev = addKeyword(EXTRACTOR_MIMETYPE,
		    "application/ogg",
		    prev);
  /* build a description from artist, title and album */
  {
    EXTRACTOR_KeywordList * keyword = malloc(sizeof(EXTRACTOR_KeywordList));
    char * word;
    int len = 1+2+2+1;
    if (get_comment(comments, "artist") != NULL)
      len += strlen(get_comment(comments, "artist"));
    if (get_comment(comments, "title") != NULL)
      len += strlen(get_comment(comments, "title"));
    if (get_comment(comments, "album") != NULL)
      len += strlen(get_comment(comments, "album"));

    word = malloc(len);
    word[0] = 0;
    if (get_comment(comments, "artist") != NULL) {
      strcat(word, get_comment(comments, "artist"));
    }
    if (get_comment(comments, "title") != NULL) {
      strcat(word,": ");
      strcat(word, get_comment(comments, "title"));
    }
    if (get_comment(comments, "album") != NULL) {
      strcat(word," (");
      strcat(word, get_comment(comments, "album"));
      strcat(word, ")");
    }
    keyword->next = prev;
    keyword->keyword = word;
    keyword->keywordType = EXTRACTOR_DESCRIPTION;
    prev = keyword;

  }

  ov_clear(&vf);
  return prev;
}
