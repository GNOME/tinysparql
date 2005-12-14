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

#ifndef __CAT_PACK_H
#define __CAT_PACK_H

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
 */

int cat_pack(void * buf, const char *fmt, ... );

int cat_unpack(const void * buf, const char *fmt, ... );

struct cat_bvec {
  unsigned long len;
  void * data;
};

#endif /* __CAT_PACK_H */


