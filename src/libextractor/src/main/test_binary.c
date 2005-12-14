/*
     This file is part of libextractor.
     (C) 2005 Vidyut Samanta and Christian Grothoff

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

static int test(const char * buf,
		size_t size) {
  char * enc;
  unsigned char * dec;
  size_t out;

  enc = EXTRACTOR_binaryEncode(buf,
			       size);
  if (0 != EXTRACTOR_binaryDecode(enc,
				  &dec,
				  &out)) {
    free(enc);
    return 0;
  }
  free(enc);
  if (out != size) {
    free(dec);
    return 0;
  }
  if (0 != memcmp(buf,
		  dec,
		  size)) {
    free(dec);
    return 0;
  }
  free(dec);
  return 1;
}

#define MAX 1024

int main(int argc,
	 char * argv[]) {
  unsigned int i;
  char buf[MAX];

  for (i=0;i<MAX;i++) {
    buf[i] = (char) rand();
    if (! test(buf, i))
      return -1;
  }
  return 0;
}
