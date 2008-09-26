/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifndef G_OS_WIN32
#include <sys/mman.h>
#endif

#include "tracker-extract.h"

#define MAX_FILE_READ	  1024 * 1024 * 10
#define MAX_MP3_SCAN_DEEP 16768

typedef struct {
	gchar *text;
	gchar *type;
} Matches;

typedef struct {
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *year;
	gchar *comment;
	gchar *genre;
} id3tag;

enum {
	MPEG_ERR,
	MPEG_V1,
	MPEG_V2,
	MPEG_V25
};

enum {
	LAYER_ERR,
	LAYER_1,
	LAYER_2,
	LAYER_3
};

static void extract_mp3 (const gchar *filename,
			 GHashTable  *metadata);

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

static const guint max_frames_scan = 1024;

static const guint sync_mask = 0xE0FF;
static const guint mpeg_ver_mask = 0x1800;
static const guint mpeg_layer_mask = 0x600;
static const guint bitrate_mask = 0xF00000;
static const guint freq_mask = 0xC0000;
static const guint ch_mask = 0xC0000000;
static const guint pad_mask = 0x20000;

static guint bitrate_table[16][6] = {
	{0  , 0  , 0  , 0  , 0	, 0},
	{32 , 32 , 32 , 32 , 32 , 8},
	{64 , 48 , 40 , 64 , 48 , 16},
	{96 , 56 , 48 , 96 , 56 , 24},
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
	{-1,  -1,  -1,	-1,  -1,  -1}
};

static gint freq_table[4][3] = {
	{44100, 22050, 11025},
	{48000, 24000, 12000},
	{32000, 16000, 8000}
};

static TrackerExtractorData data[] = {
	{ "audio/mpeg", extract_mp3 },
	{ "audio/x-mp3", extract_mp3 },
	{ NULL, NULL }
};

static gchar *
get_utf8 (const gchar *txt,
	  gint	       size,
	  gpointer     p1,
	  gpointer     p2,
	  gpointer     p3)
{
	if (!g_utf8_validate (txt, size, NULL)) {
		return g_locale_to_utf8 (txt, size, NULL, NULL, NULL);
	} else {
		return g_strndup (txt, size);
	}
}

static gboolean
get_id3 (const gchar *data,
	 size_t       size,
	 id3tag      *id3)
{
	const gchar *pos;

	if (size < 128) {
		return FALSE;
	}

	pos = &data[size - 128];

	if (strncmp ("TAG", pos, 3) != 0) {
		return FALSE;
	}

	pos += 3;

	id3->title = get_utf8 (pos, 30, NULL, NULL, NULL);
	pos += 30;
	id3->artist = get_utf8 (pos, 30, NULL, NULL, NULL);
	pos += 30;
	id3->album = get_utf8 (pos, 30, NULL, NULL, NULL);
	pos += 30;
	id3->year = get_utf8 (pos, 4, NULL, NULL, NULL);

	pos += 4;
	id3->comment = get_utf8 (pos, 30, NULL, NULL, NULL);
	pos += 30;
	id3->genre = "";

	if ((guint) pos[0] < G_N_ELEMENTS (genre_names)) {
		id3->genre = g_strdup (genre_names[(unsigned) pos[0]]);
	}

	return TRUE;
}

static void
mp3_parse (const gchar *data,
	   size_t	size,
	   GHashTable  *metadata)
{
	guint header;
	gint counter = 0;
	gchar mpeg_ver = 0;
	gchar layer_ver = 0;
	gint idx_num = 0;
	guint bitrate = 0;
	guint avg_bps = 0;
	gint vbr_flag = 0;
	guint length = 0;
	guint sample_rate = 0;
	gint ch = 0;
	guint frame_size;
	guint frames = 0;
	size_t pos = 0;

	do {
		/* Seek for frame start */
		if (pos + sizeof(header) > size) {
			return;
		}

		memcpy (&header, &data[pos], sizeof (header));

		if ((header&sync_mask) == sync_mask) {
			/* Found header sync */
			break;
		}

		pos++;
		counter++;
	} while (counter < MAX_MP3_SCAN_DEEP);

	if (counter >= MAX_MP3_SCAN_DEEP) {
		/* Give up to find mp3 header */
		return;
	};

	do {
		frames++;
		switch (header & mpeg_ver_mask) {
		case 0x1000:
			mpeg_ver = MPEG_ERR;
			break;
		case 0x800:
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:Codec"),
					     g_strdup ("MPEG"));
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:CodecVersion"),
					     g_strdup ("2"));
			mpeg_ver = MPEG_V2;
			break;
		case 0x1800:
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:Codec"),
					     g_strdup ("MPEG"));
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:CodecVersion"),
					     g_strdup ("1"));
			mpeg_ver = MPEG_V1;
			break;
		case 0:
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:Codec"),
					     g_strdup ("MPEG"));
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:CodecVersion"),
					     g_strdup ("2.5"));
			mpeg_ver = MPEG_V25;
			break;
		}

		switch (header&mpeg_layer_mask) {
		case 0x400:
			layer_ver = LAYER_2;
			break;
		case 0x200:
			layer_ver = LAYER_3;
			break;
		case 0x600:
			layer_ver = LAYER_1;
			break;
		case 0:
			layer_ver = LAYER_ERR;
		}

		if (!layer_ver || !mpeg_ver) {
			/* Unknown mpeg type */
			return;
		}

		if (mpeg_ver<3) {
			idx_num = (mpeg_ver - 1) * 3 + layer_ver - 1;
		} else {
			idx_num = 2 + layer_ver;
		}

		bitrate = 1000 * bitrate_table[(header & bitrate_mask) >> 20][idx_num];

		if (bitrate < 0) {
			frames--;
			break;
		}

		sample_rate = freq_table[(header & freq_mask) >> 18][mpeg_ver - 1];
		if (sample_rate < 0) {
			/* Error in header */
			frames--;
			break;
		}

		if ((header & ch_mask) == ch_mask) {
			ch = 1;
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:Channels"),
					     g_strdup ("1"));
		} else {
			ch=2; /*stereo non stereo select*/
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:Channels"),
					     g_strdup ("2"));
		}

		frame_size = 144 * bitrate / (sample_rate ? sample_rate : 1) + ((header & pad_mask) >> 17);
		avg_bps += bitrate / 1000;

		pos += frame_size - 4;

		if (frames > max_frames_scan) {
			/* Optimization */
			break;
		}

		if (avg_bps / frames != bitrate / 1000) {
			vbr_flag = 1;
		}

		if (pos + sizeof (header) > size) {
			/* EOF */
			break;
		}

		memcpy(&header, &data[pos], sizeof (header));
	} while ((header & sync_mask) == sync_mask);

	if (!frames) {
		/* No valid frames */
		return;
	}

	avg_bps /= frames;

	if (max_frames_scan) {
		/* If not all frames scaned */
		length = size / (avg_bps ? avg_bps : bitrate ? bitrate : 0xFFFFFFFF) / 125;
	} else{
		length = 1152 * frames / (sample_rate ? sample_rate : 0xFFFFFFFF);
	}

	g_hash_table_insert (metadata,
			     g_strdup ("Audio:Duration"),
			     g_strdup_printf ("%d", length));
	g_hash_table_insert (metadata,
			     g_strdup ("Audio:Samplerate"),
			     g_strdup_printf ("%d", sample_rate));
	g_hash_table_insert (metadata,
			     g_strdup ("Audio:Bitrate"),
			     g_strdup_printf ("%d", avg_bps));
}

static void
get_id3v24_tags (const gchar *data,
		 size_t       size,
		 GHashTable  *metadata)
{
	gint	unsync;
	gint	extendedHdr;
	gint	experimental;
	gint	footer;
	guint	tsize;
	guint	pos;
	guint	ehdrSize;
	guint	padding;
	Matches tmap[] = {
		{"COMM", "Audio:Comment"},
		{"TCOP", "File:Copyright"},
		{"TDRC", "Audio:ReleaseDate"},
		{"TCON", "Audio:Genre"},
		{"TIT1", "Audio:Genre"},
		{"TENC", "DC:Publishers"},
		{"TEXT", "Audio:Lyrics"},
		{"TPE1", "Audio:Artist"},
		{"TPE2", "Audio:Artist"},
		{"TPE3", "Audio:Performer"},
		{"TOPE", "Audio:Artist"},
		{"TPUB", "DC:Publishers"},
		{"TOAL", "Audio:Album"},
		{"TALB", "Audio:Album"},
		{"TLAN", "File:Language"},
		{"TIT2", "Audio:Title"},
		{"TIT3", "Audio:Comment"},
		{"WCOP", "File:License"},
		{NULL, 0},
	};

	if ((size < 16) ||
	    (data[0] != 0x49) ||
	    (data[1] != 0x44) ||
	    (data[2] != 0x33) ||
	    (data[3] != 0x04) ||
	    (data[4] != 0x00) ) {
		return;
	}

	unsync = (data[5] & 0x80) > 0;
	extendedHdr = (data[5] & 0x40) > 0;
	experimental = (data[5] & 0x20) > 0;
	footer = (data[5] & 0x10) > 0;
	tsize = (((data[6] & 0x7F) << 21) |
		 ((data[7] & 0x7F) << 14) |
		 ((data[8] & 0x7F) << 7) |
		 ((data[9] & 0x7F) << 0));

	if ((tsize + 10 > size) || (experimental)) {
		return;
	}

	pos = 10;
	padding = 0;

	if (extendedHdr) {
		ehdrSize = (((data[10] & 0x7F) << 21) |
			    ((data[11] & 0x7F) << 14) |
			    ((data[12] & 0x7F) << 7) |
			    ((data[13] & 0x7F) << 0));
		pos += ehdrSize;
	}

	while (pos < tsize) {
		size_t csize;
		gint i;
		unsigned short flags;

		if (pos + 10 > tsize) {
			return;
		}

		csize = (((data[pos+4] & 0x7F) << 21) |
			 ((data[pos+5] & 0x7F) << 14) |
			 ((data[pos+6] & 0x7F) << 7) |
			 ((data[pos+7] & 0x7F) << 0));

		if ((pos + 10 + csize > tsize) ||
		    (csize > tsize) ||
		    (csize == 0)) {
			break;
		}

		flags = (data[pos + 8] << 8) + data[pos + 9];
		if (((flags & 0x80) > 0) ||
		    ((flags & 0x40) > 0)) {
			pos += 10 + csize;
			continue;
		}

		i = 0;
		while (tmap[i].text != NULL) {
			if (strncmp (tmap[i].text, (const char*) &data[pos], 4) == 0) {
				gchar * word;

				if ((flags & 0x20) > 0) {
					/* The "group" identifier, skip a byte */
					pos++;
					csize--;
				}

				/* This byte describes the encoding
				 * try to convert strings to UTF-8
				 * if it fails, then forget it
				 */
				switch (data[pos + 10]) {
				case 0x00:
					word = get_utf8 ((const char*) &data[pos + 11],
							 csize,
							 NULL, NULL, NULL);
					break;
				case 0x01 :
					word = get_utf8 ((const char*) &data[pos + 11],
							 csize,
							 NULL, NULL, NULL);
					break;
				case 0x02 :
					word = get_utf8 ((const char*) &data[pos + 11],
							 csize,
							 NULL, NULL, NULL);
					break;
				case 0x03 :
					word = malloc (csize + 1);
					memcpy (word,
						&data[pos + 11],
						csize);
					word[csize] = '\0';
					break;

				default:
					/* Bad encoding byte,
					 * try to convert from
					 * iso-8859-1
					 */
					word = get_utf8 ((const char*) &data[pos + 11],
							 csize,
							 NULL, NULL, NULL);
					break;
				}

				pos++;
				csize--;

				if (word != NULL && strlen (word) > 0) {
					if (strcmp (tmap[i].text, "COMM") == 0) {
						gchar *s;

						s = g_strdup (word + strlen (word) + 1);
						g_free (word);
						word = s;
					}

					g_hash_table_insert (metadata,
							     g_strdup (tmap[i].type),
							     g_strdup (word));
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
get_id3v23_tags (const gchar *data,
		 size_t       size,
		 GHashTable  *metadata)
{
	gint	unsync;
	gint	extendedHdr;
	gint	experimental;
	guint	tsize;
	guint	pos;
	guint	ehdrSize;
	guint	padding;
	Matches tmap[] = {
		{"COMM", "Audio:Comment"},
		{"TCOP", "File:Copyright"},
		{"TDAT", "Audio:ReleaseDate"},
		{"TCON", "Audio:Genre"},
		{"TIT1", "Audio:Genre"},
		{"TENC", "DC:Publishers"},
		{"TEXT", "Audio:Lyrics"},
		{"TPE1", "Audio:Artist"},
		{"TPE2", "Audio:Artist"},
		{"TPE3", "Audio:Performer"},
		{"TIME", "Audio:ReleaseDate"},
		{"TOPE", "Audio:Artist"},
		{"TPUB", "DC:Publishers"},
		{"TOAL", "Audio:Album"},
		{"TALB", "Audio:Album"},
		{"TLAN", "File:Language"},
		{"TIT2", "Audio:Title"},
		{"WCOP", "File:License"},
		{NULL, 0},
	};

	if ((size < 16) ||
	    (data[0] != 0x49) ||
	    (data[1] != 0x44) ||
	    (data[2] != 0x33) ||
	    (data[3] != 0x03) ||
	    (data[4] != 0x00)) {
		return;
	}

	unsync = (data[5] & 0x80) > 0;
	extendedHdr = (data[5] & 0x40) > 0;
	experimental = (data[5] & 0x20) > 0;
	tsize = (((data[6] & 0x7F) << 21) |
		 ((data[7] & 0x7F) << 14) |
		 ((data[8] & 0x7F) << 7) |
		 ((data[9] & 0x7F) << 0));

	if ((tsize + 10 > size) || (experimental)) {
		return;
	}

	pos = 10;
	padding = 0;

	if (extendedHdr) {
		ehdrSize = (((data[10]) << 24) |
			    ((data[11]) << 16) |
			    ((data[12]) << 8) |
			    ((data[12]) << 0));

		padding	= (((data[15]) << 24) |
			   ((data[16]) << 16) |
			   ((data[17]) << 8) |
			   ((data[18]) << 0));

		pos += 4 + ehdrSize;

		if (padding < tsize)
			tsize -= padding;
		else {
			return;
		}
	}

	while (pos < tsize) {
		size_t csize;
		gint i;
		unsigned short flags;

		if (pos + 10 > tsize) {
			return;
		}

		csize = (data[pos + 4] << 24) +
			(data[pos + 5] << 16) +
			(data[pos + 6] << 8) +
			data[pos + 7];

		if ((pos + 10 + csize > tsize) ||
		    (csize > tsize) ||
		    (csize == 0)) {
			break;
		}

		flags = (data[pos + 8] << 8) + data[pos + 9];

		if (((flags & 0x80) > 0) || ((flags & 0x40) > 0)) {
			pos += 10 + csize;
			continue;
		}

		i = 0;
		while (tmap[i].text != NULL) {
			if (strncmp (tmap[i].text, (const gchar*) &data[pos], 4) == 0) {
				gchar * word;

				if ((flags & 0x20) > 0) {
					/* The "group" identifier, skip a byte */
					pos++;
					csize--;
				}

				csize--;

				/* This byte describes the encoding
				 * try to convert strings to UTF-8 if
				 * it fails, then forget it./
				 */

				switch (data[pos + 10]) {
				case 0x00:
					word = get_utf8 ((const gchar*) &data[pos + 11],
							 csize,
							 NULL, NULL, NULL);
					break;
				case 0x01 :
					word = get_utf8 ((const gchar*) &data[pos + 11],
							 csize,
							 NULL, NULL, NULL);
					break;
				default:
					/* Bad encoding byte, try to
					 * convert from iso-8859-1
					 */
					word = get_utf8 ((const gchar*) &data[pos + 11],
							 csize,
							 NULL, NULL, NULL);
					break;
				}

				pos++;

				if (word != NULL && strlen(word) > 0) {
					if (strcmp (tmap[i].text, "COMM") == 0) {
						gchar *s;

						s = g_strdup (word + strlen (word) + 1);
						g_free (word);
						word = s;
					}

					g_hash_table_insert (metadata,
							     g_strdup (tmap[i].type),
							     g_strdup (word));
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
get_id3v2_tags (const gchar *data,
		size_t	     size,
		GHashTable  *metadata)
{
	gint	unsync;
	guint	tsize;
	guint	pos;
	Matches tmap[] = {
		{"TAL", "Audio:Title"},
		{"TT1", "Audio:Artist"},
		{"TT2", "Audio:Title"},
		{"TT3", "Audio:Title"},
		{"TXT", "Audio:Comment"},
		{"TPB", "DC:Publishers"},
		{"WAF", "DC:Location"},
		{"WAR", "DC:Location"},
		{"WAS", "DC:Location"},
		{"WCP", "File:Copyright"},
		{"WAF", "DC:Location"},
		{"WCM", "File:License"},
		{"TYE", "Audio:ReleaseDate"},
		{"TLA", "File:Lanuguage"},
		{"TP1", "Audio:Artist"},
		{"TP2", "Audio:Artist"},
		{"TP3", "Audio:Performer"},
		{"TEN", "Audio:Performer"},
		{"TCO", "Audio:Title"},
		{"TCR", "File:Copyright"},
		{"SLT", "Audio:Lyrics"},
		{"TOA", "Audio:Artist"},
		{"TOT", "Audio:Album"},
		{"TOL", "Audio:Artist"},
		{"COM", "Audio:Comment"},
		{ NULL, 0},
	};

	if ((size < 16) ||
	    (data[0] != 0x49) ||
	    (data[1] != 0x44) ||
	    (data[2] != 0x33) ||
	    (data[3] != 0x02) ||
	    (data[4] != 0x00)) {
		return;
	}

	unsync = (data[5] & 0x80) > 0;
	tsize = (((data[6] & 0x7F) << 21) |
		 ((data[7] & 0x7F) << 14) |
		 ((data[8] & 0x7F) << 07) |
		 ((data[9] & 0x7F) << 00));

	if (tsize + 10 > size)	{
		return;
	}

	pos = 10;

	while (pos < tsize) {
		size_t csize;
		gint i;

		if (pos + 6 > tsize)  {
			return;
		}

		csize = (data[pos+3] << 16) + (data[pos + 4] << 8) + data[pos + 5];
		if ((pos + 6 + csize > tsize) ||
		    (csize > tsize) ||
		    (csize == 0)) {
			break;
		}

		i = 0;

		while (tmap[i].text != NULL) {
			if (strncmp(tmap[i].text, (const char*) &data[pos], 3) == 0) {
				gchar * word;

				/* This byte describes the encoding
				 * try to convert strings to UTF-8 if
				 * it fails, then forget it.
				 */

				switch (data[pos + 6]) {
				case 0x00:
					word = get_utf8 ((const gchar*) &data[pos + 7],
							 csize,
							 NULL, NULL, NULL);
					break;

				default:
					/* Bad encoding byte, try to
					 * convert from iso-8859-1.
					 */
					word = get_utf8 ((const gchar*) &data[pos + 7],
							 csize,
							 NULL, NULL, NULL);
					break;
				}

				pos++;
				csize--;

				if (word != NULL && strlen(word) > 0) {
					if (strcmp (tmap[i].text, "COM") == 0) {
						gchar *s;

						s = g_strdup (word + strlen(word) + 1);
						g_free (word);
						word = s;
					}

					g_hash_table_insert (metadata,
							     g_strdup (tmap[i].type),
							     g_strdup (word));
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

static void
extract_mp3 (const gchar *filename,
	     GHashTable  *metadata)
{
	gint	     file;
	void	    *buffer;
	struct stat  fstatbuf;
	size_t	     size;
	id3tag	     info;

	info.title = NULL;
	info.artist = NULL;
	info.album = NULL;
	info.year = NULL;
	info.comment = NULL;
	info.genre = NULL;

#if defined(__linux__)
	file = g_open (filename, (O_RDONLY | O_NOATIME), 0);
#else
	file = g_open (filename, O_RDONLY, 0);
#endif

	if (file == -1 || stat (filename, &fstatbuf) == -1) {
		close (file);
		return;
	}

	size = fstatbuf.st_size;
	if (size == 0) {
		close (file);
		return;
	}

	if (size >  MAX_FILE_READ) {
		size =	MAX_FILE_READ;
	}

#ifndef G_OS_WIN32
	buffer = mmap (NULL, size, PROT_READ, MAP_PRIVATE, file, 0);
#endif

	if (buffer == NULL || buffer == (void*) -1) {
		close(file);
		return;
	}

	if (!get_id3 (buffer, size, &info)) {
		/* Do nothing? */
	}

	if (info.title && strlen (info.title) > 0) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Title"),
				     g_strdup (info.title));
	}

	if (info.artist && strlen (info.artist) > 0) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Artist"),
				     g_strdup (info.artist));
	}

	if (info.album && strlen (info.album) > 0) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Album"),
				     g_strdup (info.album));
	}

	if (info.year && strlen (info.year) > 0) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:ReleaseDate"),
				     g_strdup (info.year));
	}

	if (info.genre && strlen (info.genre) > 0) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Genre"),
				     g_strdup (info.genre));
	}

	if (info.comment && strlen (info.comment) > 0) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Comment"),
				     g_strdup (info.comment));
	}

	free (info.title);
	free (info.year);
	free (info.album);
	free (info.artist);
	free (info.comment);

	/* Get other embedded tags */
	get_id3v2_tags (buffer, size, metadata);
	get_id3v23_tags (buffer, size, metadata);
	get_id3v24_tags (buffer, size, metadata);

	/* Get mp3 stream info */
	mp3_parse (buffer, size, metadata);

	/* Check that we have the minimum data. FIXME We should not need to do this */
	if (!g_hash_table_lookup (metadata, "Audio:Title")) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Title"),
				     g_strdup ("tracker:unknown"));
	}

	if (!g_hash_table_lookup (metadata, "Audio:Album")) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Album"),
				     g_strdup ("tracker:unknown"));
	}

	if (!g_hash_table_lookup (metadata, "Audio:Artist")) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Artist"),
				     g_strdup ("tracker:unknown"));
	}

	if (!g_hash_table_lookup (metadata, "Audio:Genre")) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Genre"),
				     g_strdup ("tracker:unknown"));
	}

#ifndef G_OS_WIN32
	munmap (buffer, size);
#endif
	close(file);
}

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
