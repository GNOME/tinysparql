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


     Some of this code is based on AVInfo 1.0 alpha 11
     (c) George Shuklin, gs]AT[shounen.ru, 2002-2004
     http://shounen.ru/soft/avinfo/

 */

#define DEBUG_EXTRACT_MP3 0

#include "platform.h"
#include "extractor.h"
#include "convert.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct {
  char * title;
  char * artist;
  char * album;
  char * year;
  char * comment;
  char * genre;
} id3tag;

static const char *const genre_names[] = {
    gettext_noop("Blues"),
    gettext_noop("Classic Rock"),
    gettext_noop("Country"),
    gettext_noop("Dance"),
    gettext_noop("Disco"),
    gettext_noop("Funk"),
    gettext_noop("Grunge"),
    gettext_noop("Hip-Hop"),
    gettext_noop("Jazz"),
    gettext_noop("Metal"),
    gettext_noop("New Age"),
    gettext_noop("Oldies"),
    gettext_noop("Other"),
    gettext_noop("Pop"),
    gettext_noop("R&B"),
    gettext_noop("Rap"),
    gettext_noop("Reggae"),
    gettext_noop("Rock"),
    gettext_noop("Techno"),
    gettext_noop("Industrial"),
    gettext_noop("Alternative"),
    gettext_noop("Ska"),
    gettext_noop("Death Metal"),
    gettext_noop("Pranks"),
    gettext_noop("Soundtrack"),
    gettext_noop("Euro-Techno"),
    gettext_noop("Ambient"),
    gettext_noop("Trip-Hop"),
    gettext_noop("Vocal"),
    gettext_noop("Jazz+Funk"),
    gettext_noop("Fusion"),
    gettext_noop("Trance"),
    gettext_noop("Classical"),
    gettext_noop("Instrumental"),
    gettext_noop("Acid"),
    gettext_noop("House"),
    gettext_noop("Game"),
    gettext_noop("Sound Clip"),
    gettext_noop("Gospel"),
    gettext_noop("Noise"),
    gettext_noop("Alt. Rock"),
    gettext_noop("Bass"),
    gettext_noop("Soul"),
    gettext_noop("Punk"),
    gettext_noop("Space"),
    gettext_noop("Meditative"),
    gettext_noop("Instrumental Pop"),
    gettext_noop("Instrumental Rock"),
    gettext_noop("Ethnic"),
    gettext_noop("Gothic"),
    gettext_noop("Darkwave"),
    gettext_noop("Techno-Industrial"),
    gettext_noop("Electronic"),
    gettext_noop("Pop-Folk"),
    gettext_noop("Eurodance"),
    gettext_noop("Dream"),
    gettext_noop("Southern Rock"),
    gettext_noop("Comedy"),
    gettext_noop("Cult"),
    gettext_noop("Gangsta Rap"),
    gettext_noop("Top 40"),
    gettext_noop("Christian Rap"),
    gettext_noop("Pop/Funk"),
    gettext_noop("Jungle"),
    gettext_noop("Native American"),
    gettext_noop("Cabaret"),
    gettext_noop("New Wave"),
    gettext_noop("Psychedelic"),
    gettext_noop("Rave"),
    gettext_noop("Showtunes"),
    gettext_noop("Trailer"),
    gettext_noop("Lo-Fi"),
    gettext_noop("Tribal"),
    gettext_noop("Acid Punk"),
    gettext_noop("Acid Jazz"),
    gettext_noop("Polka"),
    gettext_noop("Retro"),
    gettext_noop("Musical"),
    gettext_noop("Rock & Roll"),
    gettext_noop("Hard Rock"),
    gettext_noop("Folk"),
    gettext_noop("Folk/Rock"),
    gettext_noop("National Folk"),
    gettext_noop("Swing"),
    gettext_noop("Fast-Fusion"),
    gettext_noop("Bebob"),
    gettext_noop("Latin"),
    gettext_noop("Revival"),
    gettext_noop("Celtic"),
    gettext_noop("Bluegrass"),
    gettext_noop("Avantgarde"),
    gettext_noop("Gothic Rock"),
    gettext_noop("Progressive Rock"),
    gettext_noop("Psychedelic Rock"),
    gettext_noop("Symphonic Rock"),
    gettext_noop("Slow Rock"),
    gettext_noop("Big Band"),
    gettext_noop("Chorus"),
    gettext_noop("Easy Listening"),
    gettext_noop("Acoustic"),
    gettext_noop("Humour"),
    gettext_noop("Speech"),
    gettext_noop("Chanson"),
    gettext_noop("Opera"),
    gettext_noop("Chamber Music"),
    gettext_noop("Sonata"),
    gettext_noop("Symphony"),
    gettext_noop("Booty Bass"),
    gettext_noop("Primus"),
    gettext_noop("Porn Groove"),
    gettext_noop("Satire"),
    gettext_noop("Slow Jam"),
    gettext_noop("Club"),
    gettext_noop("Tango"),
    gettext_noop("Samba"),
    gettext_noop("Folklore"),
    gettext_noop("Ballad"),
    gettext_noop("Power Ballad"),
    gettext_noop("Rhythmic Soul"),
    gettext_noop("Freestyle"),
    gettext_noop("Duet"),
    gettext_noop("Punk Rock"),
    gettext_noop("Drum Solo"),
    gettext_noop("A Cappella"),
    gettext_noop("Euro-House"),
    gettext_noop("Dance Hall"),
    gettext_noop("Goa"),
    gettext_noop("Drum & Bass"),
    gettext_noop("Club-House"),
    gettext_noop("Hardcore"),
    gettext_noop("Terror"),
    gettext_noop("Indie"),
    gettext_noop("BritPop"),
    gettext_noop("Negerpunk"),
    gettext_noop("Polsk Punk"),
    gettext_noop("Beat"),
    gettext_noop("Christian Gangsta Rap"),
    gettext_noop("Heavy Metal"),
    gettext_noop("Black Metal"),
    gettext_noop("Crossover"),
    gettext_noop("Contemporary Christian"),
    gettext_noop("Christian Rock"),
    gettext_noop("Merengue"),
    gettext_noop("Salsa"),
    gettext_noop("Thrash Metal"),
    gettext_noop("Anime"),
    gettext_noop("JPop"),
    gettext_noop("Synthpop"),
};

#define GENRE_NAME_COUNT \
    ((unsigned int)(sizeof genre_names / sizeof (const char *const)))


#define MAX_MP3_SCAN_DEEP 16768
const int max_frames_scan=1024;
enum{ MPEG_ERR=0,MPEG_V1=1,MPEG_V2=2,MPEG_V25=3};

enum{ LAYER_ERR=0,LAYER_1=1,LAYER_2=2,LAYER_3=3};

const unsigned int sync_mask=0xE0FF;
const unsigned int mpeg_ver_mask=0x1800;
const unsigned int mpeg_layer_mask=0x600;
const unsigned int bitrate_mask=0xF00000;
const unsigned int freq_mask=0xC0000;
const unsigned int ch_mask=0xC0000000;
const unsigned int pad_mask=0x20000;

unsigned int bitrate_table[16][6]={
  {0,0,0,0,0,0},
  {32, 32, 32, 32, 32, 8},
  {64, 48, 40, 64, 48, 16},
  {96, 56, 48, 96, 56, 24},
  {128, 64 , 56 , 128, 64 , 32},
  {160, 80 , 64 , 160, 80 , 64},
  {192, 96 , 80 , 192, 96 , 80},
  {224, 112, 96 , 224, 112, 56},
  {256, 128, 112, 256, 128, 64},
  {288, 160, 128, 288, 160, 128},
  {320, 192, 160, 320, 192, 160},
  {352, 224, 192, 352, 224, 112},
  {384, 256, 224, 384, 256, 128},
  {416, 320, 256, 416, 320, 256},
  {448, 384, 320, 448, 384, 320},
  {-1,-1,-1,-1,-1,-1}
};
int freq_table[4][3]={
  {44100,22050,11025},
  {48000,24000,12000},
  {32000,16000,8000}
};


#define OK         0
#define SYSERR     1
#define INVALID_ID3 2

static int get_id3(const char * data,
		   size_t size,
		   id3tag * id3) {
  const char * pos;

  if (size < 128)
    return INVALID_ID3;

  pos = &data[size - 128];
  if (0 != strncmp("TAG",
		   pos, 
		   3))
    return INVALID_ID3;
  pos += 3;

  id3->title = convertToUtf8(pos,
			     30,
			     "ISO-8859-1"); 
  pos += 30;
  id3->artist = convertToUtf8(pos,
			      30,
			      "ISO-8859-1"); 
  pos += 30;
  id3->album = convertToUtf8(pos,
			      30,
			      "ISO-8859-1");
  pos += 30;
  id3->year = convertToUtf8(pos,
			    4,
			    "ISO-8859-1"); 
  pos += 4;
  id3->comment = convertToUtf8(pos,
			       30,
			       "ISO-8859-1");
  pos += 30;
  id3->genre = "";
  if (pos[0] < GENRE_NAME_COUNT)
    id3->genre = dgettext(PACKAGE,
			  genre_names[(unsigned) pos[0]]);
  return OK;
}

static struct EXTRACTOR_Keywords *
addkword(EXTRACTOR_KeywordList *oldhead,
	 const char * phrase,
	 EXTRACTOR_KeywordType type) {

   EXTRACTOR_KeywordList * keyword;

   keyword = malloc(sizeof(EXTRACTOR_KeywordList));
   keyword->next = oldhead;
   keyword->keyword = strdup(phrase);
   keyword->keywordType = type;
   return keyword;
}



static struct EXTRACTOR_Keywords *
mp3parse(const char * data,
	 size_t size,
	 struct EXTRACTOR_Keywords * prev) {
  unsigned int header;
  int counter=0;
  char mpeg_ver=0;
  char layer_ver=0;
  int idx_num=0;	
  int bitrate=0; /*used for each frame*/
  int avg_bps=0; /*average bitrate*/
  int vbr_flag=0;
  int length=0;
  int sample_rate=0;
  int ch=0;
  int frame_size;
  int frames=0;
  size_t pos = 0;
  char * format;

  do {		
    /* seek for frame start */
    if (pos + sizeof(header) > size) {
      return prev;
    }/*unable to find header*/
    memcpy(&header,
	   &data[pos],
	   sizeof(header));
    if ((header&sync_mask)==sync_mask)
      break;/*found header sync*/
    pos++;
    counter++; /*next try*/
  } while(counter<MAX_MP3_SCAN_DEEP);
  if (counter>=MAX_MP3_SCAN_DEEP) {
    return prev;
  };/*give up to find mp3 header*/

  prev = addkword(prev,
		  "audio/mpeg",
		  EXTRACTOR_MIMETYPE);

  do {		/*ok, now we found a mp3 frame header*/
    frames++;
    switch (header & mpeg_ver_mask){
    case 0x1000:
      mpeg_ver = MPEG_ERR; /*error*/
      break;
    case 0x800:
      prev = addkword(prev,
		      "MPEG V2",
		      EXTRACTOR_RESOURCE_TYPE);
      mpeg_ver = MPEG_V2;
      break;
    case 0x1800:
      prev = addkword(prev,
		      "MPEG V1",
		      EXTRACTOR_RESOURCE_TYPE);
      mpeg_ver = MPEG_V1;
      break;
    case 0:	
      prev = addkword(prev,
		      "MPEG V25",
		      EXTRACTOR_RESOURCE_TYPE);
      mpeg_ver = MPEG_V25;
      break;
    }
    switch(header&mpeg_layer_mask){
    case 0x400:
      layer_ver=LAYER_2;
      break;
    case 0x200:
      layer_ver=LAYER_3;
      break;
    case 0x600:
      layer_ver=LAYER_1;
      break;
    case 0:
      layer_ver=LAYER_ERR;/*error*/
    }
    if (!layer_ver||!mpeg_ver)
      return prev; /*unknown mpeg type*/
    if (mpeg_ver<3)
      idx_num=(mpeg_ver-1)*3+layer_ver-1;
    else
      idx_num=2+layer_ver;
    bitrate = 1000*bitrate_table[(header&bitrate_mask)>>20][idx_num];
    if (bitrate<0) {
      frames--;
      break;
    } /*error in header*/
    sample_rate = freq_table[(header&freq_mask)>>18][mpeg_ver-1];
    if (sample_rate<0) {
      frames--;
      break;
    } /*error in header*/
    if ((header&ch_mask)==ch_mask)
      ch=1;
    else
      ch=2; /*stereo non stereo select*/
    frame_size = 144*bitrate/(sample_rate?sample_rate:1)+((header&pad_mask)>>17);
    avg_bps += bitrate/1000;

    pos += frame_size-4;
    if (frames > max_frames_scan)
      break; /*optimization*/
    if (avg_bps/frames!=bitrate/1000)
      vbr_flag=1;
    if (pos + sizeof(header) > size)
      break; /* EOF */
    memcpy(&header,
	   &data[pos],
	   sizeof(header));
  } while ((header&sync_mask)==sync_mask);

  if (!frames)
    return prev; /*no valid frames*/
  avg_bps = avg_bps/frames;
  if (max_frames_scan){ /*if not all frames scaned*/
    length=size/(avg_bps?avg_bps:bitrate?bitrate:0xFFFFFFFF)/125;
  } else{
    length=1152*frames/(sample_rate?sample_rate:0xFFFFFFFF);
  }

  format = malloc(512);
  snprintf(format,
	   512,
	   "%d kbps, %d hz, %dm%02d %s %s",
	   avg_bps,
	   sample_rate,
	   length/60, length % 60, /* minutes / seconds */
	   ch == 2 ? _("stereo") : _("mono"),
	   vbr_flag ? _("(variable bps)"):"");
  prev = addkword(prev,
		  format,
		  EXTRACTOR_FORMAT);
  free(format);
  return prev;
}


/* mimetype = audio/mpeg */
struct EXTRACTOR_Keywords *
libextractor_mp3_extract(const char * filename,
			 const char * data,
			 size_t size,
			 struct EXTRACTOR_Keywords * klist) {
  id3tag info;
  char * word;

  if (0 != get_id3(data, size, &info))
    return klist;

  if (strlen(info.title) > 0)
    klist = addkword(klist, info.title, EXTRACTOR_TITLE);
  if (strlen(info.artist) > 0)
    klist = addkword(klist, info.artist, EXTRACTOR_ARTIST);
  if (strlen(info.album) > 0)
    klist = addkword(klist, info.album, EXTRACTOR_ALBUM);
  if (strlen(info.year) > 0)
    klist = addkword(klist, info.year, EXTRACTOR_DATE);
  if (strlen(info.genre) > 0)
    klist = addkword(klist, info.genre, EXTRACTOR_GENRE);
  if (strlen(info.genre) > 0)
    klist = addkword(klist, info.comment, EXTRACTOR_COMMENT);


  /* A keyword that has all of the information together) */
  word = (char*) malloc(strlen(info.artist) + strlen(info.title) + strlen(info.album) + 6);
  sprintf(word,
	  "%s: %s (%s)",
	  info.artist,
	  info.title,
	  info.album);
  klist = addkword(klist, word, EXTRACTOR_DESCRIPTION);
  free(word);
  free(info.title);
  free(info.year);
  free(info.album);
  free(info.artist);
  free(info.comment);

  return mp3parse(data,
		  size,
		  klist);
}

/* end of mp3extractor.c */
