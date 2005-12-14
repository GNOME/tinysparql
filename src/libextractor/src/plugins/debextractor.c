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
#include <zlib.h>

/*
 * The .deb is an ar-chive file.  It contains a tar.gz file
 * named "control.tar.gz" which then contains a file 'control'
 * that has the meta-data.  And which variant of the various
 * ar file formats is used is also not quite certain. Yuck.
 *
 * References:
 * http://www.mkssoftware.com/docs/man4/tar.4.asp
 * http://lists.debian.org/debian-policy/2003/12/msg00000.html
 * http://www.opengroup.org/onlinepubs/009695399/utilities/ar.html
 */

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

static char * stndup(const char * str,
                     size_t n) {
  char * tmp;
  tmp = malloc(n+1);
  tmp[n] = '\0';
  memcpy(tmp, str, n);
  return tmp;
}



typedef struct {
  char * text;
  EXTRACTOR_KeywordType type;
} Matches;

/* see also: "man 5 deb-control" */
static Matches tmap[] = {
  { "Package: ",        EXTRACTOR_SOFTWARE },
  { "Version: ",        EXTRACTOR_VERSIONNUMBER },
  { "Section: ",        EXTRACTOR_GENRE },
  { "Priority: ",       EXTRACTOR_PRIORITY },
  { "Architecture: ",   EXTRACTOR_CREATED_FOR },
  { "Depends: ",        EXTRACTOR_DEPENDENCY },
  { "Recommends: ",     EXTRACTOR_RELATION },
  { "Suggests: ",       EXTRACTOR_RELATION },
  { "Installed-Size: ", EXTRACTOR_SIZE },
  { "Maintainer: ",     EXTRACTOR_PACKAGER },
  { "Description: ",    EXTRACTOR_DESCRIPTION },
  { "Source: ",         EXTRACTOR_SOURCE },
  { "Pre-Depends: ",    EXTRACTOR_DEPENDENCY },
  { "Conflicts: ",      EXTRACTOR_CONFLICTS },
  { "Replaces: ",       EXTRACTOR_REPLACES },
  { "Provides: ",       EXTRACTOR_PROVIDES },
  { NULL, 0 },
  { "Essential: ",      EXTRACTOR_UNKNOWN }
};


/**
 * Process the control file.
 */
static struct EXTRACTOR_Keywords * processControl(const char * data,
						  const size_t size,
						  struct EXTRACTOR_Keywords * prev) {
  size_t pos;
  char * key;

  pos = 0;
  while (pos < size) {
    size_t colon;
    size_t eol;
    int i;

    colon = pos;
    while (data[colon] != ':') {
      if ( (colon > size) || (data[colon] == '\n') )
	return prev;
      colon++;
    }
    colon++;
    while ( (colon < size) &&
	    (isspace(data[colon]) ) )
      colon++;
    eol = colon;
    while ( (eol < size) &&
	    ( (data[eol] != '\n') ||
	      ( (eol+1 < size) &&
	        (data[eol+1] == ' ') ) ) )
      eol++;
    if ( (eol == colon) || (eol > size) )
      return prev;
    key = stndup(&data[pos], colon-pos);
    i = 0;
    while (tmap[i].text != NULL) {
      if (0 == strcmp(key, tmap[i].text)) {
	char * val;

	val = stndup(&data[colon], eol-colon);
	prev = addKeyword(tmap[i].type,
			  val,
			  prev);
	break;
      }
      i++;
    }
    free(key);
    pos = eol+1;
  }
  return prev;
}


typedef struct {
  char name[100];
  char mode[8];
  char userId[8];
  char groupId[8];
  char filesize[12];
  char lastModTime [12];
  char chksum[8];
  char link;
  char linkName[100];
} TarHeader;

typedef struct {
  TarHeader tar;
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor [8];
  char prefix[155];
} USTarHeader;

/**
 * Process the control.tar file.
 */
static struct EXTRACTOR_Keywords *
processControlTar(const char * data,
		  const size_t size,
		  struct EXTRACTOR_Keywords * prev) {
  TarHeader * tar;
  USTarHeader * ustar;
  size_t pos;

  pos = 0;
  while (pos + sizeof(TarHeader) < size) {
    unsigned long long fsize;
    char buf[13];

    tar = (TarHeader*) &data[pos];
    if (pos + sizeof(USTarHeader) < size) {
      ustar = (USTarHeader*) &data[pos];
      if (0 == strncmp("ustar",
		       &ustar->magic[0],
		       strlen("ustar")))
	pos += 512; /* sizeof(USTarHeader); */
      else
	pos += 257; /* sizeof(TarHeader); minus gcc alignment... */
    } else {
      pos += 257; /* sizeof(TarHeader); minus gcc alignment... */
    }

    memcpy(buf, &tar->filesize[0], 12);
    buf[12] = '\0';
    if (1 != sscanf(buf, "%12llo", &fsize)) /* octal! Yuck yuck! */
      return prev;
    if ( (pos + fsize > size) ||
	 (fsize > size) ||
	 (pos + fsize < pos) )
      return prev;

    if (0 == strncmp(&tar->name[0],
		     "./control",
		     strlen("./control"))) {
      return processControl(&data[pos],
			    fsize,
			    prev);
    }
    if ( (fsize & 511) != 0)
      fsize = (fsize | 511)+1; /* round up! */
    if (pos + fsize < pos)
      return prev;
    pos += fsize;
  }
  return prev;
}

#define MAX_CONTROL_SIZE (1024 * 1024)

static voidpf Emalloc(voidpf opaque, uInt items, uInt size) {
  return malloc(size * items);
}

static void Efree(voidpf opaque, voidpf ptr) {
  free(ptr);
}

/**
 * Process the control.tar.gz file.
 */
static struct EXTRACTOR_Keywords *
processControlTGZ(const unsigned char * data,
		  size_t size,
		  struct EXTRACTOR_Keywords * prev) {
  size_t bufSize;
  char * buf;
  z_stream strm;

  bufSize = data[size-4] + 256 * data[size-3] + 65536 * data[size-2] + 256*65536 * data[size-1];
  if (bufSize > MAX_CONTROL_SIZE)
    return prev;

  memset(&strm,
	 0,
	 sizeof(z_stream));

  strm.next_in = (Bytef*) data;
  strm.avail_in = size;
  strm.total_in = 0;
  strm.zalloc = &Emalloc;
  strm.zfree = &Efree;
  strm.opaque = NULL;

  if (Z_OK == inflateInit2(&strm,
			   15 + 32)) {
    buf = malloc(bufSize);
    if (buf == NULL) {
      inflateEnd(&strm);
      return prev;
    }
    strm.next_out = (Bytef*) buf;
    strm.avail_out = bufSize;
    inflate(&strm,
	    Z_FINISH);
    if (strm.total_out > 0) {
      prev = processControlTar(buf,
			       strm.total_out,
			       prev);
      inflateEnd(&strm);
      free(buf);
      return prev;
    }
    free(buf);
    inflateEnd(&strm);
  }
  return prev;
}

typedef struct {
  char name[16];
  char lastModTime [12];
  char userId[6];
  char groupId[6];
  char modeInOctal[8];
  char filesize[10];
  char trailer[2];
} ObjectHeader;

struct EXTRACTOR_Keywords *
libextractor_deb_extract(const char * filename,
			 const char * data,
			 const size_t size,
			 struct EXTRACTOR_Keywords * prev) {
  size_t pos;
  int done = 0;

  if (size < 128)
    return prev;
  if (0 != strncmp("!<arch>\n",
		   data,
		   strlen("!<arch>\n")))
    return prev;
  pos = strlen("!<arch>\n");
  while (pos + sizeof(ObjectHeader) < size) {
    ObjectHeader * hdr;
    unsigned long long fsize;
    char buf[11];

    hdr = (ObjectHeader*) &data[pos];
    if (0 != strncmp(&hdr->trailer[0],
		     "`\n",
		     2))
      return prev;

    memcpy(buf, &hdr->filesize[0], 10);
    buf[10] = '\0';
    if (1 != sscanf(buf, "%10llu", &fsize))
      return prev;
    pos += sizeof(ObjectHeader);
    if ( (pos + fsize > size) ||
	 (fsize > size) ||
	 (pos + fsize < pos) )
      return prev;
    if (0 == strncmp(&hdr->name[0],
		     "control.tar.gz",
		     strlen("control.tar.gz"))) {
      prev = processControlTGZ((const unsigned char*) &data[pos],
			       fsize,
			       prev);
      done++;
    }
    if (0 == strncmp(&hdr->name[0],
		     "debian-binary",
		     strlen("debian-binary"))) {
      prev = addKeyword(EXTRACTOR_MIMETYPE,
			strdup("application/x-debian-package"),
			prev);
      done++;
    }
    pos += fsize;
    if (done == 2)
      break; /* no need to process the rest of the archive */
  }
  return prev;
}

