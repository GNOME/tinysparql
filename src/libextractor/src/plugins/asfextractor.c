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

/*
 * This file is based on demux_asf from the xine project (copyright follows).
 *
 * Copyright (C) 2000-2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 * demultiplexer for asf streams
 *
 * based on ffmpeg's
 * ASF compatible encoder and decoder.
 * Copyright (c) 2000, 2001 Gerard Lantau.
 *
 * GUID list from avifile
 * some other ideas from MPlayer
 */

#include "platform.h"
#include "extractor.h"

#define CODEC_TYPE_AUDIO       0
#define CODEC_TYPE_VIDEO       1
#define CODEC_TYPE_CONTROL     2
#define MAX_NUM_STREAMS       23

#define DEFRAG_BUFSIZE    65536
#define DEMUX_START 1
#define DEMUX_FINISHED 0


/*
 * define asf GUIDs (list from avifile)
 */
#define GUID_ERROR                              0

    /* base ASF objects */
#define GUID_ASF_HEADER                         1
#define GUID_ASF_DATA                           2
#define GUID_ASF_SIMPLE_INDEX                   3

    /* header ASF objects */
#define GUID_ASF_FILE_PROPERTIES                4
#define GUID_ASF_STREAM_PROPERTIES              5
#define GUID_ASF_STREAM_BITRATE_PROPERTIES      6
#define GUID_ASF_CONTENT_DESCRIPTION            7
#define GUID_ASF_EXTENDED_CONTENT_ENCRYPTION    8
#define GUID_ASF_SCRIPT_COMMAND                 9
#define GUID_ASF_MARKER                        10
#define GUID_ASF_HEADER_EXTENSION              11
#define GUID_ASF_BITRATE_MUTUAL_EXCLUSION      12
#define GUID_ASF_CODEC_LIST                    13
#define GUID_ASF_EXTENDED_CONTENT_DESCRIPTION  14
#define GUID_ASF_ERROR_CORRECTION              15
#define GUID_ASF_PADDING                       16

    /* stream properties object stream type */
#define GUID_ASF_AUDIO_MEDIA                   17
#define GUID_ASF_VIDEO_MEDIA                   18
#define GUID_ASF_COMMAND_MEDIA                 19

    /* stream properties object error correction type */
#define GUID_ASF_NO_ERROR_CORRECTION           20
#define GUID_ASF_AUDIO_SPREAD                  21

    /* mutual exclusion object exlusion type */
#define GUID_ASF_MUTEX_BITRATE                 22
#define GUID_ASF_MUTEX_UKNOWN                  23

    /* header extension */
#define GUID_ASF_RESERVED_1                    24

    /* script command */
#define GUID_ASF_RESERVED_SCRIPT_COMMNAND      25

    /* marker object */
#define GUID_ASF_RESERVED_MARKER               26

    /* various */
/*
#define GUID_ASF_HEAD2                         27
*/
#define GUID_ASF_AUDIO_CONCEAL_NONE            27
#define GUID_ASF_CODEC_COMMENT1_HEADER         28
#define GUID_ASF_2_0_HEADER                    29

#define GUID_END                               30


/* asf stream types */
#define ASF_STREAM_TYPE_UNKNOWN  0
#define ASF_STREAM_TYPE_AUDIO    1
#define ASF_STREAM_TYPE_VIDEO    2
#define ASF_STREAM_TYPE_CONTROL  3

#define ASF_MAX_NUM_STREAMS     23


typedef unsigned long long ext_uint64_t;
typedef unsigned int ext_uint32_t ;
typedef unsigned short ext_uint16_t ;
typedef unsigned char ext_uint8_t;

typedef struct {
  ext_uint32_t v1;
  ext_uint16_t v2;
  ext_uint16_t v3;
  ext_uint8_t  v4[8];
} LE_GUID;

static const struct
{
    const char* name;
    const LE_GUID  guid;
} guids[] =
{
    { "error",
    { 0x0,} },


    /* base ASF objects */
    { "header",
    { 0x75b22630, 0x668e, 0x11cf, { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c }} },

    { "data",
    { 0x75b22636, 0x668e, 0x11cf, { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c }} },

    { "simple index",
    { 0x33000890, 0xe5b1, 0x11cf, { 0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb }} },


    /* header ASF objects */
    { "file properties",
    { 0x8cabdca1, 0xa947, 0x11cf, { 0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 }} },

    { "stream header",
    { 0xb7dc0791, 0xa9b7, 0x11cf, { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 }} },

    { "stream bitrate properties", /* (http://get.to/sdp) */
    { 0x7bf875ce, 0x468d, 0x11d1, { 0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2 }} },

    { "content description",
    { 0x75b22633, 0x668e, 0x11cf, { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c }} },

    { "extended content encryption",
    { 0x298ae614, 0x2622, 0x4c17, { 0xb9, 0x35, 0xda, 0xe0, 0x7e, 0xe9, 0x28, 0x9c }} },

    { "script command",
    { 0x1efb1a30, 0x0b62, 0x11d0, { 0xa3, 0x9b, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 }} },

    { "marker",
    { 0xf487cd01, 0xa951, 0x11cf, { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 }} },

    { "header extension",
    { 0x5fbf03b5, 0xa92e, 0x11cf, { 0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 }} },

    { "bitrate mutual exclusion",
    { 0xd6e229dc, 0x35da, 0x11d1, { 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe }} },

    { "codec list",
    { 0x86d15240, 0x311d, 0x11d0, { 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 }} },

    { "extended content description",
    { 0xd2d0a440, 0xe307, 0x11d2, { 0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50 }} },

    { "error correction",
    { 0x75b22635, 0x668e, 0x11cf, { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c }} },

    { "padding",
    { 0x1806d474, 0xcadf, 0x4509, { 0xa4, 0xba, 0x9a, 0xab, 0xcb, 0x96, 0xaa, 0xe8 }} },


    /* stream properties object stream type */
    { "audio media",
    { 0xf8699e40, 0x5b4d, 0x11cf, { 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b }} },

    { "video media",
    { 0xbc19efc0, 0x5b4d, 0x11cf, { 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b }} },

    { "command media",
    { 0x59dacfc0, 0x59e6, 0x11d0, { 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 }} },


    /* stream properties object error correction */
    { "no error correction",
    { 0x20fb5700, 0x5b55, 0x11cf, { 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b }} },

    { "audio spread",
    { 0xbfc3cd50, 0x618f, 0x11cf, { 0x8b, 0xb2, 0x00, 0xaa, 0x00, 0xb4, 0xe2, 0x20 }} },


    /* mutual exclusion object exlusion type */
    { "mutex bitrate",
    { 0xd6e22a01, 0x35da, 0x11d1, { 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe }} },

    { "mutex unknown",
    { 0xd6e22a02, 0x35da, 0x11d1, { 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe }} },


    /* header extension */
    { "reserved_1",
    { 0xabd3d211, 0xa9ba, 0x11cf, { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 }} },


    /* script command */
    { "reserved script command",
    { 0x4B1ACBE3, 0x100B, 0x11D0, { 0xA3, 0x9B, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6 }} },

    /* marker object */
    { "reserved marker",
    { 0x4CFEDB20, 0x75F6, 0x11CF, { 0x9C, 0x0F, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB }} },

    /* various */
    /* Already defined (reserved_1)
    { "head2",
    { 0xabd3d211, 0xa9ba, 0x11cf, { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 }} },
    */
    { "audio conceal none",
    { 0x49f1a440, 0x4ece, 0x11d0, { 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 }} },

    { "codec comment1 header",
    { 0x86d15241, 0x311d, 0x11d0, { 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 }} },

    { "asf 2.0 header",
    { 0xd6e229d1, 0x35da, 0x11d1, { 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe }} },

};

typedef struct {
  int               num;
  int               seq;

  int               frag_offset;
  int64_t           timestamp;
  int               ts_per_kbyte;
  int               defrag;

  ext_uint32_t          buf_type;
  int               stream_id;

  ext_uint8_t          *buffer;
} asf_stream_t;

typedef struct demux_asf_s {
  /* pointer to the stream data */
  char * input;
  /* current position in stream */
  size_t inputPos;
  size_t inputLen;

  int               keyframe_found;

  int               seqno;
  ext_uint32_t          packet_size;
  ext_uint8_t           packet_flags;
  ext_uint32_t          data_size;

  ext_uint32_t          bitrates[MAX_NUM_STREAMS];
  int               num_streams;
  int               num_audio_streams;
  int               num_video_streams;
  int               audio_stream;
  int               video_stream;
  int               audio_stream_id;
  int               video_stream_id;
  int               control_stream_id;

  ext_uint16_t          wavex[1024];
  int               wavex_size;

  char              title[512];
  char              author[512];
  char              copyright[512];
  char              comment[512];

  ext_uint32_t          length, rate;

  /* packet filling */
  int               packet_size_left;

  /* frame rate calculations, discontinuity detection */

  int64_t           last_pts[2];
  int32_t           frame_duration;
  int               send_newpts;
  int64_t           last_frame_pts;

  /* only for reading */
  ext_uint32_t          packet_padsize;
  int               nb_frames;
  ext_uint8_t           frame_flag;
  ext_uint8_t           segtype;
  int               frame;

  int               status;

  /* byte reordering from audio streams */
  int               reorder_h;
  int               reorder_w;
  int               reorder_b;

  off_t             header_size;
  int               buf_flag_seek;

  /* first packet position */
  int64_t           first_packet_pos;

  int               reference_mode;
} demux_asf_t ;

static int readBuf(demux_asf_t * this,
		   void * buf,
		   int len) {
  int min;

  min = len;
  if (this->inputLen - this->inputPos < min)
    min = this->inputLen - this->inputPos;
  memcpy(buf,
	 &this->input[this->inputPos],
	 min);
  this->inputPos += min;
  return min;
}

static ext_uint8_t get_byte (demux_asf_t *this) {
  ext_uint8_t buf;
  int     i;

  i = readBuf (this, &buf, 1);
  if (i != 1)
    this->status = DEMUX_FINISHED;
  return buf;
}

static ext_uint16_t get_le16 (demux_asf_t *this) {
  ext_uint8_t buf[2];
  int     i;

  i = readBuf (this, buf, 2);
  if (i != 2)
    this->status = DEMUX_FINISHED;
  return buf[0] | (buf[1] << 8);
}

static ext_uint32_t get_le32 (demux_asf_t *this) {
  ext_uint8_t buf[4];
  int     i;

  i = readBuf (this, buf, 4);
  if (i != 4)
    this->status = DEMUX_FINISHED;
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static ext_uint64_t get_le64 (demux_asf_t *this) {
  ext_uint8_t buf[8];
  int     i;

  i = readBuf (this, buf, 8);
  if (i != 8)
    this->status = DEMUX_FINISHED;
  return (ext_uint64_t) buf[0]
    | ((ext_uint64_t) buf[1] << 8)
    | ((ext_uint64_t) buf[2] << 16)
    | ((ext_uint64_t) buf[3] << 24)
    | ((ext_uint64_t) buf[4] << 32)
    | ((ext_uint64_t) buf[5] << 40)
    | ((ext_uint64_t) buf[6] << 48)
    | ((ext_uint64_t) buf[7] << 54) ;
}

static int get_guid (demux_asf_t *this) {
  int i;
  LE_GUID g;

  g.v1 = get_le32(this);
  g.v2 = get_le16(this);
  g.v3 = get_le16(this);
  for(i = 0; i < 8; i++)
    g.v4[i] = get_byte(this);
  if (this->status == DEMUX_FINISHED)
    return GUID_ERROR;
  for (i = 1; i < GUID_END; i++)
    if (!memcmp(&g, &guids[i].guid, sizeof(LE_GUID)))
      return i;

  return GUID_ERROR;
}

static void get_str16_nolen(demux_asf_t *this,
			    int len,
			    char *buf,
			    int buf_size) {

  int c;
  char *q;

  q = buf;
  while (len > 0) {
    c = get_le16(this);
    if ((q - buf) < buf_size - 1)
      *q++ = c;
    len-=2;
  }
  *q = '\0';
}

static int asf_read_header(demux_asf_t *this) {
  int            guid;
  ext_uint64_t       gsize;

  guid = get_guid(this);
  if (guid != GUID_ASF_HEADER)
    return 0;
  get_le64(this);
  get_le32(this);
  get_byte(this);
  get_byte(this);

  while (this->status != DEMUX_FINISHED) {
    guid  = get_guid(this);
    gsize = get_le64(this);

    if (gsize < 24)
      goto fail;

    switch (guid) {
      case GUID_ASF_FILE_PROPERTIES:
        {
          ext_uint64_t start_time, end_time;

          guid = get_guid(this);
          get_le64(this); /* file size */
          get_le64(this); /* file time */
          get_le64(this); /* nb_packets */

          end_time =  get_le64 (this);

          this->length = get_le64(this) / 10000;
          if (this->length)
            this->rate = this->inputLen / (this->length / 1000);
          else
            this->rate = 0;


          start_time = get_le32(this); /* start timestamp in 1/1000 s*/

          get_le32(this); /* unknown */
          get_le32(this); /* min size */
          this->packet_size = get_le32(this); /* max size */
          get_le32(this); /* max bitrate */
          get_le32(this);
        }
        break;

      case (GUID_ASF_STREAM_PROPERTIES):
        {
          int           type;
          ext_uint32_t      total_size, stream_data_size;
	  ext_uint16_t      stream_id;
          ext_uint64_t      pos1, pos2;
          pos1 = this->inputPos;

          guid = get_guid(this);
          switch (guid) {
            case GUID_ASF_AUDIO_MEDIA:
              type = CODEC_TYPE_AUDIO;
              break;

            case GUID_ASF_VIDEO_MEDIA:
              type = CODEC_TYPE_VIDEO;
              break;

            case GUID_ASF_COMMAND_MEDIA:
              type = CODEC_TYPE_CONTROL;
              break;

            default:
              goto fail;
          }

          guid = get_guid(this);
          get_le64(this);
          total_size = get_le32(this);
          stream_data_size = get_le32(this);
          stream_id = get_le16(this); /* stream id */
          get_le32(this);

          if (type == CODEC_TYPE_AUDIO) {
            ext_uint8_t buffer[6];

            readBuf (this, (ext_uint8_t *) this->wavex, total_size);
            if (guid == GUID_ASF_AUDIO_SPREAD) {
              readBuf (this, buffer, 6);
              this->reorder_h = buffer[0];
              this->reorder_w = (buffer[2]<<8)|buffer[1];
              this->reorder_b = (buffer[4]<<8)|buffer[3];
              this->reorder_w /= this->reorder_b;
            } else {
              this->reorder_b=this->reorder_h=this->reorder_w=1;
            }

            this->wavex_size = total_size; /* 18 + this->wavex[8]; */
            this->num_audio_streams++;
          }
          else if (type == CODEC_TYPE_VIDEO) {

            ext_uint16_t i;

            get_le32(this); /* width */
            get_le32(this); /* height */
            get_byte(this);

            i = get_le16(this); /* size */
	    this->inputPos += i;
            this->num_video_streams++;
          }
          else if (type == CODEC_TYPE_CONTROL) {
            this->control_stream_id = stream_id;
          }

          this->num_streams++;
          pos2 = this->inputPos;
          this->inputPos += gsize - (pos2 - pos1 + 24);
        }
        break;

      case GUID_ASF_DATA:
        goto headers_ok;
        break;
      case GUID_ASF_CONTENT_DESCRIPTION: {
          ext_uint16_t len1, len2, len3, len4, len5;

          len1 = get_le16(this);
          len2 = get_le16(this);
          len3 = get_le16(this);
          len4 = get_le16(this);
          len5 = get_le16(this);
          get_str16_nolen(this, len1, this->title, sizeof(this->title));
          get_str16_nolen(this, len2, this->author, sizeof(this->author));
          get_str16_nolen(this, len3, this->copyright, sizeof(this->copyright));
          get_str16_nolen(this, len4, this->comment, sizeof(this->comment));
          this->inputPos += len5;
        }
        break;

      case GUID_ASF_STREAM_BITRATE_PROPERTIES:
        {
          ext_uint16_t streams, stream_id;
          ext_uint16_t i;

          streams = get_le16(this);
          for (i = 0; i < streams; i++) {
            stream_id = get_le16(this);
            this->bitrates[stream_id] = get_le32(this);
          }
        }
        break;

      default:
        this->inputPos += gsize - 24;
    }
  }

 headers_ok:
  this->inputPos += sizeof(LE_GUID) + 10;
  this->packet_size_left = 0;
  this->first_packet_pos = this->inputPos;
  return 1;

 fail:
  return 0;
}


static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordType type,
					      char * keyword,
					      struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = strdup(keyword);
  result->keywordType = type;
  return result;
}


/* mimetypes:
   video/x-ms-asf: asf: ASF stream;
   video/x-ms-wmv: wmv: Windows Media Video;
   video/x-ms-wma: wma: Windows Media Audio;
   application/vnd.ms-asf: asf: ASF stream;
   application/x-mplayer2: asf,asx,asp: mplayer2;
   video/x-ms-asf-plugin: asf,asx,asp: mms animation;
   video/x-ms-wvx: wvx: wmv metafile;
   video/x-ms-wax: wva: wma metafile; */
struct EXTRACTOR_Keywords * libextractor_asf_extract(char * filename,
                                                     char * data,
                                                     size_t size,
                                                     struct EXTRACTOR_Keywords * prev) {
  demux_asf_t * this;

  this = malloc(sizeof(demux_asf_t));
  memset(this, 0, sizeof(demux_asf_t));
  this->input = data;
  this->inputPos = 0;
  this->inputLen = size;
  this->status = DEMUX_START;

  if (0 == asf_read_header(this)) {
    free(this);
    return prev;
  }

  if (strlen(this->title) > 0)
    prev = addKeyword(EXTRACTOR_TITLE, this->title, prev);
  if (strlen(this->author) > 0)
    prev = addKeyword(EXTRACTOR_AUTHOR, this->author, prev);
  if (strlen(this->comment) > 0)
    prev = addKeyword(EXTRACTOR_COMMENT, this->comment, prev);
  if (strlen(this->copyright) > 0)
    prev = addKeyword(EXTRACTOR_COPYRIGHT, this->copyright, prev);
  prev = addKeyword(EXTRACTOR_MIMETYPE, "video/x-ms-asf", prev);

  /* build a description from author and title */
  if (strlen(this->author) * strlen(this->title) > 0) {
    EXTRACTOR_KeywordList * keyword = malloc(sizeof(EXTRACTOR_KeywordList));
    char * word;
    int len = 3 + strlen(this->author) + strlen(this->title);

    word = malloc(len);
    word[0] = '\0';
    strcat(word, this->author);
    strcat(word,": ");
    strcat(word, this->title);
    keyword->next = prev;
    keyword->keyword = word;
    keyword->keywordType = EXTRACTOR_DESCRIPTION;
    prev = keyword;
  }
  free(this);
  return prev;
}


/*  end of asfextractor.c */
