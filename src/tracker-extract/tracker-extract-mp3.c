/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>


#define MAX_FILE_READ 1024 * 1024 * 10


typedef struct {
	char * text;
	char * type;
} Matches;

static struct {
	char * name;
	char *meta_name;
	gboolean writable;
} tags[] = {
	 {"title", "Audio.Title", FALSE},
	 {"artist", "Audio.Artist", FALSE},
	 {"album", "Audio.Album", FALSE},
	 {"albumartist", "Audio.AlbumArtist", FALSE},
	 {"trackcount", "Audio.AlbumTrackCount", FALSE},
	 {"tracknumber", "Audio.TrackNo", FALSE},
	 {"DiscNo", "Audio.DiscNo", FALSE},
	 {"Performer", "Audio.Performer", FALSE},
	 {"TrackGain", "Audio.TrackGain", FALSE},
	 {"TrackPeakGain", "Audio.TrackPeakGain", FALSE},
	 {"AlbumGain", "Audio.AlbumGain", FALSE},
	 {"AlbumPeakGain", "Audio.AlbumPeakGain", FALSE},
	 {"date", "Audio.ReleaseDate", FALSE},
	 {"comment", "Audio.Comment", FALSE},
	 {"genre", "Audio.Genre", FALSE},
	 {"Codec", "Audio.Codec", FALSE},
	 {"CodecVersion", "Audio.CodecVersion", FALSE},
	 {"Samplerate", "Audio.Samplerate", FALSE},
	 {"Channels", "Audio.Channels", FALSE},
	 {"MBAlbumID", "Audio.MBAlbumID", FALSE},
	 {"MBArtistID", "Audio.MBArtistID", FALSE},
	 {"MBAlbumArtistID", "Audio.MBAlbumArtistID", FALSE},
	 {"MBTrackID", "Audio.MBTrackID", FALSE},
	 {"Lyrics", "Audio.Lyrics", FALSE},
	 {"Copyright", "File.Copyright", FALSE},
	 {"License", "File.License", FALSE},
	 {"Organization", "File.Organization", FALSE},
	 {"Location", "File.Location", FALSE},
	 {"Publisher", "File.Publisher", FALSE},
	 {NULL, NULL, FALSE},
};


typedef struct {
	char * title;
	char * artist;
	char * album;
	char * year;
	char * comment;
	char * genre;
} id3tag;

static const char *const genre_names[] = {
	"Blues",
	"Classic Rock",
	"Country",
	"Dance",
	"Disco",
	"Funk",
	"Grunge",
	"Hip-Hop",
	"Jazz",
	"Metal",
	"New Age",
	"Oldies",
	"Other",
	"Pop",
	"R&B",
	"Rap",
	"Reggae",
	"Rock",
	"Techno",
	"Industrial",
	"Alternative",
	"Ska",
	"Death Metal",
	"Pranks",
	"Soundtrack",
	"Euro-Techno",
	"Ambient",
	"Trip-Hop",
	"Vocal",
	"Jazz+Funk",
	"Fusion",
	"Trance",
	"Classical",
	"Instrumental",
	"Acid",
	"House",
	"Game",
	"Sound Clip",
	"Gospel",
	"Noise",
	"Alt. Rock",
	"Bass",
	"Soul",
	"Punk",
	"Space",
	"Meditative",
	"Instrumental Pop",
	"Instrumental Rock",
	"Ethnic",
	"Gothic",
	"Darkwave",
	"Techno-Industrial",
	"Electronic",
	"Pop-Folk",
	"Eurodance",
	"Dream",
	"Southern Rock",
	"Comedy",
	"Cult",
	"Gangsta Rap",
	"Top 40",
	"Christian Rap",
	"Pop/Funk",
	"Jungle",
	"Native American",
	"Cabaret",
	"New Wave",
	"Psychedelic",
	"Rave",
	"Showtunes",
	"Trailer",
	"Lo-Fi",
	"Tribal",
	"Acid Punk",
	"Acid Jazz",
	"Polka",
	"Retro",
	"Musical",
	"Rock & Roll",
	"Hard Rock",
	"Folk",
	"Folk/Rock",
	"National Folk",
	"Swing",
	"Fast-Fusion",
	"Bebob",
	"Latin",
	"Revival",
	"Celtic",
	"Bluegrass",
	"Avantgarde",
	"Gothic Rock",
	"Progressive Rock",
	"Psychedelic Rock",
	"Symphonic Rock",
	"Slow Rock",
	"Big Band",
	"Chorus",
	"Easy Listening",
	"Acoustic",
	"Humour",
	"Speech",
	"Chanson",
	"Opera",
	"Chamber Music",
	"Sonata",
	"Symphony",
	"Booty Bass",
	"Primus",
	"Porn Groove",
	"Satire",
	"Slow Jam",
	"Club",
	"Tango",
	"Samba",
	"Folklore",
	"Ballad",
	"Power Ballad",
	"Rhythmic Soul",
	"Freestyle",
	"Duet",
	"Punk Rock",
	"Drum Solo",
	"A Cappella",
	"Euro-House",
	"Dance Hall",
	"Goa",
	"Drum & Bass",
	"Club-House",
	"Hardcore",
	"Terror",
	"Indie",
	"BritPop",
	"Negerpunk",
	"Polsk Punk",
	"Beat",
	"Christian Gangsta Rap",
	"Heavy Metal",
	"Black Metal",
	"Crossover",
	"Contemporary Christian",
	"Christian Rock",
	"Merengue",
	"Salsa",
	"Thrash Metal",
	"Anime",
	"JPop",
	"Synthpop"
};


#define GENRE_NAME_COUNT \
    ((unsigned int)(sizeof genre_names / sizeof (const char *const)))


#define MAX_MP3_SCAN_DEEP 16768
const unsigned int max_frames_scan=1024;
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

static char *
get_utf8 (const char *txt, int size, gpointer p1, gpointer p2, gpointer p3)
{
	if (!g_utf8_validate (txt, size, NULL)) {
		return g_locale_to_utf8 (txt, size, NULL, NULL, NULL);
	} else {
		return g_strndup (txt, size);
	}
}

static int get_id3 (const char * data,
		    size_t size,
		    id3tag * id3) {

	const char * pos;


	if (size < 128) return INVALID_ID3;

	pos = &data[size - 128];

  	if (0 != strncmp ("TAG",
			   pos, 
			   3)) {
		return INVALID_ID3;
	}

	pos += 3;

	id3->title = get_utf8 (pos,
			       30,
			       NULL, NULL, NULL); 
	pos += 30;
	id3->artist = get_utf8 (pos,
				      30,
				       NULL, NULL, NULL); 
	pos += 30;
	id3->album = get_utf8 (pos,
				      30,
				       NULL, NULL, NULL);
  	pos += 30;
  	id3->year = get_utf8 (pos, 4, NULL, NULL, NULL);

  	pos += 4;
  	id3->comment = get_utf8 (pos,
				       30,
				        NULL, NULL, NULL);
  	pos += 30;
  	id3->genre = "";
  	if ( (unsigned int)pos[0] < GENRE_NAME_COUNT) {
    		id3->genre = g_strdup (genre_names[(unsigned) pos[0]]);
	}

	return OK;
}


gboolean
tracker_metadata_mp3_is_writable (const char *meta)
{
	int i;

	i = 0;
	while (tags[i].name != NULL) {
		
		if (strcmp (tags[i].meta_name, meta) == 0) {
			return tags[i].writable;
		}
		
		i++;
	}

	return FALSE;

}


gboolean
tracker_metadata_mp3_write (const char *meta_name, const char *value) 
{
	/* to do */
	return FALSE;
}



static void
mp3_parse (const char *data, size_t size, GHashTable *metadata) 
{

	unsigned int header;
	int counter=0;
	char mpeg_ver=0;
	char layer_ver=0;
	int idx_num=0;	
	unsigned int bitrate=0; /*used for each frame*/
	unsigned int avg_bps=0; /*average bitrate*/
	int vbr_flag=0;
	unsigned int length=0;
	unsigned int sample_rate=0;
	int ch=0;
	unsigned int frame_size;
	unsigned int frames=0;
	size_t pos = 0;

	do {		
		/* seek for frame start */
		if (pos + sizeof(header) > size) {
			return;
		}/*unable to find header*/

		memcpy (&header, &data[pos], sizeof (header));

		if ((header&sync_mask) == sync_mask) break;/*found header sync*/

		pos++;
		counter++; /*next try*/

	} while (counter < MAX_MP3_SCAN_DEEP);

	if (counter>=MAX_MP3_SCAN_DEEP) {
		return;
	};/*give up to find mp3 header*/


	do {		/*ok, now we found a mp3 frame header*/
		frames++;
		switch (header & mpeg_ver_mask){
		case 0x1000:
			mpeg_ver = MPEG_ERR; /*error*/
			break;
		case 0x800:
			g_hash_table_insert (metadata, g_strdup ("Audio.Codec"), g_strdup ("MPEG"));
			g_hash_table_insert (metadata, g_strdup ("Audio.CodecVersion"), g_strdup ("2"));
			mpeg_ver = MPEG_V2;
			break;
		case 0x1800:
			g_hash_table_insert (metadata, g_strdup ("Audio.Codec"), g_strdup ("MPEG"));
			g_hash_table_insert (metadata, g_strdup ("Audio.CodecVersion"), g_strdup ("1"));
			mpeg_ver = MPEG_V1;
			break;
		case 0:	
			g_hash_table_insert (metadata, g_strdup ("Audio.Codec"), g_strdup ("MPEG"));
			g_hash_table_insert (metadata, g_strdup ("Audio.CodecVersion"), g_strdup ("2.5"));
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
			return; /*unknown mpeg type*/
		if (mpeg_ver<3)
			idx_num=(mpeg_ver-1)*3+layer_ver-1;
		else
			idx_num=2+layer_ver;

		bitrate = 1000 * bitrate_table[(header&bitrate_mask)>>20][idx_num];
		
		if (bitrate<0) {
			frames--;
			break;
		} 

		sample_rate = freq_table[(header&freq_mask)>>18][mpeg_ver-1];
		if (sample_rate<0) {
			frames--;
			break;
		} /*error in header*/

		if ((header&ch_mask)==ch_mask) {
			ch=1;
			g_hash_table_insert (metadata, g_strdup ("Audio.Channels"), g_strdup ("1"));
		} else {
			ch=2; /*stereo non stereo select*/
			g_hash_table_insert (metadata, g_strdup ("Audio.Channels"), g_strdup ("2"));
		}

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

	if (!frames) return; /*no valid frames*/

	avg_bps = avg_bps/frames;

	if (max_frames_scan){ /*if not all frames scaned*/
		length=size/(avg_bps?avg_bps:bitrate?bitrate:0xFFFFFFFF)/125;
	} else{
		length=1152*frames/(sample_rate?sample_rate:0xFFFFFFFF);
	}

	g_hash_table_insert (metadata, g_strdup ("Audio.Duration"), g_strdup_printf ("%d", length));
	g_hash_table_insert (metadata, g_strdup ("Audio.Samplerate"), g_strdup_printf ("%d", sample_rate));
	g_hash_table_insert (metadata, g_strdup ("Audio.Bitrate"), g_strdup_printf ("%d", avg_bps));

}


static void
get_id3v24_tags (const char *data, size_t size, GHashTable *metadata) 
{
	Matches tmap[] = {
		{"COMM", "Audio.Comment"},
		{"TCOP", "File.Copyright"},
		{"TDRC", "Audio.ReleaseDate"},
		{"TCON", "Audio.Genre"},
		{"TIT1", "Audio.Genre"},
		{"TENC", "File.Publisher"},
		{"TEXT", "Audio.Lyrics"},
		{"TPE1", "Audio.Artist"},
		{"TPE2", "Audio.Artist"},
		{"TPE3", "Audio.Performer"},
		{"TOPE", "Audio.Artist"},
		{"TPUB", "File.Publisher"},
		{"TOAL", "Audio.Album"},
		{"TALB", "Audio.Album"},
		{"TLAN", "File.Language"},
		{"TIT2", "Audio.Title"},
		{"TIT3", "Audio.Comment"},
		{"WCOP", "File.License"},
		{NULL, 0},
	};

	

	int unsync;
	int extendedHdr;
	int experimental;
	int footer;
	unsigned int tsize;
	unsigned int pos;
	unsigned int ehdrSize;
	unsigned int padding;

	if ( (size < 16) ||
			 (data[0] != 0x49) ||
			 (data[1] != 0x44) ||
			 (data[2] != 0x33) ||
			 (data[3] != 0x04) ||
			 (data[4] != 0x00) ) {

		////g_print ("no id3v24 tags info (stage 1)\n");
		return;
	}

	unsync = (data[5] & 0x80) > 0;
	extendedHdr = (data[5] & 0x40) > 0;
	experimental = (data[5] & 0x20) > 0;
	footer = (data[5] & 0x10) > 0;
	tsize = ( ( (data[6] & 0x7F) << 21 ) |
			( (data[7] & 0x7F) << 14 ) |
			( (data[8] & 0x7F) << 7 ) |
			( (data[9] & 0x7F) << 0 ) );

	if ( (tsize + 10 > size) || (experimental) ) {

		////g_print ("no id3v24 tags info (stage 2)\n");
		return;
	}
	

	pos = 10;
	padding = 0;
	if (extendedHdr) {
		ehdrSize = ( ( (data[10] & 0x7F) << 21 ) |
		 ( (data[11] & 0x7F) << 14 ) |
		 ( (data[12] & 0x7F) << 7 ) |
		 ( (data[13] & 0x7F) << 0 ) );
		pos += ehdrSize;
	}


	while (pos < tsize) {
		size_t csize;
		int i;
		unsigned short flags;

		if (pos + 10 > tsize) {

			////g_print ("no id3v24 tags info (stage 3)\n");
			return;
		}

		csize = ( ( (data[pos+4] & 0x7F) << 21 ) |
				( (data[pos+5] & 0x7F) << 14 ) |
				( (data[pos+6] & 0x7F) <<	7 ) |
				( (data[pos+7] & 0x7F) <<	0 ) );

		if ( (pos + 10 + csize > tsize) ||
			 (csize > tsize) ||
			 (csize == 0) )
			break;
		
		flags = (data[pos+8]<<8) + data[pos+9];
		if ( ( (flags & 0x80) > 0) /* compressed, not yet supported */ ||
				 ( (flags & 0x40) > 0) /* encrypted, not supported */ ) {
			pos += 10 + csize;
			continue;
		}

		i = 0;
		while (tmap[i].text != NULL) {
			if (0 == strncmp(tmap[i].text,
					 (const char*) &data[pos],
					 4)) {
				char * word;
				if ( (flags & 0x20) > 0) {
					/* "group" identifier, skip a byte */
					pos++;
					csize--;
				}

				/* this byte describes the encoding
				 try to convert strings to UTF-8
				 if it fails, then forget it */
				switch (data[pos+10]) {
					case 0x00 :
						word = get_utf8 ((const char*) &data[pos+11],
						 csize,
						  NULL, NULL, NULL);
						break;
					case 0x01 :
						word = get_utf8((const char*) &data[pos+11],
						 csize,
						  NULL, NULL, NULL);
						break;
					case 0x02 :
						word = get_utf8((const char*) &data[pos+11],
						 csize,
						 NULL, NULL, NULL);
						break;
					case 0x03 :
						word = malloc(csize+1);	
						memcpy(word,
						 &data[pos+11],
						 csize);
						word[csize] = '\0';
						break;

					default:
						/* bad encoding byte,
						 try to convert from iso-8859-1 */
						word = get_utf8((const char*) &data[pos+11],
							 csize,
							 NULL, NULL, NULL);
						break;
				}
				pos++;
				csize--;
				if ((word != NULL) && (strlen(word) > 0)) {

					if (strcmp (tmap[i].text, "COMM") == 0) {
						char *s = g_strdup (word+strlen(word)+1);
						g_free (word);
						word = s;
					}

					g_hash_table_insert (metadata, g_strdup (tmap[i].type), g_strdup (word));
				} else {
					g_free (word);
				}
				break;
			}
			i++;
		}
		pos += 10 + csize;
	}
}


static void
get_id3v23_tags (const char *data, size_t size, GHashTable *metadata) 
{
 	Matches tmap[] = {
		{"COMM", "Audio.Comment"},
		{"TCOP", "File.Copyright"},
		{"TDAT", "Audio.ReleaseDate"},
		{"TCON", "Audio.Genre"},
		{"TIT1", "Audio.Genre"},
		{"TENC", "File.Publisher"},
		{"TEXT", "Audio.Lyrics"},
		{"TPE1", "Audio.Artist"},
		{"TPE2", "Audio.Artist"},
		{"TPE3", "Audio.Performer"},
		{"TIME", "Audio.ReleaseDate"},
		{"TOPE", "Audio.Artist"},
		{"TPUB", "File.Publisher"},
		{"TOAL", "Audio.Album"},
		{"TALB", "Audio.Album"},
		{"TLAN", "File.Language"},
		{"TIT2", "Audio.Title"},
		{"WCOP", "File.License"},
		{NULL, 0},
	};

	int unsync;
	int extendedHdr;
	int experimental;
	unsigned int tsize;
	unsigned int pos;
	unsigned int ehdrSize;
	unsigned int padding;

	if ( (size < 16) ||
			 (data[0] != 0x49) ||
			 (data[1] != 0x44) ||
			 (data[2] != 0x33) ||
			 (data[3] != 0x03) ||
			 (data[4] != 0x00) ) {

		//g_print ("no id3v23 tags info (stage 1)\n");
		return;
	}
	unsync = (data[5] & 0x80) > 0;
	extendedHdr = (data[5] & 0x40) > 0;
	experimental = (data[5] & 0x20) > 0;
	tsize = ( ( (data[6] & 0x7F) << 21 ) |
			( (data[7] & 0x7F) << 14 ) |
			( (data[8] & 0x7F) << 7 ) |
			( (data[9] & 0x7F) << 0 ) );
	if ( (tsize + 10 > size) || (experimental) ) {

		//g_print ("no id3v23 tags info (stage 2)\n");
		return;
	}
	pos = 10;
	padding = 0;
	if (extendedHdr) {
		ehdrSize = ( ( (data[10]) << 24 ) |
		 ( (data[11]) << 16 ) |
		 ( (data[12]) << 8 ) |
		 ( (data[12]) << 0 ) );

		padding	= ( ( (data[15]) << 24 ) |
		 ( (data[16]) << 16 ) |
		 ( (data[17]) << 8 ) |
		 ( (data[18]) << 0 ) );
		pos += 4 + ehdrSize;
		if (padding < tsize)
			tsize -= padding;
		else {
			//g_print ("no id3v23 tags info (stage 3)\n");
			return;
		}
	}


	while (pos < tsize) {
		size_t csize;
		int i;
		unsigned short flags;

		if (pos + 10 > tsize) {

			//g_print ("no id3v23 tags info (stage 4)\n");
			return;
		}
		csize = (data[pos+4] << 24) + (data[pos+5] << 16) + (data[pos+6] << 8) + data[pos+7];
		if ( (pos + 10 + csize > tsize) ||
	 	(csize > tsize) ||
	 	(csize == 0) )
			break;
		flags = (data[pos+8]<<8) + data[pos+9];
		if ( ( (flags & 0x80) > 0) /* compressed, not yet supported */ ||
	 		( (flags & 0x40) > 0) /* encrypted, not supported */ ) {
			pos += 10 + csize;
			continue;
		}
		i = 0;
		while (tmap[i].text != NULL) {
			if (0 == strncmp(tmap[i].text,
					 (const char*) &data[pos],
					 4)) {
				char * word;
				if ( (flags & 0x20) > 0) {
					/* "group" identifier, skip a byte */
					pos++;
					csize--;
				}
				csize--;
				/* this byte describes the encoding
				 try to convert strings to UTF-8
				 if it fails, then forget it */
			
				switch (data[pos+10]) {
					case 0x00 :
						word = get_utf8((const char*) &data[pos+11],
						 csize,
						NULL, NULL, NULL);
						break;
					case 0x01 :
						word = get_utf8((const char*) &data[pos+11],
						 csize,
						 NULL, NULL, NULL);
						break;
					default:
						/* bad encoding byte,
						 try to convert from iso-8859-1 */
						word = get_utf8((const char*) &data[pos+11],
						 csize,
						NULL, NULL, NULL);
						break;
				}

				pos++;
				if ((word != NULL) && (strlen(word) > 0)) {
					
					if (strcmp (tmap[i].text, "COMM") == 0) {
						char *s = g_strdup (word+strlen(word)+1);
						g_free (word);
						word = s;
					}

					g_hash_table_insert (metadata, g_strdup (tmap[i].type), g_strdup (word));
				} else {
					g_free (word);
				}
				break;
			}
			i++;
		}
		pos += 10 + csize;
	}

}


static void
get_id3v2_tags (const char *data, size_t size, GHashTable *metadata) 
{

	Matches tmap[] = {
		{"TAL", "Audio.Title"},
		{"TT1", "Audio.Artist"},
		{"TT2", "Audio.Title"},
		{"TT3", "Audio.Title"},
		{"TXT", "Audio.Comment"},
		{"TPB", "File.Publisher"},
		{"WAF", "File.Location"},
		{"WAR", "File.Location"},
		{"WAS", "File.Location"},
		{"WCP", "File.Copyright"},
		{"WAF", "File.Location"},
		{"WCM", "File.License"},
		{"TYE", "Audio.ReleaseDate"},
		{"TLA", "File.Lanuguage"},
		{"TP1", "Audio.Artist"},
		{"TP2", "Audio.Artist"},
		{"TP3", "Audio.Performer"},
		{"TEN", "Audio.Performer"},
		{"TCO", "Audio.Title"},
		{"TCR", "File.Copyright"},
		{"SLT", "Audio.Lyrics"},
		{"TOA", "Audio.Artist"},
		{"TOT", "Audio.Album"},
		{"TOL", "Audio.Artist"},
		{"COM", "Audio.Comment"},
		{ NULL, 0},
	};
	
	int unsync;
	unsigned int tsize;
	unsigned int pos;

	if ((size < 16) ||
			 (data[0] != 0x49) ||
			 (data[1] != 0x44) ||
			 (data[2] != 0x33) ||
			 (data[3] != 0x02) ||
			 (data[4] != 0x00)) {

		//g_print ("no id3v2 tags info (stage 1)\n");
		return;
	}

	unsync = (data[5] & 0x80) > 0;
	tsize = (((data[6] & 0x7F) << 21) |
			((data[7] & 0x7F) << 14) |
			((data[8] & 0x7F) << 07) |
			((data[9] & 0x7F) << 00));

	if (tsize + 10 > size)  {

		//g_print ("no id3v2 tags info (stage 2)\n");
		return;
	}

	pos = 10;
	while (pos < tsize) {
		size_t csize;
		int i;

		if (pos + 6 > tsize)  {

			//g_print ("no id3v2 tags info (stage 3)\n");
			return;
		}

		csize = (data[pos+3] << 16) + (data[pos+4] << 8) + data[pos+5];
		if ((pos + 6 + csize > tsize) ||
			 (csize > tsize) ||
			 (csize == 0))
			break;


		i = 0;
		while (tmap[i].text != NULL) {

			if (0 == strncmp(tmap[i].text,
					 (const char*) &data[pos],
					 3)) {
				char * word;

	
				/* this byte describes the encoding
				 try to convert strings to UTF-8
				 if it fails, then forget it */

				switch (data[pos+6]) {
					case 0x00:
						word = get_utf8((const char*) &data[pos+7],
						 csize,
						 NULL, NULL, NULL);
					break;
					
					default:
						/* bad encoding byte,
						 try to convert from iso-8859-1 */
						word = get_utf8((const char*) &data[pos+7],
						 csize,
						 NULL, NULL, NULL);
						break;
				}
				pos++;
				csize--;

				if ((word != NULL) && (strlen(word) > 0)) {

					if (strcmp (tmap[i].text, "COM") == 0) {
						char *s = g_strdup (word+strlen(word)+1);
						g_free (word);
						word = s;
					}

					g_hash_table_insert (metadata, g_strdup (tmap[i].type), g_strdup (word));
				} else {
					g_free (word);
				}

				break;
			}
			i++;
		}
		pos += 6 + csize;
	}


}



void 
tracker_extract_mp3 (const char *filename, GHashTable *metadata)
{
	int file;
	void * buffer;
	struct stat fstatbuf;
	size_t size;
  	id3tag info;

	info.title = NULL;
	info.artist = NULL;
	info.album = NULL;
	info.year = NULL;
	info.comment = NULL;
	info.genre = NULL;

	file = open (filename, O_RDONLY, 0);

	if ((file == -1) || (stat (filename, &fstatbuf) == -1)) {
		close(file);	
		//g_print ("could not open file %s\n", filename);
		return;
	}

	size = fstatbuf.st_size;
	if (size == 0) {
		//g_print ("could not stat file %s\n", filename);
		close(file);	
		return;
	}

	
  	if (size >  MAX_FILE_READ) {
		size =  MAX_FILE_READ;
	}

	//g_print ("file size is %d\n", size);
	buffer = mmap (NULL, size, PROT_READ, MAP_PRIVATE, file, 0);

  	if ((buffer == NULL) || (buffer == (void *) -1)) {
		//g_print ("mmap failure\n");
		close(file);
		return;
  	}



	if (get_id3 (buffer, size, &info) != 0) {
		//g_print ("no id3 info detected\n");
	}


	if (info.title && strlen (info.title) > 0) {
		g_hash_table_insert (metadata, g_strdup ("Audio.Title"), g_strdup (info.title));
	}

	if (info.artist && strlen (info.artist) > 0) {
		g_hash_table_insert (metadata, g_strdup ("Audio.Artist"), g_strdup (info.artist));
	}

	if (info.album && strlen (info.album) > 0) {
		g_hash_table_insert (metadata, g_strdup ("Audio.Album"), g_strdup (info.album));
	}

	if (info.year && strlen (info.year) > 0) {
		g_hash_table_insert (metadata, g_strdup ("Audio.ReleaseDate"), g_strdup (info.year));
	}

	if (info.genre && strlen (info.genre) > 0) {
		g_hash_table_insert (metadata, g_strdup ("Audio.Genre"), g_strdup (info.genre));
	}

	if (info.comment && strlen (info.comment) > 0) {
		g_hash_table_insert (metadata, g_strdup ("Audio.Comment"), g_strdup (info.comment));
	}

	free(info.title);
	free(info.year);
	free(info.album);
	free(info.artist);
	free(info.comment);

	/* get other embedded tags */

	get_id3v2_tags (buffer, size, metadata);
	get_id3v23_tags (buffer, size, metadata);
	get_id3v24_tags (buffer, size, metadata);

	/* get mp3 stream info */
	mp3_parse (buffer, size, metadata);


  	munmap (buffer, size);
  	close(file);
	
}

