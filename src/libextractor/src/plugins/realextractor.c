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

#define UINT32 unsigned int
#define UINT16 unsigned short
#define UINT8 unsigned char

typedef struct{
  UINT32     object_id;
  UINT32     size;
  UINT16     object_version; /* must be 0 */
  UINT16                      stream_number;
  UINT32                      max_bit_rate;
  UINT32                      avg_bit_rate;
  UINT32                      max_packet_size;
  UINT32                      avg_packet_size;
  UINT32                      start_time;
  UINT32                      preroll;
  UINT32                      duration;
  UINT8                       stream_name_size;
  UINT8 data[0]; /* variable length section */
  /*
    UINT8[stream_name_size]     stream_name;
    UINT8                       mime_type_size;
    UINT8[mime_type_size]       mime_type;
    UINT32                      type_specific_len;
    UINT8[type_specific_len]    type_specific_data;
  */
} Media_Properties;

typedef struct {
  UINT32     object_id;
  UINT32     size;
  UINT16      object_version; /* must be 0 */
  UINT16    title_len;
  UINT8 data[0]; /* variable length section */
  /*
    UINT8[title_len]  title;
    UINT16    author_len;
    UINT8[author_len]  author;
    UINT16    copyright_len;
    UINT8[copyright_len]  copyright;
    UINT16    comment_len;
    UINT8[comment_len]  comment;
  */
} Content_Description;
/* author, copyright and comment are supposed to be ASCII */

#define REAL_HEADER 0x2E524d46
#define MDPR_HEADER 0x4D445052
#define CONT_HEADER 0x434F4e54

#define RAFF4_HEADER 0x2E7261FD

static struct EXTRACTOR_Keywords *
addKeyword(EXTRACTOR_KeywordType type,
	   char * keyword,
	   struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}

static struct EXTRACTOR_Keywords *
processMediaProperties(const Media_Properties * prop,
		       struct EXTRACTOR_Keywords * prev) {

  UINT8 mime_type_size;
  UINT32 prop_size;
  char * data;

  prop_size = ntohl(prop->size);
  if (prop_size <= sizeof(Media_Properties))
    return prev;
  if (0 != prop->object_version)
    return prev;
  if (prop_size <= prop->stream_name_size + sizeof(UINT8)
      + sizeof(Media_Properties))
    return prev;

  mime_type_size = prop->data[prop->stream_name_size];
  if (prop_size <= prop->stream_name_size + sizeof(UINT8) +
      + mime_type_size + sizeof(Media_Properties))
    return prev;

  data = malloc(mime_type_size+1);
  memcpy(data,&prop->data[prop->stream_name_size+1],mime_type_size);
  data[mime_type_size]='\0';

  return addKeyword(EXTRACTOR_MIMETYPE,
		    data,
		    prev);
}

static struct EXTRACTOR_Keywords *
processContentDescription(const Content_Description * prop,
			  struct EXTRACTOR_Keywords * prev) {


  UINT16 author_len;
  UINT16 copyright_len;
  UINT16 comment_len;
  UINT16 title_len;
  char * title;
  char * author;
  char * copyright;
  char * comment;
  UINT32 prop_size;

  prop_size = ntohl(prop->size);
  if (prop_size <= sizeof(Content_Description))
    return prev;
  if (0 != prop->object_version)
    return prev;
  title_len = ntohs(prop->title_len);
  if (prop_size <= title_len + sizeof(UINT16)
      + sizeof(Content_Description))
    return prev;


  author_len = ntohs( *(UINT16*)&prop->data[title_len]);

  if (prop_size <= title_len + sizeof(UINT16)
      + author_len + sizeof(Content_Description))
    return prev;

  copyright_len =ntohs(  *(UINT16*)&prop->data[title_len+
					author_len+
					sizeof(UINT16)]);

  if (prop_size <= title_len + 2*sizeof(UINT16)
      + author_len + copyright_len + sizeof(Content_Description))
    return prev;

  comment_len = ntohs( *(UINT16*)&prop->data[title_len+
				      author_len+
				      copyright_len+
				      2*sizeof(UINT16)]);

  if (prop_size < title_len + 3*sizeof(UINT16)
      + author_len + copyright_len + comment_len
      + sizeof(Content_Description))
    return prev;

  title = malloc(title_len+1);
  memcpy(title,&prop->data[0],title_len);
  title[title_len]='\0';

  prev = addKeyword(EXTRACTOR_TITLE,
		    title,
		    prev);

  author = malloc(author_len+1);
  memcpy(author,&prop->data[title_len+sizeof(UINT16)],author_len);
  author[author_len]='\0';

  prev = addKeyword(EXTRACTOR_AUTHOR,
		    author,
		    prev);

  copyright=malloc(copyright_len+1);
  memcpy(copyright,
	 &prop->data[title_len + sizeof(UINT16)*2 + author_len],
	 copyright_len);
  copyright[copyright_len]='\0';


  prev = addKeyword(EXTRACTOR_COPYRIGHT,
		    copyright,
		    prev);


  comment=malloc(comment_len+1);
  memcpy(comment,
	 &prop->data[title_len + sizeof(UINT16)*3 + author_len + copyright_len],
	 comment_len);
  comment[comment_len]='\0';

  prev = addKeyword(EXTRACTOR_COMMENT,
		    comment,
		    prev);

  return prev;
}

typedef struct RAFF4_header {
  unsigned short version;
  unsigned short revision;
  unsigned short header_length;
  unsigned short compression_type;
  unsigned int granularity;
  unsigned int total_bytes;
  unsigned int bytes_per_minute;
  unsigned int bytes_per_minute2;
  unsigned short interleave_factor;
  unsigned short interleave_block_size;
  unsigned int user_data;
  float sample_rate;
  unsigned short sample_size;
  unsigned short channels;
  unsigned char interleave_code[5];
  unsigned char compression_code[5];
  unsigned char is_interleaved;
  unsigned char copy_byte;
  unsigned char stream_type;
  /*
  unsigned char tlen;
  unsigned char title[tlen];
  unsigned char alen;
  unsigned char author[alen];
  unsigned char clen;
  unsigned char copyright[clen];
  unsigned char aplen;
  unsigned char app[aplen]; */
} RAFF4_header;

#define RAFF4_HDR_SIZE 53

static char * stndup(const char * str,
                     size_t n) {
  char * tmp;
  tmp = malloc(n+1);
  tmp[n] = '\0';
  memcpy(tmp, str, n);
  return tmp;
}

/* audio/vnd.rn-realaudio */
struct EXTRACTOR_Keywords * libextractor_real_extract(unsigned char * filename,
                                                      const unsigned char * data,
                                                      size_t size,
                                                      struct EXTRACTOR_Keywords * prev) {
  const unsigned char * pos;
  const unsigned char * end;
  struct EXTRACTOR_Keywords * result;
  unsigned int length;
  const RAFF4_header * hdr;
  unsigned char tlen;
  unsigned char alen;
  unsigned char clen;
  unsigned char aplen;

  if (size <= 2*sizeof(int))
    return prev;

  if (RAFF4_HEADER == ntohl(*(int*)data)) {
    /* HELIX */
    if (size <= RAFF4_HDR_SIZE + 16 + 4)
      return prev;
    prev = addKeyword(EXTRACTOR_MIMETYPE,
		      strdup("audio/vnd.rn-realaudio"),
		      prev);
    hdr = (const RAFF4_header*) &data[16];
    if (ntohs(hdr->header_length) + 16 > size)
      return prev;
    tlen = data[16 + RAFF4_HDR_SIZE];
    if (tlen + RAFF4_HDR_SIZE + 20 > size)
      return prev;
    alen = data[17 + tlen + RAFF4_HDR_SIZE];
    if (tlen + alen + RAFF4_HDR_SIZE + 20 > size)
      return prev;
    clen = data[18 + tlen + alen + RAFF4_HDR_SIZE];
    if (tlen + alen + clen + RAFF4_HDR_SIZE + 20 > size)
      return prev;
    aplen = data[19 + tlen + clen + alen + RAFF4_HDR_SIZE];
    if (tlen + alen + clen + aplen + RAFF4_HDR_SIZE + 20 > size)
      return prev;

    if (tlen > 0)
      prev = addKeyword(EXTRACTOR_TITLE,
			stndup((const char*) &data[17 + RAFF4_HDR_SIZE],
			       tlen),
			prev);
    if (alen > 0)
      prev = addKeyword(EXTRACTOR_AUTHOR,
			stndup((const char*) &data[18 + RAFF4_HDR_SIZE + tlen],
			       alen),
			prev);
    if (clen > 0)
      prev = addKeyword(EXTRACTOR_COPYRIGHT,
			stndup((const char*) &data[19 + RAFF4_HDR_SIZE + tlen + alen],
			       clen),
			prev);
    if (aplen > 0)
      prev = addKeyword(EXTRACTOR_SOFTWARE,
			stndup((const char*) &data[20 + RAFF4_HDR_SIZE + tlen + alen + clen],
			       aplen),
			prev);
    return prev;

  }
  if (REAL_HEADER == ntohl(*(int*)data)) {
    /* old real */
    result = prev;
    end = &data[size];
    pos = &data[0];
    while(1) {
      if ( (pos+8 >= end) ||
	   (pos+8 < pos) )
	break;
      length = ntohl(*(((unsigned int*) pos)+1));
      if (length <= 0)
	break;
      if ( (pos + length >= end) ||
	   (pos + length < pos) )
      break;
      switch (ntohl(*((unsigned int*) pos))) {
      case MDPR_HEADER:
	result = processMediaProperties((Media_Properties *)pos,
					result);
	pos += length;
	break;
      case CONT_HEADER:
	result = processContentDescription((Content_Description *)pos,
					   result);
	pos += length;
	break;
      case REAL_HEADER: /* treat like default */
      default:
	pos += length;
	break;
      }
    }
    return result;
  }
  return prev;
}

