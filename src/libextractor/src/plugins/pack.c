/*
Catlib Copyright Notice

The author of this software is Christopher Adam Telfer
Copyright (c) 1998, 1999, 2000, 2001, 2002
by Christopher Adam Telfer.  All Rights Reserved.

Permission to use, copy, modify, and distribute this software for any
purpose without fee is hereby granted, provided that the above copyright
notice, this paragraph, and the following two paragraphs appear in all
copies, modifications, and distributions.

IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE.  THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS".   THE AUTHOR HAS NO
OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

*/

#include "platform.h"
#include "pack.h"

typedef unsigned char       byte;
typedef unsigned short      half;
typedef unsigned long       word;
typedef signed char         sbyte;
typedef signed short        shalf;
typedef signed long         sword;


/*
   "bhwAcslPBHWCSL"

   Small letters: do not convert (not implemented for arrays and P)
   Captial letters: convert from network byte order to host byte order

   b - byte
   h - half-word
   w - word
   a - array (32-byte unsigned long + that many bytes)
   c - signed 8 bit value
   s - signed 16 bit value
   l - signed 32 bit value
   p - (unpack only) value is a pointer to a pointer.  Generate the buffer
       to hold the data.

   prefixing with a number K means that the argument will be an array of K
   of the arguments specified by the letter
 */

int cat_pack(void * buf,
	     const char *fmt,
	     ...) {
  va_list ap;
  word blen, val;
  int npacked;
  unsigned int nreps, i;
  byte *bp, *bytep;
  half *halfp;
  word *wordp;
  void * arr;
  struct cat_bvec *cbvp;
  char *cp;

  va_start(ap, fmt);

  npacked = 0;
  bp = (byte *)buf;

  while( *fmt ) {
    nreps = 0;

    if ( isdigit(*fmt) ) {
      /* We use cp instead of fmt to keep the 'const' qualifier of fmt */
      nreps = strtoul(fmt, &cp, 0);
      fmt = cp;
    }

    switch(*fmt) {
    case 'B':
    case 'b':
    case 'C':
    case 'c':
      if ( ! nreps ) {
          *bp++ = va_arg(ap, int);
          npacked += 1;
      } else {
	bytep = va_arg(ap, byte*);
	for ( i = 0 ; i < nreps ; ++i ) {
	  *bp++ = bytep[i];
	  npacked += 1;
	}
      }
      break;

    case 'h':
    case 's':
      if ( ! nreps ) {
	val = va_arg(ap, int);
	*bp++ = val & 0xFF;
	*bp++ = val >> 8;
	npacked += 2;
      } else {
	halfp = va_arg(ap, half*);
	for ( i = 0 ; i < nreps ; ++i ) {
	  val = halfp[i];
	  *bp++ = val & 0xFF;
	  *bp++ = val >> 8;
	  npacked += 2;
	}
      }
      break;

    case 'H':
    case 'S':
      if ( ! nreps ) {
	val = va_arg(ap, int);
	*bp++ = val >> 8;
	*bp++ = val & 0xFF;
	npacked += 2;
      } else {
	halfp = va_arg(ap, half*);
	for ( i = 0 ; i < nreps ; ++i ) {
	  val = halfp[i];
	  *bp++ = val >> 8;
	  *bp++ = val & 0xFF;
	  npacked += 2;
	}
      }
      break;

    case 'l':
    case 'w':
      if ( ! nreps ) {
	val = va_arg(ap, word);
	*bp++ = val & 0xFF;
	*bp++ = val >> 8;
	*bp++ = val >> 16;
	*bp++ = val >> 24;
	npacked += 4;
      } else {
	wordp = va_arg(ap, word*);
	for ( i = 0 ; i < nreps ; ++i ) {
	  val = wordp[i];
	  *bp++ = val & 0xFF;
	  *bp++ = val >> 8;
	  *bp++ = val >> 16;
	  *bp++ = val >> 24;
	  npacked += 4;
	}
      }
      break;

    case 'L':
    case 'W':
      if ( ! nreps ) {
	val = va_arg(ap, word);
	*bp++ = val >> 24;
	*bp++ = val >> 16;
	*bp++ = val >> 8;
	*bp++ = val & 0xFF;
	npacked += 4;
      } else {
	wordp = va_arg(ap, word*);
	for ( i = 0 ; i < nreps ; ++i ) {
	  val = wordp[i];
	  *bp++ = val >> 24;
	  *bp++ = val >> 16;
	  *bp++ = val >> 8;
	  *bp++ = val & 0xFF;
	  npacked += 4;
	}
      }
      break;

    case 'A':
      if ( ! nreps ) {
	blen = va_arg(ap, word);
	arr = va_arg(ap, void *);
	*bp++ = blen >> 24;
	*bp++ = blen >> 16;
	*bp++ = blen >> 8;
	*bp++ = blen & 0xFF;
	memmove(bp, arr, blen);
	bp += blen;
	npacked += blen + 4; /* +4 for the 32 bits of length field */
      } else {
	cbvp = va_arg(ap, struct cat_bvec*);
	for ( i = 0 ; i < nreps ; ++i ) {
	  blen = cbvp[i].len;
	  arr = cbvp[i].data;
	  *bp++ = blen >> 24;
	  *bp++ = blen >> 16;
	  *bp++ = blen >> 8;
	  *bp++ = blen & 0xFF;
	  memmove(bp, arr, blen);
	  bp += blen;
	  npacked += blen + 4; /* see above */
	}
      }
      break;

    default:
      va_end(ap);
      return -1;
    }
    ++fmt;
  }

  va_end(ap);
  return npacked;
}



int cat_unpack(const void * buf,
	       const char *fmt,
	       ...) {
  va_list ap;
  word maxlen, len, *wordp;
  void * arr;
  byte *bp, *bytep, *newbuf;
  half *halfp;
  sbyte *sbytep;
  shalf *shalfp;
  sword *swordp;
  int npacked;
  unsigned int nreps, i, isnonprefixed = 1;  /* used for 'a' types only */
  struct cat_bvec *cbvp;
  char *cp;

  bp = (byte *)buf;
  npacked = 0;

  va_start(ap, fmt);

  while ( *fmt ) {
    nreps = 1;

    if ( isdigit(*fmt) ) {
      /* We use cp instead of format to keep the 'const' qualifier of fmt */
      nreps = strtoul(fmt, &cp, 0);
      fmt = cp;
      if ( *fmt == 'a' )
	isnonprefixed = 0;
    }

    switch (*fmt) {
    case 'B':
    case 'b':
      bytep = va_arg(ap, byte*);
      for ( i = 0 ; i < nreps ; ++i ) {
	*bytep = *bp++;
	++bytep;
	npacked += 1;
      }
      break;



    case 'h':
      halfp = va_arg(ap, half*);
      for ( i = 0 ; i < nreps ; ++i ) {
	*halfp = *bp++;
	*halfp |= *bp++ << 8;
	++halfp;
	npacked += 2;
      }
      break;

    case 'H':
      halfp = va_arg(ap, half*);
      for ( i = 0 ; i < nreps ; ++i ) {
	*halfp = *bp++ << 8;
	*halfp |= *bp++;
	++halfp;
	npacked += 2;
      }
      break;


    case 'w':
      wordp = va_arg(ap, word*);
      for ( i = 0 ; i < nreps ; ++i ) {
	*wordp = *bp++;
	*wordp |= *bp++ << 8;
	*wordp |= *bp++ << 16;
	*wordp |= *bp++ << 24;
	++wordp;
	npacked += 4;
      }
      break;

    case 'W':
      wordp = va_arg(ap, word*);
      for ( i = 0 ; i < nreps ; ++i ) {
	*wordp = *bp++ << 24;
	*wordp |= *bp++ << 16;
	*wordp |= *bp++ << 8;
	*wordp |= *bp++;
	++wordp;
	npacked += 4;
      }
      break;

    case 'A':
      if ( isnonprefixed ) {
	maxlen = va_arg(ap, word);
	arr    = va_arg(ap, void *);
	
	len = *bp++ << 24;
	len |= *bp++ << 16;
	len |= *bp++ << 8;
	len |= *bp++;
	
	if ( len > maxlen )
	  return -1;
	
	memmove(arr, bp, len);
	bp += len;
	
	npacked += len;
      } else {
	cbvp = va_arg(ap, struct cat_bvec *);
	for ( i = 0 ; i < nreps ; ++i ) {
	  maxlen = cbvp->len;
	  arr    = cbvp->data;
	
	  len = *bp++ << 24;
	  len |= *bp++ << 16;
	  len |= *bp++ << 8;
	  len |= *bp++;
	
	  if ( len > maxlen )
	    return -1;
	
	  memmove(arr, bp, len);
	  cbvp->len = len;
	  bp += len;
	
	  ++cbvp;
	  npacked += len;
	}
	isnonprefixed = 1;
      }
      break;

    case 'C':	
    case 'c':
      sbytep = va_arg(ap, sbyte*);
      for ( i = 0 ; i < nreps ; ++i ) {
	*sbytep = *bp++;
	
	if ( (sizeof(sbyte) > 1) && (*sbytep & 0x80) )
	  *sbytep |= (~0) << ((sizeof(sbyte)-1) * 8);
	
	++sbytep;
	npacked += 1;
      }
      break;


    case 's':
      shalfp = va_arg(ap, shalf*);
      for ( i = 0 ; i < nreps ; ++i )	{
	*shalfp = *bp++;
	*shalfp |= *bp++ << 8;
	
	if ( (sizeof(shalf) > 2) && (*shalfp & 0x8000) )
	  *shalfp |= (~0) << ((sizeof(shalf)-2) * 8);
	
	++shalfp;
	npacked += 2;
      }
      break;

    case 'S':
      shalfp = va_arg(ap, shalf*);
      for ( i = 0 ; i < nreps ; ++i )	{
	*shalfp = *bp++ << 8;
	*shalfp |= *bp++;
	
	if ( (sizeof(shalf) > 2) && (*shalfp & 0x8000) )
	  *shalfp |= (~0) << ((sizeof(shalf)-2) * 8);
	
	++shalfp;
	npacked += 2;
      }
      break;

    case 'l':
      swordp = va_arg(ap, sword*);
      for ( i = 0 ; i < nreps ; ++i )	{
	*swordp = *bp++;
	*swordp |= *bp++ << 8;
	*swordp |= *bp++ << 16;
	*swordp |= *bp++ << 24;
	
	if ( (sizeof(swordp) > 4) && (*swordp & 0x80000000) )
	  *swordp |= (~0) << ((sizeof(sword)-4) * 8);
	
	++swordp;
	npacked += 4;
      }
      break;

    case 'L':
      swordp = va_arg(ap, sword*);
      for ( i = 0 ; i < nreps ; ++i )	{
	*swordp = *bp++ << 24;
	*swordp |= *bp++ << 16;
	*swordp |= *bp++ << 8;
	*swordp |= *bp++;
	
	if ( (sizeof(swordp) > 4) && (*swordp & 0x80000000) )
	  *swordp |= (~0) << ((sizeof(sword)-4) * 8);
	
	++swordp;
	npacked += 4;
      }
      break;

    case 'P':
      cbvp = va_arg(ap, struct cat_bvec *);
      for ( i = 0 ; i < nreps ; ++i )	{
	len = *bp++ << 24;
	len |= *bp++ << 16;
	len |= *bp++ << 8;
	len |= *bp++;
	
	newbuf = (byte *)malloc(len);
	
	if ( ! newbuf ) {
	  int j;
	  for ( j = 0 ; j < i ; j++ )
	    free(cbvp[i].data);
	  return -1;
	}
	
	memmove(newbuf, bp, len);
	cbvp[i].data = newbuf;
	cbvp[i].len  = len;
	
	bp += len;
	npacked += len;
      }
      break;

    default:
      va_end(ap);
      return -1;
    }

    ++fmt;
  }

  va_end(ap);
  return 0;
}



