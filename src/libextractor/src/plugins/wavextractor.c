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

     This code was based on bitcollider 0.6.0
     (PD) 2004 The Bitzi Corporation
     http://bitzi.com/
     (PD) 2001 The Bitzi Corporation
     Please see file COPYING or http://bitzi.com/publicdomain
     for more info.
*/


#include "platform.h"
#include "extractor.h"
#include "pack.h"

static void addKeyword(struct EXTRACTOR_Keywords ** list,
		       char * keyword,
		       EXTRACTOR_KeywordType type) {
  EXTRACTOR_KeywordList * next;
  next = malloc(sizeof(EXTRACTOR_KeywordList));
  next->next = *list;
  next->keyword = keyword;
  next->keywordType = type;
  *list = next;
}

#if BIG_ENDIAN_HOST
static short toLittleEndian16(short in) {
  char *ptr = (char *)&in;

  return ((ptr[1] & 0xFF) << 8) | (ptr[0] & 0xFF);
}

static unsigned int toLittleEndian32(unsigned int in) {
  char *ptr = (char *)&in;

  return ((ptr[3] & 0xFF) << 24) | ((ptr[2] & 0xFF) << 16) | ((ptr[1] & 0xFF) << 8) | (ptr[0] & 0xFF);
}
#endif


/*
  16      4 bytes  0x00000010     // Length of the fmt data (16 bytes)
  20      2 bytes  0x0001         // Format tag: 1 = PCM
  22      2 bytes  <channels>     // Channels: 1 = mono, 2 = stereo
  24      4 bytes  <sample rate>  // Samples per second: e.g., 44100
*/
struct EXTRACTOR_Keywords * libextractor_wav_extract(char * filename,
						     const unsigned char * buf,
						     size_t bufLen,
						     struct EXTRACTOR_Keywords * prev) {
  unsigned short channels;
  unsigned short sampleSize;
  unsigned int sampleRate;
  unsigned int dataLen;
  unsigned int samples;
  char * scratch;


  if ( (bufLen < 44) ||
       (buf[0] != 'R' || buf[1] != 'I' ||
	buf[2] != 'F' || buf[3] != 'F' ||
	buf[8] != 'W' || buf[9] != 'A' ||
	buf[10] != 'V' || buf[11] != 'E' ||
	buf[12] != 'f' || buf[13] != 'm' ||
	buf[14] != 't' || buf[15] != ' ') )
    return prev; /* not a WAV file */

  channels = *((unsigned short *)&buf[22]);
  sampleRate = *((unsigned int *)&buf[24]);
  sampleSize = *((unsigned short *)&buf[34]);
  dataLen = *((unsigned int *)&buf[40]);

#if BIG_ENDIAN_HOST
  channels = toLittleEndian16(channels);
  sampleSize = toLittleEndian16(sampleSize);
  sampleRate = toLittleEndian32(sampleRate);
  dataLen = toLittleEndian32(dataLen);
#endif

  if (sampleSize != 8 && sampleSize != 16)
    return prev; /* invalid sample size found in wav file */
  if (channels == 0)
    return prev; /* invalid channels value -- avoid division by 0! */
  samples = dataLen / (channels * (sampleSize >> 3));

  scratch = malloc(256);
  snprintf(scratch,
	   256,
	   "%u ms, %d Hz, %s",
	   (samples < sampleRate)
	   ? (samples * 1000 / sampleRate)
	   : (samples / sampleRate) * 1000,
	   sampleRate,
	   channels == 1 ?
	   _("mono") : _("stereo"));
  addKeyword(&prev,
	     scratch,
	     EXTRACTOR_FORMAT);
  addKeyword(&prev,
	     strdup("audio/x-wav"),
	     EXTRACTOR_MIMETYPE);
  return prev;
}
