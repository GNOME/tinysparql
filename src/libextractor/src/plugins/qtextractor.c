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
 * Copyright (C) 2001-2003 the xine project
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
 * Quicktime File Demuxer by Mike Melanson (melanson@pcisys.net)
 *  based on a Quicktime parsing experiment entitled 'lazyqt'
 *
 * Ideally, more documentation is forthcoming, but in the meantime:
 * functional flow:
 *  create_qt_info
 *  open_qt_file
 *   parse_moov_atom
 *    parse_mvhd_atom
 *    parse_trak_atom
 *    build_frame_table
 *  free_qt_info
 *
 * $Id$
 *
 */

#include "platform.h"
#include "extractor.h"

#include <zlib.h>

typedef unsigned int qt_atom;

typedef unsigned long long ext_uint64_t;
typedef unsigned int ext_uint32_t ;
typedef unsigned char ext_uint8_t;

#define QT_ATOM( ch0, ch1, ch2, ch3 ) \
        ( (unsigned char)(ch3) | \
        ( (unsigned char)(ch2) << 8 ) | \
        ( (unsigned char)(ch1) << 16 ) | \
        ( (unsigned char)(ch0) << 24 ) )

#define BE_16(x)  ((((ext_uint8_t*)(x))[0] << 8) | ((ext_uint8_t*)(x))[1])

#define BE_32(x)  ((((ext_uint8_t*)(x))[0] << 24) | \
                   (((ext_uint8_t*)(x))[1] << 16) | \
                   (((ext_uint8_t*)(x))[2] << 8) | \
                    ((ext_uint8_t*)(x))[3])

#define LE_16(x)  ((((ext_uint8_t*)(x))[1] << 8) | ((ext_uint8_t*)(x))[0])
#define LE_32(x)  ((((ext_uint8_t*)(x))[3] << 24) | \
                   (((ext_uint8_t*)(x))[2] << 16) | \
                   (((ext_uint8_t*)(x))[1] << 8) | \
                    ((ext_uint8_t*)(x))[0])

#ifdef WORDS_BIGENDIAN

#define ME_16(x) BE_16(x)
#define ME_32(x) BE_32(x)
#else
#define ME_16(x) LE_16(x)
#define ME_32(x) LE_32(x)

#endif


/* top level atoms */
#define FREE_ATOM QT_ATOM('f', 'r', 'e', 'e')
#define JUNK_ATOM QT_ATOM('j', 'u', 'n', 'k')
#define MDAT_ATOM QT_ATOM('m', 'd', 'a', 't')
#define MOOV_ATOM QT_ATOM('m', 'o', 'o', 'v')
#define PNOT_ATOM QT_ATOM('p', 'n', 'o', 't')
#define SKIP_ATOM QT_ATOM('s', 'k', 'i', 'p')
#define WIDE_ATOM QT_ATOM('w', 'i', 'd', 'e')
#define PICT_ATOM QT_ATOM('P', 'I', 'C', 'T')
#define FTYP_ATOM QT_ATOM('f', 't', 'y', 'p')

#define CMOV_ATOM QT_ATOM('c', 'm', 'o', 'v')

#define MVHD_ATOM QT_ATOM('m', 'v', 'h', 'd')

#define VMHD_ATOM QT_ATOM('v', 'm', 'h', 'd')
#define SMHD_ATOM QT_ATOM('s', 'm', 'h', 'd')

#define TRAK_ATOM QT_ATOM('t', 'r', 'a', 'k')
#define TKHD_ATOM QT_ATOM('t', 'k', 'h', 'd')
#define MDHD_ATOM QT_ATOM('m', 'd', 'h', 'd')
#define ELST_ATOM QT_ATOM('e', 'l', 's', 't')

/* atoms in a sample table */
#define STSD_ATOM QT_ATOM('s', 't', 's', 'd')
#define STSZ_ATOM QT_ATOM('s', 't', 's', 'z')
#define STSC_ATOM QT_ATOM('s', 't', 's', 'c')
#define STCO_ATOM QT_ATOM('s', 't', 'c', 'o')
#define STTS_ATOM QT_ATOM('s', 't', 't', 's')
#define STSS_ATOM QT_ATOM('s', 't', 's', 's')
#define CO64_ATOM QT_ATOM('c', 'o', '6', '4')

#define ESDS_ATOM QT_ATOM('e', 's', 'd', 's')
#define WAVE_ATOM QT_ATOM('w', 'a', 'v', 'e')

#define IMA4_FOURCC QT_ATOM('i', 'm', 'a', '4')
#define MP4A_FOURCC QT_ATOM('m', 'p', '4', 'a')
#define TWOS_FOURCC QT_ATOM('t', 'w', 'o', 's')
#define SOWT_FOURCC QT_ATOM('s', 'o', 'w', 't')

#define UDTA_ATOM QT_ATOM('u', 'd', 't', 'a')
#define CPY_ATOM QT_ATOM(0xA9, 'c', 'p', 'y')
#define NAM_ATOM QT_ATOM(0xA9, 'n', 'a', 'm')
#define DES_ATOM QT_ATOM(0xA9, 'd', 'e', 's')
#define CMT_ATOM QT_ATOM(0xA9, 'c', 'm', 't')

#define RMDA_ATOM QT_ATOM('r', 'm', 'd', 'a')
#define RDRF_ATOM QT_ATOM('r', 'd', 'r', 'f')
#define RMDR_ATOM QT_ATOM('r', 'm', 'd', 'r')
#define RMVC_ATOM QT_ATOM('r', 'm', 'v', 'c')
#define QTIM_ATOM QT_ATOM('q', 't', 'i', 'm')

/* placeholder for cutting and pasting */
#define _ATOM QT_ATOM('', '', '', '')

#define ATOM_PREAMBLE_SIZE 8
#define PALETTE_COUNT 256

#define MAX_PTS_DIFF 100000


const int64_t bandwidths[]={14400,19200,28800,33600,34430,57600,
                            115200,262200,393216,524300,1544000,10485800};

/* these are things that can go wrong */
typedef enum {
  QT_OK,
  QT_FILE_READ_ERROR,
  QT_NO_MEMORY,
  QT_NOT_A_VALID_FILE,
  QT_NO_MOOV_ATOM,
  QT_NO_ZLIB,
  QT_ZLIB_ERROR,
  QT_HEADER_TROUBLE
} qt_error;

/* there are other types but these are the ones we usually care about */
typedef enum {

  MEDIA_AUDIO,
  MEDIA_VIDEO,
  MEDIA_OTHER

} media_type;

typedef struct {
  int64_t offset;
  unsigned int size;
  int64_t pts;
  int keyframe;
} qt_frame;

typedef struct {
  unsigned int track_duration;
  unsigned int media_time;
} edit_list_table_t;

typedef struct {
  unsigned int first_chunk;
  unsigned int samples_per_chunk;
} sample_to_chunk_table_t;

typedef struct {
  unsigned int count;
  unsigned int duration;
} time_to_sample_table_t;

typedef struct {
  char *url;
  int64_t data_rate;
  int qtim_version;
} reference_t;

typedef struct {

  /* trak description */
  media_type type;
  union {

    struct {
      unsigned int codec_fourcc;
      unsigned int codec_buftype;
      unsigned int width;
      unsigned int height;
      int palette_count;
      int depth;
      int edit_list_compensation;  /* special trick for edit lists */
    } video;

    struct {
      unsigned int codec_fourcc;
      unsigned int codec_buftype;
      unsigned int sample_rate;
      unsigned int channels;
      unsigned int bits;
      unsigned int vbr;
      unsigned int wave_present;


      /* special audio parameters */
      unsigned int samples_per_packet;
      unsigned int bytes_per_packet;
      unsigned int bytes_per_frame;
      unsigned int bytes_per_sample;
      unsigned int samples_per_frame;
    } audio;

  } properties;

  /* internal frame table corresponding to this trak */
  qt_frame *frames;
  unsigned int frame_count;
  unsigned int current_frame;

  /* trak timescale */
  unsigned int timescale;

  /* flags that indicate how a trak is supposed to be used */
  unsigned int flags;

  /* decoder data pass information to the AAC decoder */
  void *decoder_config;
  int decoder_config_len;

  /* verbatim copy of the stsd atom */
  int          stsd_size;
  void        *stsd;

  /****************************************/
  /* temporary tables for loading a chunk */

  /* edit list table */
  unsigned int edit_list_count;
  edit_list_table_t *edit_list_table;

  /* chunk offsets */
  unsigned int chunk_offset_count;
  int64_t *chunk_offset_table;

  /* sample sizes */
  unsigned int sample_size;
  unsigned int sample_size_count;
  unsigned int *sample_size_table;

  /* sync samples, a.k.a., keyframes */
  unsigned int sync_sample_count;
  unsigned int *sync_sample_table;

  /* sample to chunk table */
  unsigned int sample_to_chunk_count;
  sample_to_chunk_table_t *sample_to_chunk_table;

  /* time to sample table */
  unsigned int time_to_sample_count;
  time_to_sample_table_t *time_to_sample_table;

} qt_trak;

typedef struct {
  char * input;
  size_t inputPos;
  size_t inputLen;


  int compressed_header;  /* 1 if there was a compressed moov; just FYI */

  unsigned int creation_time;  /* in ms since Jan-01-1904 */
  unsigned int modification_time;
  unsigned int timescale;  /* base clock frequency is Hz */
  unsigned int duration;

  int64_t moov_first_offset;

  int               trak_count;
  qt_trak          *traks;

  /* the trak numbers that won their respective frame count competitions */
  int               video_trak;
  int               audio_trak;
  int seek_flag;  /* this is set to indicate that a seek has just occurred */

  char              *copyright;
  char              *name;
  char              *description;
  char              *comment;

  /* a QT movie may contain a number of references pointing to URLs */
  reference_t       *references;
  int                reference_count;
  int                chosen_reference;

  qt_error last_error;
} qt_info;

static int readBuf(qt_info * this,
		   void * buf,
		   int len) {
  int min;

  min = len;
  if ( (this->inputLen < this->inputPos) ||
       (this->inputPos < 0) )
    return -1; /* invalid pos/len */
  if (this->inputLen - this->inputPos < min)
    min = this->inputLen - this->inputPos;
  memcpy(buf,
	 &this->input[this->inputPos],
	 min);
  this->inputPos += min;
  return min;
}

/**********************************************************************
 * lazyqt special debugging functions
 **********************************************************************/

/* define DEBUG_ATOM_LOAD as 1 to get a verbose parsing of the relevant
 * atoms */
#define DEBUG_ATOM_LOAD 0

/* define DEBUG_EDIT_LIST as 1 to get a detailed look at how the demuxer is
 * handling edit lists */
#define DEBUG_EDIT_LIST 0

/* define DEBUG_FRAME_TABLE as 1 to dump the complete frame table that the
 * demuxer plans to use during file playback */
#define DEBUG_FRAME_TABLE 0

/* define DEBUG_VIDEO_DEMUX as 1 to see details about the video chunks the
 * demuxer is sending off to the video decoder */
#define DEBUG_VIDEO_DEMUX 0

/* define DEBUG_AUDIO_DEMUX as 1 to see details about the audio chunks the
 * demuxer is sending off to the audio decoder */
#define DEBUG_AUDIO_DEMUX 0

/* Define DEBUG_DUMP_MOOV as 1 to dump the raw moov atom to disk. This is
 * particularly useful in debugging a file with a compressed moov (cmov)
 * atom. The atom will be dumped to the filename specified as
 * RAW_MOOV_FILENAME. */
#define DEBUG_DUMP_MOOV 0
#define RAW_MOOV_FILENAME "moovatom.raw"

#if DEBUG_ATOM_LOAD
#define debug_atom_load printf
#else
static inline void debug_atom_load(const char *format, ...) { }
#endif

#if DEBUG_EDIT_LIST
#define debug_edit_list printf
#else
static inline void debug_edit_list(const char *format, ...) { }
#endif

#if DEBUG_FRAME_TABLE
#define debug_frame_table printf
#else
static inline void debug_frame_table(const char *format, ...) { }
#endif

#if DEBUG_VIDEO_DEMUX
#define debug_video_demux printf
#else
static inline void debug_video_demux(const char *format, ...) { }
#endif

#if DEBUG_AUDIO_DEMUX
#define debug_audio_demux printf
#else
static inline void debug_audio_demux(const char *format, ...) { }
#endif

static inline void dump_moov_atom(unsigned char *moov_atom, int moov_atom_size) {
#if DEBUG_DUMP_MOOV

  FILE *f;

  f = FOPEN(RAW_MOOV_FILENAME, "w");
  if (!f) {
    perror(RAW_MOOV_FILENAME);
    return;
  }

  if (GN_FWRITE(moov_atom, moov_atom_size, 1, f) != 1)
    printf ("  qt debug: could not write moov atom to disk\n");

  fclose(f);

#endif
}

/**********************************************************************
 * lazyqt functions
 **********************************************************************/

/*
 * This function traverses a file and looks for a moov atom. Returns the
 * file offset of the beginning of the moov atom (that means the offset
 * of the 4-byte length preceding the characters 'moov'). Returns -1
 * if no moov atom was found.
 *
 * Note: Do not count on the input stream being positioned anywhere in
 * particular when this function is finished.
 */
static void find_moov_atom(qt_info *input, off_t *moov_offset,
  int64_t *moov_size) {

  off_t atom_size;
  qt_atom atom;
  unsigned char atom_preamble[ATOM_PREAMBLE_SIZE];

  /* init the passed variables */
  *moov_offset = *moov_size = -1;

  /* take it from the top */
  input->inputPos = 0;

  /* traverse through the input */
  while (*moov_offset == -1) {
    if (readBuf(input, atom_preamble, ATOM_PREAMBLE_SIZE) !=
      ATOM_PREAMBLE_SIZE)
      break;

    atom_size = BE_32(&atom_preamble[0]);
    atom = BE_32(&atom_preamble[4]);

    /* if the moov atom is found, log the position and break from the loop */
    if (atom == MOOV_ATOM) {
      *moov_offset = input->inputPos - ATOM_PREAMBLE_SIZE;
      *moov_size = atom_size;
      break;
    }

    /* special case alert: 'free' atoms are known to contain 'cmov' atoms.
     * If this is a free atom, check for cmov immediately following.
     * QT Player can handle it, so xine should too. */
    if (atom == FREE_ATOM) {

      /* get the next atom preamble */
      if (readBuf(input, atom_preamble, ATOM_PREAMBLE_SIZE) !=
        ATOM_PREAMBLE_SIZE)
        break;

      /* if there is a cmov, qualify this free atom as the moov atom */
      if (BE_32(&atom_preamble[4]) == CMOV_ATOM) {
        /* pos = current pos minus 2 atom preambles */
        *moov_offset = input->inputPos - ATOM_PREAMBLE_SIZE * 2;
        *moov_size = atom_size;
        break;
      } else {
        /* otherwise, rewind the stream */
        input->inputPos -= ATOM_PREAMBLE_SIZE;
      }
    }

    /* if this atom is not the moov atom, make sure that it is at least one
     * of the other top-level QT atom */
    if ((atom != FREE_ATOM) &&
        (atom != JUNK_ATOM) &&
        (atom != MDAT_ATOM) &&
        (atom != PNOT_ATOM) &&
        (atom != SKIP_ATOM) &&
        (atom != WIDE_ATOM) &&
        (atom != PICT_ATOM) &&
        (atom != FTYP_ATOM))
      break;

    /* 64-bit length special case */
    if (atom_size == 1) {
      if (readBuf(input, atom_preamble, ATOM_PREAMBLE_SIZE) !=
        ATOM_PREAMBLE_SIZE)
        break;

      atom_size = BE_32(&atom_preamble[0]);
      atom_size <<= 16; /* <<= 32 causes compiler warning if we */
      atom_size <<= 16; /* are not running on 64 bit. */
      atom_size |= BE_32(&atom_preamble[4]);
      atom_size -= ATOM_PREAMBLE_SIZE * 2;
    } else
      atom_size -= ATOM_PREAMBLE_SIZE;

    input->inputPos += atom_size;
  }

  /* reset to the start of the stream on the way out */
  input->inputPos = 0;
}

/* create a qt_info structure or return NULL if no memory */
static qt_info *create_qt_info(void) {
  qt_info *info;

  info = (qt_info *)malloc(sizeof(qt_info));

  if (!info)
    return NULL;

  info->compressed_header = 0;

  info->creation_time = 0;
  info->modification_time = 0;
  info->timescale = 0;
  info->duration = 0;

  info->trak_count = 0;
  info->traks = NULL;

  info->video_trak = -1;
  info->audio_trak = -1;

  info->copyright = NULL;
  info->name = NULL;
  info->description = NULL;
  info->comment = NULL;

  info->references = NULL;
  info->reference_count = 0;
  info->chosen_reference = -1;

  info->last_error = QT_OK;

  return info;
}

/* release a qt_info structure and associated data */
static void free_qt_info(qt_info *info) {

  int i;

  if (info) {
    if (info->traks) {
      for (i = 0; i < info->trak_count; i++) {
        free(info->traks[i].frames);
        free(info->traks[i].edit_list_table);
        free(info->traks[i].chunk_offset_table);
        /* this pointer might have been set to -1 as a special case */
        if (info->traks[i].sample_size_table != (void *)-1)
          free(info->traks[i].sample_size_table);
        free(info->traks[i].sync_sample_table);
        free(info->traks[i].sample_to_chunk_table);
        free(info->traks[i].time_to_sample_table);
        free(info->traks[i].decoder_config);
        free(info->traks[i].stsd);
      }
      free(info->traks);
    }
    if (info->references) {
      for (i = 0; i < info->reference_count; i++)
        free(info->references[i].url);
      free(info->references);
    }
    if (info->copyright != NULL)
      free(info->copyright);
    if (info->name != NULL)
      free(info->name);
    if (info->description != NULL)
      free(info->description);
    if (info->comment != NULL)
      free(info->comment);
    free(info);
    info = NULL;
  }
}

/* fetch interesting information from the movie header atom */
static void parse_mvhd_atom(qt_info *info, unsigned char *mvhd_atom) {

  info->creation_time = BE_32(&mvhd_atom[0x0C]);
  info->modification_time = BE_32(&mvhd_atom[0x10]);
  info->timescale = BE_32(&mvhd_atom[0x14]);
  info->duration = BE_32(&mvhd_atom[0x18]);

}

/* helper function from mplayer's parse_mp4.c */
static int mp4_read_descr_len(unsigned char *s, ext_uint32_t *length) {
  ext_uint8_t b;
  ext_uint8_t numBytes = 0;

  *length = 0;

  do {
    b = *s++;
    numBytes++;
    *length = (*length << 7) | (b & 0x7F);
  } while ((b & 0x80) && numBytes < 4);

  return numBytes;
}

/*
 * This function traverses through a trak atom searching for the sample
 * table atoms, which it loads into an internal trak structure.
 */
static qt_error parse_trak_atom (qt_trak *trak,
				 unsigned char *trak_atom) {

  int i, j;
  unsigned int trak_atom_size = BE_32(&trak_atom[0]);
  qt_atom current_atom;
  unsigned int current_atom_size;
  qt_error last_error = QT_OK;

  /* for palette traversal */
  int color_depth;
  int color_flag;
  int color_start;
  int color_count;
  int color_end;
  int color_index;
  int color_dec;
  int color_greyscale;

  /* initialize trak structure */
  trak->edit_list_count = 0;
  trak->edit_list_table = NULL;
  trak->chunk_offset_count = 0;
  trak->chunk_offset_table = NULL;
  trak->sample_size = 0;
  trak->sample_size_count = 0;
  trak->sample_size_table = NULL;
  trak->sync_sample_table = 0;
  trak->sync_sample_table = NULL;
  trak->sample_to_chunk_count = 0;
  trak->sample_to_chunk_table = NULL;
  trak->time_to_sample_count = 0;
  trak->time_to_sample_table = NULL;
  trak->frames = NULL;
  trak->frame_count = 0;
  trak->current_frame = 0;
  trak->timescale = 0;
  trak->flags = 0;
  trak->decoder_config = NULL;
  trak->decoder_config_len = 0;
  trak->stsd = NULL;
  trak->stsd_size = 0;
  memset(&trak->properties, 0, sizeof(trak->properties));

  /* default type */
  trak->type = MEDIA_OTHER;

  /* search for media type atoms */
  for (i = ATOM_PREAMBLE_SIZE; i < trak_atom_size - 4; i++) {
    current_atom = BE_32(&trak_atom[i]);

    if (current_atom == VMHD_ATOM) {
      trak->type = MEDIA_VIDEO;
      break;
    } else if (current_atom == SMHD_ATOM) {
      trak->type = MEDIA_AUDIO;
      break;
    }
  }

  debug_atom_load("  qt: parsing %s trak atom\n",
    (trak->type == MEDIA_VIDEO) ? "video" :
      (trak->type == MEDIA_AUDIO) ? "audio" : "other");

  /* search for the useful atoms */
  for (i = ATOM_PREAMBLE_SIZE; i < trak_atom_size - 4; i++) {
    current_atom_size = BE_32(&trak_atom[i - 4]);	
    current_atom = BE_32(&trak_atom[i]);

    if (current_atom == TKHD_ATOM) {
      trak->flags = BE_16(&trak_atom[i + 6]);

      if (trak->type == MEDIA_VIDEO) {
        /* fetch display parameters */
        if( !trak->properties.video.width ||
            !trak->properties.video.height ) {

          trak->properties.video.width =
            BE_16(&trak_atom[i + 0x50]);
          trak->properties.video.height =
            BE_16(&trak_atom[i + 0x54]);
        }
      }
    } else if (current_atom == ELST_ATOM) {

      /* there should only be one edit list table */
      if (trak->edit_list_table) {
        last_error = QT_HEADER_TROUBLE;
        goto free_trak;
      }

      trak->edit_list_count = BE_32(&trak_atom[i + 8]);

      debug_atom_load("    qt elst atom (edit list atom): %d entries\n",
        trak->edit_list_count);

      trak->edit_list_table = (edit_list_table_t *)malloc(
        trak->edit_list_count * sizeof(edit_list_table_t));
      if (!trak->edit_list_table) {
        last_error = QT_NO_MEMORY;
        goto free_trak;
      }

      /* load the edit list table */
      for (j = 0; j < trak->edit_list_count; j++) {
        trak->edit_list_table[j].track_duration =
          BE_32(&trak_atom[i + 12 + j * 12 + 0]);
        trak->edit_list_table[j].media_time =
          BE_32(&trak_atom[i + 12 + j * 12 + 4]);
        debug_atom_load("      %d: track duration = %d, media time = %d\n",
          j,
          trak->edit_list_table[j].track_duration,
          trak->edit_list_table[j].media_time);
      }

    } else if (current_atom == MDHD_ATOM)
      trak->timescale = BE_32(&trak_atom[i + 0x10]);
    else if (current_atom == STSD_ATOM) {

      int hack_adjust;

      debug_atom_load ("demux_qt: stsd atom\n");

      /* copy whole stsd atom so it can later be sent to the decoder */

      trak->stsd_size = current_atom_size;
      trak->stsd = realloc (trak->stsd, current_atom_size);
      memset (trak->stsd, 0, trak->stsd_size);

      /* awful, awful hack to support a certain type of stsd atom that
       * contains more than 1 video description atom */
      if (BE_32(&trak_atom[i + 8]) == 1) {
        /* normal case */
        memcpy (trak->stsd, &trak_atom[i], current_atom_size);
        hack_adjust = 0;
      } else {
        /* pathological case; take this route until a more definite
         * solution is found: jump over the first atom video
         * description atom */

        /* copy the first 12 bytes since those remain the same */
        memcpy (trak->stsd, &trak_atom[i], 12);

        /* skip to the second atom and copy it */
        hack_adjust = BE_32(&trak_atom[i + 0x0C]);
        memcpy(trak->stsd + 12, &trak_atom[i + 0x0C + hack_adjust],
          BE_32(&trak_atom[i + 0x0C + hack_adjust]));

        /* use this variable to reference into the second atom, and
         * fix at the end of the stsd parser */
        i += hack_adjust;
      }

      if (trak->type == MEDIA_VIDEO) {

        /* initialize to sane values */
        trak->properties.video.width = 0;
        trak->properties.video.height = 0;
        trak->properties.video.depth = 0;

        /* assume no palette at first */
        trak->properties.video.palette_count = 0;

        /* fetch video parameters */
        if( BE_16(&trak_atom[i + 0x2C]) &&
            BE_16(&trak_atom[i + 0x2E]) ) {
          trak->properties.video.width =
            BE_16(&trak_atom[i + 0x2C]);
          trak->properties.video.height =
            BE_16(&trak_atom[i + 0x2E]);
        }
        trak->properties.video.codec_fourcc =
          ME_32(&trak_atom[i + 0x10]);

        /* figure out the palette situation */
        color_depth = trak_atom[i + 0x5F];
        trak->properties.video.depth = color_depth;
        color_greyscale = color_depth & 0x20;
        color_depth &= 0x1F;

        /* if the depth is 2, 4, or 8 bpp, file is palettized */
        if ((color_depth == 2) || (color_depth == 4) || (color_depth == 8)) {

          color_flag = BE_16(&trak_atom[i + 0x60]);

          if (color_greyscale) {

            trak->properties.video.palette_count =
              1 << color_depth;

            /* compute the greyscale palette */
            color_index = 255;
            color_dec = 256 /
              (trak->properties.video.palette_count - 1);
            for (j = 0;
                 j < trak->properties.video.palette_count;
                 j++) {

              color_index -= color_dec;
              if (color_index < 0)
                color_index = 0;
            }

          } else if (color_flag & 0x08) {

            /* if flag bit 3 is set, load the default palette */
            trak->properties.video.palette_count =
              1 << color_depth;

          } else {

            /* load the palette from the file */
            color_start = BE_32(&trak_atom[i + 0x62]);
            color_count = BE_16(&trak_atom[i + 0x66]);
            color_end = BE_16(&trak_atom[i + 0x68]);
            trak->properties.video.palette_count =
              color_end + 1;

            for (j = color_start; j <= color_end; j++) {

              color_index = BE_16(&trak_atom[i + 0x6A + j * 8]);
              if (color_count & 0x8000)
                color_index = j;
            }
          }
        } else
          trak->properties.video.palette_count = 0;

        debug_atom_load("    video description\n");
        debug_atom_load("      %dx%d, video fourcc = '%c%c%c%c' (%02X%02X%02X%02X)\n",
          trak->properties.video.width,
          trak->properties.video.height,
          trak_atom[i + 0x10],
          trak_atom[i + 0x11],
          trak_atom[i + 0x12],
          trak_atom[i + 0x13],
          trak_atom[i + 0x10],
          trak_atom[i + 0x11],
          trak_atom[i + 0x12],
          trak_atom[i + 0x13]);
        debug_atom_load("      %d RGB colors\n",
          trak->properties.video.palette_count);

      } else if (trak->type == MEDIA_AUDIO) {

        /* fetch audio parameters */
        trak->properties.audio.codec_fourcc =
	  ME_32(&trak_atom[i + 0x10]);
        trak->properties.audio.sample_rate =
          BE_16(&trak_atom[i + 0x2C]);
        trak->properties.audio.channels = trak_atom[i + 0x25];
        trak->properties.audio.bits = trak_atom[i + 0x27];

        /* assume uncompressed audio parameters */
        trak->properties.audio.bytes_per_sample =
          trak->properties.audio.bits / 8;
        trak->properties.audio.samples_per_frame =
          trak->properties.audio.channels;
        trak->properties.audio.bytes_per_frame =
          trak->properties.audio.bytes_per_sample *
          trak->properties.audio.samples_per_frame;
        trak->properties.audio.samples_per_packet =
          trak->properties.audio.samples_per_frame;
        trak->properties.audio.bytes_per_packet =
          trak->properties.audio.bytes_per_sample;

        /* special case time: some ima4-encoded files don't have the
         * extra header; compensate */
        if (BE_32(&trak_atom[i + 0x10]) == IMA4_FOURCC) {
          trak->properties.audio.samples_per_packet = 64;
          trak->properties.audio.bytes_per_packet = 34;
          trak->properties.audio.bytes_per_frame = 34 *
            trak->properties.audio.channels;
          trak->properties.audio.bytes_per_sample = 2;
          trak->properties.audio.samples_per_frame = 64 *
            trak->properties.audio.channels;
        }

        /* it's time to dig a little deeper to determine the real audio
         * properties; if a the stsd compressor atom has 0x24 bytes, it
         * appears to be a handler for uncompressed data; if there are an
         * extra 0x10 bytes, there are some more useful decoding params */
        if (BE_32(&trak_atom[i + 0x0C]) > 0x24) {

          if (BE_32(&trak_atom[i + 0x30]))
            trak->properties.audio.samples_per_packet =
              BE_32(&trak_atom[i + 0x30]);
          if (BE_32(&trak_atom[i + 0x34]))
            trak->properties.audio.bytes_per_packet =
              BE_32(&trak_atom[i + 0x34]);
          if (BE_32(&trak_atom[i + 0x38]))
            trak->properties.audio.bytes_per_frame =
              BE_32(&trak_atom[i + 0x38]);
          if (BE_32(&trak_atom[i + 0x3C]))
            trak->properties.audio.bytes_per_sample =
              BE_32(&trak_atom[i + 0x3C]);
          trak->properties.audio.samples_per_frame =
            (trak->properties.audio.bytes_per_frame /
             trak->properties.audio.bytes_per_packet) *
             trak->properties.audio.samples_per_packet;

        }

        /* see if the trak deserves a promotion to VBR */
        if (BE_16(&trak_atom[i + 0x28]) == 0xFFFE)
          trak->properties.audio.vbr = 1;
        else
          trak->properties.audio.vbr = 0;

        /* if this is MP4 audio, mark the trak as VBR */
        if (BE_32(&trak_atom[i + 0x10]) == MP4A_FOURCC)
          trak->properties.audio.vbr = 1;

        /* check for a MS-style WAVE format header */
        if ((current_atom_size >= 0x48) &&
            (BE_32(&trak_atom[i + 0x44]) == WAVE_ATOM)) {
          trak->properties.audio.wave_present = 1;
        } else {
          trak->properties.audio.wave_present = 0;
        }

        debug_atom_load("    audio description\n");
        debug_atom_load("      %d Hz, %d bits, %d channels, %saudio fourcc = '%c%c%c%c' (%02X%02X%02X%02X)\n",
          trak->properties.audio.sample_rate,
          trak->properties.audio.bits,
          trak->properties.audio.channels,
          (trak->properties.audio.vbr) ? "vbr, " : "",
          trak_atom[i + 0x10],
          trak_atom[i + 0x11],
          trak_atom[i + 0x12],
          trak_atom[i + 0x13],
          trak_atom[i + 0x10],
          trak_atom[i + 0x11],
          trak_atom[i + 0x12],
          trak_atom[i + 0x13]);
        if (BE_32(&trak_atom[i + 0x0C]) > 0x24) {
          debug_atom_load("      %d samples/packet, %d bytes/packet, %d bytes/frame\n",
            trak->properties.audio.samples_per_packet,
            trak->properties.audio.bytes_per_packet,
            trak->properties.audio.bytes_per_frame);
          debug_atom_load("      %d bytes/sample (%d samples/frame)\n",
            trak->properties.audio.bytes_per_sample,
            trak->properties.audio.samples_per_frame);
        }
      }

      i -= hack_adjust;

    } else if (current_atom == ESDS_ATOM) {

      ext_uint32_t len;

      debug_atom_load("    qt/mpeg-4 esds atom\n");

      if ((trak->type == MEDIA_VIDEO) ||
          (trak->type == MEDIA_AUDIO)) {

        j = i + 8;
        if( trak_atom[j++] == 0x03 ) {
          j += mp4_read_descr_len( &trak_atom[j], &len );
          j++;
        }
        j += 2;
        if( trak_atom[j++] == 0x04 ) {
          j += mp4_read_descr_len( &trak_atom[j], &len );
          j += 13;
          if( trak_atom[j++] == 0x05 ) {
            j += mp4_read_descr_len( &trak_atom[j], &len );
            debug_atom_load("      decoder config is %d (0x%X) bytes long\n",
              len, len);
            trak->decoder_config = realloc(trak->decoder_config, len);
            trak->decoder_config_len = len;
            memcpy(trak->decoder_config,&trak_atom[j],len);
          }
        }
      }

    } else if (current_atom == STSZ_ATOM) {

      /* there should only be one of these atoms */
      if (trak->sample_size_table) {
        last_error = QT_HEADER_TROUBLE;
        goto free_trak;
      }

      trak->sample_size = BE_32(&trak_atom[i + 8]);
      trak->sample_size_count = BE_32(&trak_atom[i + 12]);

      debug_atom_load("    qt stsz atom (sample size atom): sample size = %d, %d entries\n",
        trak->sample_size, trak->sample_size_count);

      /* allocate space and load table only if sample size is 0 */
      if (trak->sample_size == 0) {
        trak->sample_size_table = (unsigned int *)malloc(
          trak->sample_size_count * sizeof(unsigned int));
        if (!trak->sample_size_table) {
          last_error = QT_NO_MEMORY;
          goto free_trak;
        }
        /* load the sample size table */
        for (j = 0; j < trak->sample_size_count; j++) {
          trak->sample_size_table[j] =
            BE_32(&trak_atom[i + 16 + j * 4]);
          debug_atom_load("      sample size %d: %d\n",
            j, trak->sample_size_table[j]);
        }
      } else
        /* set the pointer to non-NULL to indicate that the atom type has
         * already been seen for this trak atom */
        trak->sample_size_table = (void *)-1;

    } else if (current_atom == STSS_ATOM) {

      /* there should only be one of these atoms */
      if (trak->sync_sample_table) {
        last_error = QT_HEADER_TROUBLE;
        goto free_trak;
      }

      trak->sync_sample_count = BE_32(&trak_atom[i + 8]);

      debug_atom_load("    qt stss atom (sample sync atom): %d sync samples\n",
        trak->sync_sample_count);

      trak->sync_sample_table = (unsigned int *)malloc(
        trak->sync_sample_count * sizeof(unsigned int));
      if (!trak->sync_sample_table) {
        last_error = QT_NO_MEMORY;
        goto free_trak;
      }

      /* load the sync sample table */
      for (j = 0; j < trak->sync_sample_count; j++) {
        trak->sync_sample_table[j] =
          BE_32(&trak_atom[i + 12 + j * 4]);
        debug_atom_load("      sync sample %d: sample %d (%d) is a keyframe\n",
          j, trak->sync_sample_table[j],
          trak->sync_sample_table[j] - 1);
      }

    } else if (current_atom == STCO_ATOM) {

      /* there should only be one of either stco or co64 */
      if (trak->chunk_offset_table) {
        last_error = QT_HEADER_TROUBLE;
        goto free_trak;
      }

      trak->chunk_offset_count = BE_32(&trak_atom[i + 8]);

      debug_atom_load("    qt stco atom (32-bit chunk offset atom): %d chunk offsets\n",
        trak->chunk_offset_count);

      trak->chunk_offset_table = (int64_t *)malloc(
        trak->chunk_offset_count * sizeof(int64_t));
      if (!trak->chunk_offset_table) {
        last_error = QT_NO_MEMORY;
        goto free_trak;
      }

      /* load the chunk offset table */
      for (j = 0; j < trak->chunk_offset_count; j++) {
        trak->chunk_offset_table[j] =
          BE_32(&trak_atom[i + 12 + j * 4]);
        debug_atom_load("      chunk %d @ 0x%llX\n",
          j, trak->chunk_offset_table[j]);
      }

    } else if (current_atom == CO64_ATOM) {

      /* there should only be one of either stco or co64 */
      if (trak->chunk_offset_table) {
        last_error = QT_HEADER_TROUBLE;
        goto free_trak;
      }

      trak->chunk_offset_count = BE_32(&trak_atom[i + 8]);

      debug_atom_load("    qt co64 atom (64-bit chunk offset atom): %d chunk offsets\n",
        trak->chunk_offset_count);

      trak->chunk_offset_table = (int64_t *)malloc(
        trak->chunk_offset_count * sizeof(int64_t));
      if (!trak->chunk_offset_table) {
        last_error = QT_NO_MEMORY;
        goto free_trak;
      }

      /* load the 64-bit chunk offset table */
      for (j = 0; j < trak->chunk_offset_count; j++) {
        trak->chunk_offset_table[j] =
          BE_32(&trak_atom[i + 12 + j * 8 + 0]);
        trak->chunk_offset_table[j] <<= 32;
        trak->chunk_offset_table[j] |=
          BE_32(&trak_atom[i + 12 + j * 8 + 4]);
        debug_atom_load("      chunk %d @ 0x%llX\n",
          j, trak->chunk_offset_table[j]);
      }

    } else if (current_atom == STSC_ATOM) {

      /* there should only be one of these atoms */
      if (trak->sample_to_chunk_table) {
        last_error = QT_HEADER_TROUBLE;
        goto free_trak;
      }

      trak->sample_to_chunk_count = BE_32(&trak_atom[i + 8]);

      debug_atom_load("    qt stsc atom (sample-to-chunk atom): %d entries\n",
        trak->sample_to_chunk_count);

      trak->sample_to_chunk_table = (sample_to_chunk_table_t *)malloc(
        trak->sample_to_chunk_count * sizeof(sample_to_chunk_table_t));
      if (!trak->sample_to_chunk_table) {
        last_error = QT_NO_MEMORY;
        goto free_trak;
      }

      /* load the sample to chunk table */
      for (j = 0; j < trak->sample_to_chunk_count; j++) {
        trak->sample_to_chunk_table[j].first_chunk =
          BE_32(&trak_atom[i + 12 + j * 12 + 0]);
        trak->sample_to_chunk_table[j].samples_per_chunk =
          BE_32(&trak_atom[i + 12 + j * 12 + 4]);
        debug_atom_load("      %d: %d samples/chunk starting at chunk %d (%d)\n",
          j, trak->sample_to_chunk_table[j].samples_per_chunk,
          trak->sample_to_chunk_table[j].first_chunk,
          trak->sample_to_chunk_table[j].first_chunk - 1);
      }

    } else if (current_atom == STTS_ATOM) {

      /* there should only be one of these atoms */
      if (trak->time_to_sample_table) {
        last_error = QT_HEADER_TROUBLE;
        goto free_trak;
      }

      trak->time_to_sample_count = BE_32(&trak_atom[i + 8]);

      debug_atom_load("    qt stts atom (time-to-sample atom): %d entries\n",
        trak->time_to_sample_count);

      trak->time_to_sample_table = (time_to_sample_table_t *)malloc(
        (trak->time_to_sample_count+1) * sizeof(time_to_sample_table_t));
      if (!trak->time_to_sample_table) {
        last_error = QT_NO_MEMORY;
        goto free_trak;
      }

      /* load the time to sample table */
      for (j = 0; j < trak->time_to_sample_count; j++) {
        trak->time_to_sample_table[j].count =
          BE_32(&trak_atom[i + 12 + j * 8 + 0]);
        trak->time_to_sample_table[j].duration =
          BE_32(&trak_atom[i + 12 + j * 8 + 4]);
        debug_atom_load("      %d: count = %d, duration = %d\n",
          j, trak->time_to_sample_table[j].count,
          trak->time_to_sample_table[j].duration);
      }
      trak->time_to_sample_table[j].count = 0; /* terminate with zero */
    }
  }

  return QT_OK;

  /* jump here to make sure everything is free'd and avoid leaking memory */
free_trak:
  free(trak->edit_list_table);
  free(trak->chunk_offset_table);
  /* this pointer might have been set to -1 as a special case */
  if (trak->sample_size_table != (void *)-1)
    free(trak->sample_size_table);
  free(trak->sync_sample_table);
  free(trak->sample_to_chunk_table);
  free(trak->time_to_sample_table);
  free(trak->decoder_config);
  free(trak->stsd);

  return last_error;
}

/* Traverse through a reference atom and extract the URL and data rate. */
static qt_error parse_reference_atom (reference_t *ref,
                                      unsigned char *ref_atom) {

  int i, j;
  unsigned int ref_atom_size = BE_32(&ref_atom[0]);
  qt_atom current_atom;
  unsigned int current_atom_size;

  /* initialize reference atom */
  ref->url = NULL;
  ref->data_rate = 0;
  ref->qtim_version = 0;

  /* traverse through the atom looking for the key atoms */
  for (i = ATOM_PREAMBLE_SIZE; i < ref_atom_size - 4; i++) {

    current_atom_size = BE_32(&ref_atom[i - 4]);
    if (i + current_atom_size > ref_atom_size)
      break;
    current_atom = BE_32(&ref_atom[i]);

    if (current_atom == RDRF_ATOM) {     
      /* if the URL starts with "http://", copy it */
      if (strncmp((char*) &ref_atom[i + 12], "http://", 7) == 0) {

        /* URL is spec'd to terminate with a NULL; don't trust it */
        ref->url = malloc(BE_32((char*) &ref_atom[i + 12]) + 1);
        strncpy(ref->url, (char*) &ref_atom[i + 16], BE_32(&ref_atom[i + 12]));
        ref->url[BE_32(&ref_atom[i + 12]) - 1] = '\0';

      } else {

        int string_size = BE_32(&ref_atom[i + 12]) + 1;

        /* otherwise, append relative URL to base MRL */
        ref->url = malloc(string_size);
        strncpy(ref->url, (char*) &ref_atom[i + 16], BE_32(&ref_atom[i + 12]));
        ref->url[string_size - 1] = '\0';
      }

      debug_atom_load("    qt rdrf URL reference:\n      %s\n", ref->url);

    } else if (current_atom == RMDR_ATOM) {

      /* load the data rate */
      ref->data_rate = BE_32(&ref_atom[i + 8]);
      ref->data_rate *= 10;

      debug_atom_load("    qt rmdr data rate = %lld\n", ref->data_rate);

    } else if (current_atom == RMVC_ATOM) {

      debug_atom_load("    qt rmvc atom\n");

      /* search the rmvc atom for 'qtim'; 2 bytes will follow the qtim
       * chars so only search to 6 bytes to the end */
      for (j = 4; j < current_atom_size - 6; j++) {

        if (BE_32(&ref_atom[i + j]) == QTIM_ATOM) {

          ref->qtim_version = BE_16(&ref_atom[i + j + 4]);
          debug_atom_load("      qtim version = %04X\n", ref->qtim_version);
        }
      }
    }
  }

  return QT_OK;
}

/* This is a little support function used to process the edit list when
 * building a frame table. */
#define MAX_DURATION 0x7FFFFFFF
static void 
get_next_edit_list_entry(qt_trak *trak,
			 unsigned int *edit_list_index,
			 unsigned int *edit_list_media_time,
			 int64_t *edit_list_duration,
			 unsigned int global_timescale) {

  /* if there is no edit list, set to max duration and get out */
  if (!trak->edit_list_table) {

    *edit_list_media_time = 0;
    *edit_list_duration = MAX_DURATION;
    debug_edit_list("  qt: no edit list table, initial = %d, %lld\n", *edit_list_media_time, *edit_list_duration);
    return;

  } else while (*edit_list_index < trak->edit_list_count) {

    /* otherwise, find an edit list entries whose media time != -1 */
    if (trak->edit_list_table[*edit_list_index].media_time != -1) {

      *edit_list_media_time =
        trak->edit_list_table[*edit_list_index].media_time;
      *edit_list_duration =
        trak->edit_list_table[*edit_list_index].track_duration;

      /* duration is in global timescale units; convert to trak timescale */
      *edit_list_duration *= trak->timescale;
      *edit_list_duration /= global_timescale;

      *edit_list_index = *edit_list_index + 1;
      break;
    }

    *edit_list_index = *edit_list_index + 1;
  }

  /* on the way out, check if this is the last edit list entry; if so,
   * don't let the duration expire (so set it to an absurdly large value)
   */
  if (*edit_list_index == trak->edit_list_count)
    *edit_list_duration = MAX_DURATION;
  debug_edit_list("  qt: edit list table exists, initial = %d, %lld\n", *edit_list_media_time, *edit_list_duration);
}

static qt_error build_frame_table(qt_trak *trak,
				  unsigned int global_timescale) {

  int i, j;
  unsigned int frame_counter;
  unsigned int chunk_start, chunk_end;
  unsigned int samples_per_chunk;
  ext_uint64_t current_offset;
  int64_t current_pts;
  unsigned int pts_index;
  unsigned int pts_index_countdown;
  unsigned int audio_frame_counter = 0;
  unsigned int edit_list_media_time;
  int64_t edit_list_duration;
  int64_t frame_duration = 0;
  unsigned int edit_list_index;
  unsigned int edit_list_pts_counter;

  /* AUDIO and OTHER frame types follow the same rules; VIDEO and vbr audio
   * frame types follow a different set */
  if ((trak->type == MEDIA_VIDEO) ||
      (trak->properties.audio.vbr)) {

    /* in this case, the total number of frames is equal to the number of
     * entries in the sample size table */
    trak->frame_count = trak->sample_size_count;
    trak->frames = (qt_frame *)malloc(
      trak->frame_count * sizeof(qt_frame));
    if (!trak->frames)
      return QT_NO_MEMORY;
    trak->current_frame = 0;

    /* initialize more accounting variables */
    frame_counter = 0;
    current_pts = 0;
    pts_index = 0;
    pts_index_countdown =
      trak->time_to_sample_table[pts_index].count;

    /* iterate through each start chunk in the stsc table */
    for (i = 0; i < trak->sample_to_chunk_count; i++) {
      /* iterate from the first chunk of the current table entry to
       * the first chunk of the next table entry */
      chunk_start = trak->sample_to_chunk_table[i].first_chunk;
      if (i < trak->sample_to_chunk_count - 1)
        chunk_end =
          trak->sample_to_chunk_table[i + 1].first_chunk;
      else
        /* if the first chunk is in the last table entry, iterate to the
           final chunk number (the number of offsets in stco table) */
        chunk_end = trak->chunk_offset_count + 1;

      /* iterate through each sample in a chunk */
      for (j = chunk_start - 1; j < chunk_end - 1; j++) {

        samples_per_chunk =
          trak->sample_to_chunk_table[i].samples_per_chunk;
        current_offset = trak->chunk_offset_table[j];
        while (samples_per_chunk > 0) {

          /* figure out the offset and size */
          trak->frames[frame_counter].offset = current_offset;
          if (trak->sample_size) {
            trak->frames[frame_counter].size =
              trak->sample_size;
            current_offset += trak->sample_size;
          } else {
            trak->frames[frame_counter].size =
              trak->sample_size_table[frame_counter];
            current_offset +=
              trak->sample_size_table[frame_counter];
          }

          /* if there is no stss (sample sync) table, make all of the frames
           * keyframes; otherwise, clear the keyframe bits for now */
          if (trak->sync_sample_table)
            trak->frames[frame_counter].keyframe = 0;
          else
            trak->frames[frame_counter].keyframe = 1;

          /* figure out the pts situation */
          trak->frames[frame_counter].pts = current_pts;
          current_pts +=
            trak->time_to_sample_table[pts_index].duration;
          pts_index_countdown--;
          /* time to refresh countdown? */
          if (!pts_index_countdown) {
            pts_index++;
            pts_index_countdown =
              trak->time_to_sample_table[pts_index].count;
          }

          samples_per_chunk--;
          frame_counter++;
        }
      }
    }

    /* fill in the keyframe information */
    if (trak->sync_sample_table) {
      for (i = 0; i < trak->sync_sample_count; i++)
        trak->frames[trak->sync_sample_table[i] - 1].keyframe = 1;
    }

    /* initialize edit list considerations */
    edit_list_index = 0;
    get_next_edit_list_entry(trak,
			     &edit_list_index,
			     &edit_list_media_time, 
			     &edit_list_duration, 
			     global_timescale);

    /* fix up pts information w.r.t. the edit list table */
    edit_list_pts_counter = 0;
    for (i = 0; i < trak->frame_count; i++) {

      debug_edit_list("    %d: (before) pts = %lld...", i, trak->frames[i].pts);

      if (trak->frames[i].pts < edit_list_media_time)
        trak->frames[i].pts = edit_list_pts_counter;
      else {
        if (i < trak->frame_count - 1)
          frame_duration =
            (trak->frames[i + 1].pts - trak->frames[i].pts);

        debug_edit_list("duration = %lld...", frame_duration);
        trak->frames[i].pts = edit_list_pts_counter;
        edit_list_pts_counter += frame_duration;
        edit_list_duration -= frame_duration;
      }

      debug_edit_list("(fixup) pts = %lld...", trak->frames[i].pts);

      /* reload media time and duration */
      if (edit_list_duration <= 0) {
        get_next_edit_list_entry(trak, &edit_list_index,
          &edit_list_media_time, &edit_list_duration, global_timescale);
      }

      debug_edit_list("(after) pts = %lld...\n", trak->frames[i].pts);
    }

    /* compute final pts values */
    for (i = 0; i < trak->frame_count; i++) {
      trak->frames[i].pts *= 90000;
      trak->frames[i].pts /= trak->timescale;
      debug_edit_list("  final pts for sample %d = %lld\n", i, trak->frames[i].pts);
    }

  } else {

    /* in this case, the total number of frames is equal to the number of
     * chunks */
    trak->frame_count = trak->chunk_offset_count;
    trak->frames = (qt_frame *)malloc(
      trak->frame_count * sizeof(qt_frame));
    if (!trak->frames)
      return QT_NO_MEMORY;

    if (trak->type == MEDIA_AUDIO) {
      /* iterate through each start chunk in the stsc table */
      for (i = 0; i < trak->sample_to_chunk_count; i++) {
        /* iterate from the first chunk of the current table entry to
         * the first chunk of the next table entry */
        chunk_start = trak->sample_to_chunk_table[i].first_chunk;
        if (i < trak->sample_to_chunk_count - 1)
          chunk_end =
            trak->sample_to_chunk_table[i + 1].first_chunk;
        else
          /* if the first chunk is in the last table entry, iterate to the
             final chunk number (the number of offsets in stco table) */
          chunk_end = trak->chunk_offset_count + 1;

        /* iterate through each sample in a chunk and fill in size and
         * pts information */
        for (j = chunk_start - 1; j < chunk_end - 1; j++) {

          /* figure out the pts for this chunk */
          trak->frames[j].pts = audio_frame_counter;
          trak->frames[j].pts *= 90000;
          trak->frames[j].pts /= trak->timescale;

          /* fetch the alleged chunk size according to the QT header */
          trak->frames[j].size =
            trak->sample_to_chunk_table[i].samples_per_chunk;

          /* the chunk size is actually the audio frame count */
          audio_frame_counter += trak->frames[j].size;

          /* compute the actual chunk size */
          trak->frames[j].size =
            (trak->frames[j].size *
             trak->properties.audio.channels) /
             trak->properties.audio.samples_per_frame *
             trak->properties.audio.bytes_per_frame;
        }
      }
    }

    /* fill in the rest of the information for the audio samples */
    for (i = 0; i < trak->frame_count; i++) {
      trak->frames[i].offset = trak->chunk_offset_table[i];
      trak->frames[i].keyframe = 0;
      if (trak->type != MEDIA_AUDIO)
        trak->frames[i].pts = 0;
    }
  }

  return QT_OK;
}

/*
 * This function takes a pointer to a qt_info structure and a pointer to
 * a buffer containing an uncompressed moov atom. When the function
 * finishes successfully, qt_info will have a list of qt_frame objects,
 * ordered by offset.
 */
static void parse_moov_atom(qt_info *info, unsigned char *moov_atom) {
  int i, j;
  unsigned int moov_atom_size = BE_32(&moov_atom[0]);
  qt_atom current_atom;
  int string_size;
  unsigned int max_video_frames = 0;
  unsigned int max_audio_frames = 0;

  /* make sure this is actually a moov atom */
  if (BE_32(&moov_atom[4]) != MOOV_ATOM) {
    info->last_error = QT_NO_MOOV_ATOM;
    return;
  }

  /* prowl through the moov atom looking for very specific targets */
  for (i = ATOM_PREAMBLE_SIZE; i < moov_atom_size - 4; i++) {
    current_atom = BE_32(&moov_atom[i]);

    if (current_atom == MVHD_ATOM) {
      parse_mvhd_atom(info, &moov_atom[i - 4]);
      if (info->last_error != QT_OK)
        return;
      i += BE_32(&moov_atom[i - 4]) - 4;
    } else if (current_atom == TRAK_ATOM) {

      /* create a new trak structure */
      info->trak_count++;
      info->traks = (qt_trak *)realloc(info->traks,
        info->trak_count * sizeof(qt_trak));

      parse_trak_atom (&info->traks[info->trak_count - 1], &moov_atom[i - 4]);
      if (info->last_error != QT_OK) {
        info->trak_count--;
        return;
      }
      i += BE_32(&moov_atom[i - 4]) - 4;

    } else if (current_atom == CPY_ATOM) {

      string_size = BE_16(&moov_atom[i + 4]) + 1;
      info->copyright = realloc (info->copyright, string_size);
      strncpy(info->copyright, 
	      (char*) &moov_atom[i + 8], 
	      string_size - 1);
      info->copyright[string_size - 1] = 0;

    } else if (current_atom == NAM_ATOM) {

      string_size = BE_16(&moov_atom[i + 4]) + 1;
      info->name = realloc (info->name, string_size);
      strncpy(info->name, 
	      (char*) &moov_atom[i + 8], 
	      string_size - 1);
      info->name[string_size - 1] = 0;

    } else if (current_atom == DES_ATOM) {

      string_size = BE_16(&moov_atom[i + 4]) + 1;
      info->description = realloc (info->description, string_size);
      strncpy(info->description,
	      (char*) &moov_atom[i + 8], 
	      string_size - 1);
      info->description[string_size - 1] = 0;

    } else if (current_atom == CMT_ATOM) {

      string_size = BE_16(&moov_atom[i + 4]) + 1;
      info->comment = realloc (info->comment, string_size);
      strncpy(info->comment, 
	      (char*) &moov_atom[i + 8],
	      string_size - 1);
      info->comment[string_size - 1] = 0;

    } else if (current_atom == RMDA_ATOM) {

      /* create a new reference structure */
      info->reference_count++;
      info->references = (reference_t *)realloc(info->references,
        info->reference_count * sizeof(reference_t));

      parse_reference_atom(&info->references[info->reference_count - 1],
        &moov_atom[i - 4]);

    }
  }
  debug_atom_load("  qt: finished parsing moov atom\n");

  /* build frame tables corresponding to each trak */
  debug_frame_table("  qt: preparing to build %d frame tables\n",
    info->trak_count);
  for (i = 0; i < info->trak_count; i++) {

    debug_frame_table("    qt: building frame table #%d (%s)\n", i,
      (info->traks[i].type == MEDIA_VIDEO) ? "video" : "audio");
    build_frame_table(&info->traks[i], info->timescale);

    /* dump the frame table in debug mode */
    for (j = 0; j < info->traks[i].frame_count; j++)
      debug_frame_table("      %d: %8X bytes @ %llX, %lld pts%s\n",
        j,
        info->traks[i].frames[j].size,
        info->traks[i].frames[j].offset,
        info->traks[i].frames[j].pts,
        (info->traks[i].frames[j].keyframe) ? " (keyframe)" : "");

    /* decide which audio trak and which video trak has the most frames */
    if ((info->traks[i].type == MEDIA_VIDEO) &&
        (info->traks[i].frame_count > max_video_frames)) {

      info->video_trak = i;
      max_video_frames = info->traks[i].frame_count;

    } else if ((info->traks[i].type == MEDIA_AUDIO) &&
               (info->traks[i].frame_count > max_audio_frames)) {

      info->audio_trak = i;
      max_audio_frames = info->traks[i].frame_count;
    }
  }

  /* check for references */
  if (info->reference_count > 0) {

    /* init chosen reference to the first entry */
    info->chosen_reference = 0;

    /* iterate through 1..n-1 reference entries and decide on the right one */
    for (i = 1; i < info->reference_count; i++) {

      if (info->references[i].qtim_version >
          info->references[info->chosen_reference].qtim_version)
        info->chosen_reference = i;
      else if ((info->references[i].data_rate >
                info->references[info->chosen_reference].data_rate))
        info->chosen_reference = i;
    }

    debug_atom_load("  qt: chosen reference is ref #%d, qtim version %04X, %lld bps\n      URL: %s\n",
      info->chosen_reference,
      info->references[info->chosen_reference].qtim_version,
      info->references[info->chosen_reference].data_rate,
      info->references[info->chosen_reference].url);
  }
}

static qt_error open_qt_file(qt_info *info) {

  unsigned char *moov_atom = NULL;
  off_t moov_atom_offset = -1;
  int64_t moov_atom_size = -1;

  /* zlib stuff */
  z_stream z_state;
  int z_ret_code;
  unsigned char *unzip_buffer;

  find_moov_atom(info, &moov_atom_offset, &moov_atom_size);

  if (moov_atom_offset == -1) {
    info->last_error = QT_NO_MOOV_ATOM;
    return info->last_error;
  }
  info->moov_first_offset = moov_atom_offset;

  moov_atom = (unsigned char *)malloc(moov_atom_size);
  if (moov_atom == NULL) {
    info->last_error = QT_NO_MEMORY;
    return info->last_error;
  }

  /* seek to the start of moov atom */
  info->inputPos = info->moov_first_offset;
  if (readBuf(info, moov_atom, moov_atom_size) !=
    moov_atom_size) {
    free(moov_atom);
    info->last_error = QT_FILE_READ_ERROR;
    return info->last_error;
  }

  /* check if moov is compressed */
  if (BE_32(&moov_atom[12]) == CMOV_ATOM) {

    info->compressed_header = 1;

    z_state.next_in = &moov_atom[0x28];
    z_state.avail_in = moov_atom_size - 0x28;
    z_state.avail_out = BE_32(&moov_atom[0x24]);
    unzip_buffer = (unsigned char *)malloc(BE_32(&moov_atom[0x24]));
    if (!unzip_buffer) {
      free(moov_atom);
      info->last_error = QT_NO_MEMORY;
      return info->last_error;
    }

    z_state.next_out = unzip_buffer;
    z_state.zalloc = (alloc_func)0;
    z_state.zfree = (free_func)0;
    z_state.opaque = (voidpf)0;

    z_ret_code = inflateInit (&z_state);
    if (Z_OK != z_ret_code) {
      free(unzip_buffer);
      free(moov_atom);
      info->last_error = QT_ZLIB_ERROR;
      return info->last_error;
    }

    z_ret_code = inflate(&z_state, Z_NO_FLUSH);
    if ((z_ret_code != Z_OK) && (z_ret_code != Z_STREAM_END)) {
      free(unzip_buffer);
      free(moov_atom);
      info->last_error = QT_ZLIB_ERROR;
      return info->last_error;
    }

    z_ret_code = inflateEnd(&z_state);
    if (Z_OK != z_ret_code) {
      free(unzip_buffer);
      free(moov_atom);
      info->last_error = QT_ZLIB_ERROR;
      return info->last_error;
    }

    /* replace the compressed moov atom with the decompressed atom */
    free (moov_atom);
    moov_atom = unzip_buffer;
    moov_atom_size = BE_32(&moov_atom[0]);
  }

  if (!moov_atom) {
    info->last_error = QT_NO_MOOV_ATOM;
    return info->last_error;
  }

  /* write moov atom to disk if debugging option is turned on */
  dump_moov_atom(moov_atom, moov_atom_size);

  /* take apart the moov atom */
  parse_moov_atom(info, moov_atom);
  if (info->last_error != QT_OK) {
    free(moov_atom);
    return info->last_error;
  }

  free(moov_atom);

  return QT_OK;
}


#if _blitzi_quicktime_decoder_in_public_domain



#define PLUGIN_VERSION "0.1.0"
#define PLUGIN_NAME    "Video metadata (AVI, QuickTime, MPEG-1, MPEG-2)"

#define HEAD_BUFFER    12		/* We must be able to determine
					 * file format using this many bytes
					 * from the beginning of the file */

/* 32-bit-specific definitions of portable data types */
typedef unsigned int uint32;

/* The various formats we support. */
typedef enum
{
   UNKNOWN,
   AVI,					/* Microsoft AVI */
   QUICKTIME,				/* Apple QuickTime (MOV) */
   MPEG					/* MPEG 1 or 2 */
} Format;

/* Wrap the metadata we're collecting into a struct for easy passing */
typedef struct
{
   unsigned int width;			/* width in pixels */
   unsigned int height;			/* height in pixels */
   unsigned int fps;			/* frames per second */
   unsigned int duration;		/* duration in milliseconds */
   unsigned int bitrate;		/* bitrate in kbps */
   char *codec;				/* video compression codec */
} Data;

/* local prototypes */

Format find_format(FILE *file);
void parse_avi(FILE *file, Data *data);
void parse_quicktime(FILE *file, Data *data);
int parse_mpeg(FILE *file, Data *data);
double round_double(double num);
unsigned long int fread_le(FILE *file, int bytes);
unsigned long int fread_be(FILE *file, int bytes);


/* QuickTime uses big-endian ordering, and block ("atom") lengths include the
 * entire atom, including the fourcc specifying atom type and the length
 * integer itself.
 */
void parse_quicktime(FILE *file, Data *data)
{
   char fourcc[5];
   unsigned blockLen;
   unsigned subBlockLen;
   unsigned subSubBlockLen;
   unsigned timescale;
   long blockStart;
   long subBlockStart;
   long subSubBlockStart;

   fseek(file, 4L, SEEK_SET);
   fread(fourcc, sizeof(char), 4, file);
   /* If data is first, header's at end of file, so skip to it */
   if(memcmp(fourcc, "mdat", 4)==0)
   {
      fseek(file, 0L, SEEK_SET);
      blockLen = fread_be(file, 4);
      fseek(file, (long) (blockLen + 4), SEEK_SET);
      fread(fourcc, sizeof(char), 4, file);
   }

   if(memcmp(fourcc, "moov", 4)!=0)
      return;
   blockStart = ftell(file);
   blockLen = fread_be(file, 4);	/* mvhd length */
   fread(fourcc, sizeof(char), 4, file);
   if(memcmp(fourcc, "mvhd", 4)!=0)
      return;

   /* Now we're at the start of the movie header */

   /* 20: time scale (time units per second) (4 bytes) */
   fseek(file, blockStart + 20, SEEK_SET);
   timescale = fread_be(file, 4);

   /* 24: duration in time units (4 bytes) */
   data->duration = (unsigned int) round_double((double) fread_be(file, 4)
						/ timescale * 1000);

   /* Skip the rest of the mvhd */
   fseek(file, blockStart + blockLen, SEEK_SET);

   /* Find and parse trak atoms */
   while(!feof(file))
   {
      unsigned int width, height;

      /* Find the next trak atom */
      blockStart = ftell(file);
      blockLen = fread_be(file, 4);	/* trak (or other atom) length */
      fread(fourcc, sizeof(char), 4, file);
      if(memcmp(fourcc, "trak", 4)!=0)	/* If it's not a trak atom, skip it */
      {
	 if(!feof(file))
	    fseek(file, blockStart + blockLen, SEEK_SET);
	 continue;
      }

      subBlockStart = ftell(file);
      subBlockLen = fread_be(file, 4);	/* tkhd length */
      fread(fourcc, sizeof(char), 4, file);
      if(memcmp(fourcc, "tkhd", 4)!=0)
	 return;

      /* Now in the track header */

      /* 84: width (2 bytes) */
      fseek(file, subBlockStart + 84, SEEK_SET);
      width = fread_be(file, 2);

      /* 88: height (2 bytes) */
      fseek(file, subBlockStart + 88, SEEK_SET);
      height = fread_be(file, 2);

      /* Note on above: Apple's docs say that width/height are 4-byte integers,
       * but all files I've seen have the data stored in the high-order two
       * bytes, with the low-order two being 0x0000.  Interpreting it the
       * "official" way would make width/height be thousands of pixels each.
       */

      /* Skip rest of tkhd */
      fseek(file, subBlockStart + subBlockLen, SEEK_SET);

      /* Find mdia atom for this trak */
      subBlockStart = ftell(file);
      subBlockLen = fread_be(file, 4);
      fread(fourcc, sizeof(char), 4, file);
      while(memcmp(fourcc, "mdia", 4)!=0)
      {
	 fseek(file, subBlockStart + subBlockLen, SEEK_SET);
	 subBlockStart = ftell(file);
	 subBlockLen = fread_be(file, 4);
	 fread(fourcc, sizeof(char), 4, file);
      }

      /* Now we're in the mdia atom; first sub-atom should be mdhd */
      subSubBlockStart = ftell(file);
      subSubBlockLen = fread_be(file, 4);
      fread(fourcc, sizeof(char), 4, file);
      if(memcmp(fourcc, "mdhd", 4)!=0)
	 return;
      /* TODO: extract language from the mdhd?  For now skip to hdlr. */
      fseek(file, subSubBlockStart + subSubBlockLen, SEEK_SET);
      subSubBlockStart = ftell(file);
      subSubBlockLen = fread_be(file, 4);
      fread(fourcc, sizeof(char), 4, file);
      if(memcmp(fourcc, "hdlr", 4)!=0)
	 return;
      /* 12: Component type: "mhlr" or "dhlr"; we only care about mhlr,
       * which should (?) appear first */
      fseek(file, subSubBlockStart + 12, SEEK_SET);
      fread(fourcc, sizeof(char), 4, file);
      if(memcmp(fourcc, "mhlr", 4)!=0)
	 return;
      fread(fourcc, sizeof(char), 4, file);
      if(memcmp(fourcc, "vide", 4)==0)	/* This is a video trak */
      {
	 data->height = height;
	 data->width = width;
      }

      /* Skip rest of the trak */
      fseek(file, blockStart + blockLen, SEEK_SET);
   }
}

#endif













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
   video/quicktime: mov,qt: Quicktime animation;
   video/x-quicktime: mov,qt: Quicktime animation;
   application/x-quicktimeplayer: qtl: Quicktime list;
 */
struct EXTRACTOR_Keywords * libextractor_qt_extract(char * filename,
                                                    char * data,
                                                    size_t size,
                                                    struct EXTRACTOR_Keywords * prev) {
  qt_info * this;

  if (size < 8)
    return prev;
  if ( (memcmp(&data[4],
	       "moov",
	       4) != 0) &&
       (memcmp(&data[4],
	       "mdat",
	       4) != 0) )
    return prev;

  this = create_qt_info();

  this->input = data;
  this->inputPos = 0;
  this->inputLen = size;
  if (QT_OK != open_qt_file(this)) {
    free_qt_info(this);
    return prev;
  }

  if (this->description != NULL)
    prev = addKeyword(EXTRACTOR_TITLE, this->description, prev);
  if (this->comment != NULL)
    prev = addKeyword(EXTRACTOR_COMMENT, this->comment, prev);
  if (this->copyright != NULL)
    prev = addKeyword(EXTRACTOR_COPYRIGHT, this->copyright, prev);
  if (this->name != NULL)
    prev = addKeyword(EXTRACTOR_DESCRIPTION, this->name, prev);
  prev = addKeyword(EXTRACTOR_MIMETYPE, "video/quicktime", prev);

  free_qt_info(this);
  return prev;
}


/*  end of qtextractor.c */
