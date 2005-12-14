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

     This code was inspired by pdfinfo and depends heavily
     on the xpdf code that pdfinfo is a part of. See also
     the INFO file in this directory.
 */

#include "platform.h"
#include "extractor.h"
#include "../convert.h"
#include <math.h>

#include "parseargs.h"
#include "GString.h"
#include "gmem.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

extern "C" {

  static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordType type,
						char * keyword,
						struct EXTRACTOR_Keywords * next) {
    EXTRACTOR_KeywordList * result;

    if (keyword == NULL)
      return next;
    result = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
    result->next = next;
    result->keyword = keyword;
    result->keywordType = type;
    return result;
  }


  static struct EXTRACTOR_Keywords * printInfoString(Dict *infoDict,
						     char *key,
						     EXTRACTOR_KeywordType type,
						     struct EXTRACTOR_Keywords * next) {
    Object obj;
    GString *s1;
    char * s;

    if (infoDict->lookup(key, &obj)->isString()) {
      s1 = obj.getString();
      s = s1->getCString();
      if ((((unsigned char)s[0]) & 0xff) == 0xfe &&
	  (((unsigned char)s[1]) & 0xff) == 0xff) {
	char * result;
	unsigned char u[2];
	unsigned int pos;
	unsigned int len;
	char * con;

	result = (char*) malloc(s1->getLength() * 4);
	result[0] = '\0';
	len = s1->getLength();
	for (pos=0;pos<len;pos+=2) {
	  u[0] = s1->getChar(pos+1);
	  u[1] = s1->getChar(pos);
	  con = (char*) convertToUtf8((const char*) u, 2, "UNICODE");
	  strcat(result, con);
	  free(con);
	}
	next = addKeyword(type,
			  strdup(result),
			  next);
	free(result);
      } else {
        unsigned int len = (NULL == s) ? 0 : strlen(s);

        while(0 < len) {
        /*
         * Avoid outputting trailing spaces.
         *
         * The following expression might be rewritten as
         * (! isspace(s[len - 1]) && 0xA0 != s[len - 1]).
         * There seem to exist isspace() implementations 
         * which do return non-zero from NBSP (maybe locale-dependent).
         * Remove ISO-8859 non-breaking space (NBSP, hex value 0xA0) from
         * the expression if it looks suspicious (locale issues for instance).
         *
         * Squeezing out all non-printable characters might also be useful.
         */
          if ( (' '  != s[len - 1]) && ((char)0xA0 != s[len - 1]) &&
               ('\r' != s[len - 1]) && ('\n' != s[len - 1]) &&
               ('\t' != s[len - 1]) && ('\v' != s[len - 1]) &&
               ('\f' != s[len - 1]) )
             break;

          else
            len --;
        }

        /* there should be a check to truncate preposterously long values. */

        if (0 < len) {
	  next = addKeyword(type,
	  		  convertToUtf8(s, len,
					"ISO-8859-1"),
			  next);
        }
      }
    }
    obj.free();
    return next;
  }

  static struct EXTRACTOR_Keywords * printInfoDate(Dict *infoDict,
						   char *key,
						   EXTRACTOR_KeywordType type,
						   struct EXTRACTOR_Keywords * next) {
    Object obj;
    char *s;
    GString *s1;

    if (infoDict->lookup(key, &obj)->isString()) {
      s1 = obj.getString();
      s = s1->getCString();

      if ((s1->getChar(0) & 0xff) == 0xfe &&
	  (s1->getChar(1) & 0xff) == 0xff) {
	/* isUnicode */
	char * result;
	unsigned char u[2];
	unsigned int pos;
	unsigned int len;
	char * con;

	result = (char*) malloc(s1->getLength() * 4);
	result[0] = '\0';
	len = s1->getLength();
	for (pos=0;pos<len;pos+=2) {
	  u[0] = s1->getChar(pos+1);
	  u[1] = s1->getChar(pos);
	  con = (char*) convertToUtf8((const char*) u, 2, "UNICODE");
	  strcat(result, con);
	  free(con);
	}		
	next = addKeyword(type,
			  strdup(result),
			  next);
	free(result);
      } else {
	if (s[0] == 'D' && s[1] == ':') {
	  s += 2;
	}
	next = addKeyword(type, strdup(s), next);
      }
      /* printf(fmt, s);*/
    }
    obj.free();
    return next;
  }


  /* which mime-types should not be subjected to
     the PDF extractor? (no use trying!) */
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


  static const char *
  extractLast (const EXTRACTOR_KeywordType type,
	       EXTRACTOR_KeywordList * keywords)
  {
    char *result = NULL;
    while (keywords != NULL)
      {
	if (keywords->keywordType == type)
	  result = keywords->keyword;
	keywords = keywords->next;
      }
    return result;
  }

  struct EXTRACTOR_Keywords * libextractor_pdf_extract(const char * filename,
						       char * data,
                                                       size_t size,
                                                       struct EXTRACTOR_Keywords * prev) {
    PDFDoc * doc;
    Object info;
    Object obj;
    BaseStream * stream;
    struct EXTRACTOR_Keywords * result;
    const char * mime;

    /* if the mime-type of the file is blacklisted, don't
       run the printable extactor! */
    mime = extractLast(EXTRACTOR_MIMETYPE,
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

    /* errorInit();   -- keep commented out, otherwise errors are printed to stderr for non-pdf files! */
    obj.initNull();
    stream = new MemStream(data, 0, size, &obj);
    doc = new PDFDoc(stream, NULL, NULL);
    if (! doc->isOk()) {
      delete doc;
      return prev;
    }

    result = addKeyword(EXTRACTOR_MIMETYPE,
			strdup("application/pdf"),
			prev);
    doc->getDocInfo(&info);
    if (info.isDict()) {
      result = printInfoString(info.getDict(),
			       "Title",
			       EXTRACTOR_TITLE,
			       result);
      result = printInfoString(info.getDict(),
			       "Subject",
			       EXTRACTOR_SUBJECT,
			       result);
      result = printInfoString(info.getDict(),
			       "Keywords",
			       EXTRACTOR_KEYWORDS,
			       result);
      result = printInfoString(info.getDict(),
			       "Author",
			       EXTRACTOR_AUTHOR,
			       result);
      /*
       * we now believe that Adobe's Creator
       * is not a person nor an organisation,
       * but just a piece of software.
       */
      result = printInfoString(info.getDict(),
			       "Creator",
			       EXTRACTOR_SOFTWARE,
			       result);
      result = printInfoString(info.getDict(),
			       "Producer",
			       EXTRACTOR_PRODUCER,
			       result);
      {
	char pcnt[20];
	sprintf(pcnt, "%d", doc->getNumPages());
	result = addKeyword(EXTRACTOR_PAGE_COUNT,
			    strdup(pcnt),
			    result);
      }
      {
	char pcnt[20];
	sprintf(pcnt, "PDF %.1f", doc->getPDFVersion());
	result = addKeyword(EXTRACTOR_FORMAT,
			    strdup(pcnt),
			    result);
      }
      result = printInfoDate(info.getDict(),
			     "CreationDate",
			     EXTRACTOR_CREATION_DATE,
			     result);
      result = printInfoDate(info.getDict(),
			     "ModDate",
			     EXTRACTOR_MODIFICATION_DATE,
			     result);
    }

    info.free();
    delete doc;

    return result;
  }
}



void __attribute__ ((constructor)) xpdf_init(void) {
  initParams(".xpdfrc", ".xpdfrc");
}

void __attribute__ ((destructor)) xpdf_done(void) {
  freeParams();
}
