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

/**
 * Convert the given input using the given converter
 * and return as a 0-terminated string.
 */
static char * iconvHelper(iconv_t cd,
			  const char * in) {
  size_t inSize;
  char * buf;
  char * ibuf;
  const char * i;
  size_t outSize;
  size_t outLeft;

  i = in;
  /* reset iconv */
  iconv(cd, NULL, NULL, NULL, NULL);

  inSize = strlen(in);
  outSize = 4 * strlen(in) + 2;
  outLeft = outSize - 2; /* make sure we have 2 0-terminations! */
  buf = malloc(outSize);
  ibuf = buf;
  memset(buf, 0, outSize);
  if (iconv(cd,
	    (char**) &in,
	    &inSize,
	    &ibuf,
	    &outLeft) == (size_t)-1) {
    /* conversion failed */
    free(buf);
    return strdup(i);
  }
  return buf;
}
