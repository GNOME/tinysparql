/*
     This file is part of libextractor.
     (C) 2004 Vidyut Samanta and Christian Grothoff

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
#include "convert.h"

/**
 * Convert the len characters long character sequence
 * given in input that is in the given charset
 * to UTF-8.
 * @return the converted string (0-terminated),
 *  if conversion fails, a copy of the orignal
 *  string is returned.
 */
char * convertToUtf8(const char * input,
		     size_t len,
		     const char * charset) {
  size_t tmpSize;
  size_t finSize;
  char * tmp;
  char * ret;
  char * itmp;
  const char * i;
  iconv_t cd;

  i = input;
  cd = iconv_open("UTF-8", charset);
  if (cd == (iconv_t) -1)
    return strdup(i);
  tmpSize = 3 * len + 4;
  tmp = malloc(tmpSize);
  itmp = tmp;
  finSize = tmpSize;
  if (iconv(cd,
	    (char**) &input,
	    &len,
	    &itmp,
	    &finSize) == (size_t)-1) {
    iconv_close(cd);
    free(tmp);
    return strdup(i);
  }
  ret = malloc(tmpSize - finSize + 1);
  memcpy(ret,
	 tmp,
	 tmpSize - finSize);
  ret[tmpSize - finSize] = '\0';
  free(tmp);
  iconv_close(cd);
  return ret;
}

/* end of convert.c */
