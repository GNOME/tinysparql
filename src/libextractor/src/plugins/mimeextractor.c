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
 * Detect a file-type.
 * @param data the contents of the file
 * @param len the length of the file
 * @param arg closure...
 * @return 0 if the file does not match, 1 if it does
 **/
typedef int (*Detector)(char * data,
			size_t len,
			void * arg);

/**
 * Detect a file-type.
 * @param data the contents of the file
 * @param len the length of the file
 * @return always 1
 **/
static int defaultDetector(char * data,
			   size_t len,
			   void * arg) {
  return 1;
}

/**
 * Detect a file-type.
 * @param data the contents of the file
 * @param len the length of the file
 * @return always 0
 **/
static int disableDetector(char * data,
			   size_t len,
			   void * arg) {
  return 0;
}

typedef struct ExtraPattern {
  int pos;
  int len;
  char * pattern;
} ExtraPattern;

/**
 * Define special matching rules for complicated formats...
 **/
static ExtraPattern xpatterns[] = {
#define AVI_XPATTERN 0
  { 8, 4, "AVI "},
  { 0, 0, NULL },
#define WAVE_XPATTERN 2
  { 8, 4, "WAVE"},
  { 0, 0, NULL },
#define ACE_XPATTERN 4
  { 4, 10, "\x00\x00\x90**ACE**"},
  { 0, 0, NULL },
#define TAR_XPATTERN 6
  { 257, 6, "ustar\x00"},
  { 0, 0, NULL },
#define GTAR_XPATTERN 8
  { 257, 8, "ustar\040\040\0"},
  { 0, 0, NULL },
#define RMID_XPATTERN 10
  { 8, 4, "RMID"},
  { 0, 0, NULL },
#define ACON_XPATTERN 12
  { 8, 4, "ACON"},
  { 0, 0, NULL},
};

/**
 * Detect AVI. A pattern matches if all XPatterns until the next {0,
 * 0, NULL} slot match. OR-ing patterns can be achieved using multiple
 * entries in the main table, so this "AND" (all match) semantics are
 * the only reasonable answer.
 **/
static int xPatternMatcher(char * data,
			   size_t len,
			   ExtraPattern * arg) {
  while (arg->pattern != NULL) {
    if (arg->pos + arg->len > len)
      return 0;
    if (0 != memcmp(&data[arg->pos],
		    arg->pattern,
		    arg->len))
      return 0;
    arg++;
  }
  return 1;
}

/**
 * Use this detector, if the simple header-prefix matching is
 * sufficient.
 **/
#define DEFAULT &defaultDetector, NULL

/**
 * Use this detector, to disable the mime-type (effectively comment it
 * out).
 **/
#define DISABLED &disableDetector, NULL

/**
 * Select an entry in xpatterns for matching
 **/
#define XPATTERN(a) (Detector)&xPatternMatcher, &xpatterns[(a)]
				
typedef struct Pattern {
  char * pattern;
  int size;
  char * mimetype;
  Detector detector;
  void * arg;
} Pattern;

static Pattern patterns[] = {
  { "\xFF\xD8", 2, "image/jpeg", DEFAULT},
  { "\211PNG\r\n\032\n", 8, "image/png", DEFAULT},
  { "/* XPM */", 9, "image/x-xpm", DEFAULT},
  { "GIF8", 4, "image/gif", DEFAULT},
  { "P1", 2, "image/x-portable-bitmap", DEFAULT},
  { "P2", 2, "image/x-portable-graymap", DEFAULT},
  { "P3", 2, "image/x-portable-pixmap", DEFAULT},
  { "P4", 2, "image/x-portable-bitmap", DEFAULT},
  { "P5", 2, "image/x-portable-graymap", DEFAULT},
  { "P6", 2, "image/x-portable-pixmap", DEFAULT},
  { "P7", 2, "image/x-portable-anymap", DEFAULT},
  { "BM", 2, "image/x-bmp", DEFAULT},
  { "\x89PNG", 4, "image/x-png", DEFAULT},
  { "id=ImageMagick", 14, "application/x-imagemagick-image", DEFAULT},
  { "hsi1", 4, "image/x-jpeg-proprietary", DEFAULT},
  { "\x2E\x52\x4d\x46", 4, "video/real", DEFAULT},
  { "\x2e\x72\x61\xfd", 4, "audio/real", DEFAULT},
  { "\177ELF", 4, "application/elf", DEFAULT},
  /* FIXME: correct MIME-type for an ELF!? */
  { "\xca\xfe\xba\xbe", 4, "application/java", DEFAULT},
  /* FIXME: correct MIME for a class-file? */
  { "gimp xcf", 8, "image/xcf", DEFAULT},
  { "IIN1", 4, "image/tiff", DEFAULT},
  { "MM\x00\x2a", 4, "image/tiff", DEFAULT}, /* big-endian */
  { "II\x2a\x00", 4, "image/tiff", DEFAULT}, /* little-endian */
  { "%PDF", 4, "application/pdf", DEFAULT},
  { "%!PS-Adobe-", 11, "application/postscript", DEFAULT},
  { "\004%!PS-Adobe-", 12, "application/postscript, DEFAULT"}, /* DOS? :-) */
  { "RIFF", 4, "video/x-msvideo", XPATTERN(AVI_XPATTERN)},
  { "RIFF", 4, "audio/x-wav", XPATTERN(WAVE_XPATTERN)},
  { "RIFX", 4, "video/x-msvideo", XPATTERN(AVI_XPATTERN)},
  { "RIFX", 4, "audio/x-wav", XPATTERN(WAVE_XPATTERN)},
  { "RIFF", 4, "audio/midi", XPATTERN(RMID_XPATTERN)},
  { "RIFX", 4, "audio/midi", XPATTERN(RMID_XPATTERN)},
  { "RIFF", 4, "image/x-animated-cursor", XPATTERN(ACON_XPATTERN)},
  { "RIFX", 4, "image/x-animated-cursor", XPATTERN(ACON_XPATTERN)},
  { "\211GND\r\n\032\n", 8, "application/gnunet-directory", DEFAULT},
  { "{\\rtf", 5, "application/rtf", DEFAULT},
  { "\xf7\x02", 2, "application/x-dvi", DEFAULT},
  { "\x1F\x8B\x08\x00", 4, "application/x-gzip", DEFAULT},
  { "BZh91AY&SY", 10, "application/bz2", DEFAULT},
  { "\xED\xAB\xEE\xDB", 4, "application/x-rpm", DEFAULT}, /* binary */
  { "!<arch>\ndebian", 14, "application/x-dpkg", DEFAULT}, /* .deb */
  { "PK\x03\x04", 4, "application/x-zip", DEFAULT},
  { "\xea\x60", 2, "application/x-arj", DEFAULT},
  { "\037\235", 2, "application/x-compress", DEFAULT},
  { "Rar!", 4, "application/x-rar", DEFAULT},
  { "", 0, "application/x-ace", XPATTERN(ACE_XPATTERN)},
  { "", 0, "application/x-tar", XPATTERN(TAR_XPATTERN)},
  { "", 0, "application/x-gtar", XPATTERN(GTAR_XPATTERN)},
  { "-lh0-", 5, "application/x-lha", DEFAULT},
  { "-lh1-", 5, "application/x-lha", DEFAULT},
  { "-lh2-", 5, "application/x-lha", DEFAULT},
  { "-lh3-", 5, "application/x-lha", DEFAULT},
  { "-lh4-", 5, "application/x-lha", DEFAULT},
  { "-lh5-", 5, "application/x-lha", DEFAULT},
  { "-lh6-", 5, "application/x-lha", DEFAULT},
  { "-lh7-", 5, "application/x-lha", DEFAULT},
  { "-lhd-", 5, "application/x-lha", DEFAULT},
  { "-lh\40-", 5, "application/x-lha", DEFAULT},
  { "-lz4-", 5, "application/x-lha", DEFAULT},
  { "-lz5-", 5, "application/x-lha", DEFAULT},
  { "-lzs-", 5, "application/x-lha", DEFAULT},
  { "\xFD\x76", 2, "application/x-lzh", DEFAULT},
  { "\x00\x00\x01\xb3", 4, "video/mpeg", DEFAULT},
  { "\x00\x00\x01\xba", 4, "video/mpeg", DEFAULT},
  { "moov", 4, "video/quicktime", DEFAULT},
  { "mdat", 4, "video/quicktime", DEFAULT},
  { "\x8aMNG", 4, "video/x-mng", DEFAULT},
  { "\x30\x26\xb2\x75\x8e\x66", 6, "video/asf", DEFAULT}, /* same as .wmv ? */
  { "FWS", 3, "application/x-shockwave-flash", DEFAULT},
  { "MThd", 4, "audio/midi", DEFAULT},
  { "ID3", 3, "audio/mpeg", DEFAULT},
  { "\xFF\xFA", 2, "audio/mpeg", DEFAULT},
  { "\xFF\xFB", 2, "audio/mpeg", DEFAULT},
  { "\xFF\xFC", 2, "audio/mpeg", DEFAULT},
  { "\xFF\xFD", 2, "audio/mpeg", DEFAULT},
  { "\xFF\xFE", 2, "audio/mpeg", DEFAULT},
  { "\xFF\xFF", 2, "audio/mpeg", DEFAULT},
  { "OggS", 4, "application/ogg", DEFAULT},
  { "#!/bin/sh",    9, "application/x-shellscript", DEFAULT},
  { "#!/bin/bash", 11, "application/x-shellscript", DEFAULT},
  { "#!/bin/csh",  10, "application/x-shellscript", DEFAULT},
  { "#!/bin/tcsh", 11, "application/x-shellscript", DEFAULT},
  { "#!/bin/perl", 11, "application/x-perl", DEFAULT},
  {NULL, 0, NULL, DISABLED},
};

struct EXTRACTOR_Keywords * libextractor_mime_extract(char * filename,
                                                      char * data,
                                                      size_t size,
                                                      struct EXTRACTOR_Keywords * prev) {
  int i;
  const char * mime;

  mime = EXTRACTOR_extractLast(EXTRACTOR_MIMETYPE,
			       prev);
  if (mime != NULL)
    return prev; /* if the mime-type has already
		    been determined, there is no need
		    to probe again (and potentially be wrong...) */
  i=0;
  while (patterns[i].pattern != NULL) {
    if (size < patterns[i].size) {
      i++;
      continue;
    }
    if (0 == memcmp(patterns[i].pattern,
		    data,
		    patterns[i].size)) {
      if (patterns[i].detector(data, size, patterns[i].arg))
	return addKeyword(EXTRACTOR_MIMETYPE,
			  strdup(patterns[i].mimetype),
			  prev);
    }
    i++;
  }
  return prev;
}

