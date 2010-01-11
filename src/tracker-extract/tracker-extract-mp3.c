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
 * * You should have received a copy of the GNU General Public
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
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifndef G_OS_WIN32
#include <sys/mman.h>
#endif /* G_OS_WIN32 */

#ifdef HAVE_ENCA
#include <enca.h>
#endif

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"
#include "tracker-extract-albumart.h"
#include "tracker-escape.h"
#include "tracker-utils.h"

/* We mmap the beginning of the file and read separately the last 128
 * bytes for id3v1 tags. While these are probably cornercases the
 * rationale is that we don't want to fault a whole page for the last
 * 128 bytes and on the other we don't want to mmap the whole file
 * with unlimited size (might need to create private copy in some
 * special cases, finding continuous space etc). We now take 5 first
 * MB of the file and assume that this is enough. In theory there is
 * no maximum size as someone could embed 50 gigabytes of albumart
 * there. 
 */

#define MAX_FILE_READ	  1024 * 1024 * 5
#define MAX_MP3_SCAN_DEEP 16768

#define MAX_FRAMES_SCAN   512
#define VBR_THRESHOLD     16

#define ID3V1_SIZE        128

typedef struct {
	const gchar *text;
	const gchar *type;
} tag_matches;

typedef struct {
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *year;
	gchar *comment;
	gchar *trackno;
	gchar *genre;
	gchar *encoding;
} tag_info;

typedef struct {
	size_t         size;
	size_t         id3v2_size;

	guint32        duration;

	unsigned char *albumartdata;
	size_t         albumartsize;
	gchar         *albumartmime;
} file_data;

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

/* This list is based on the comprehensive list on the French wiki
 * page here:
 * 
 *   http://fr.wikipedia.org/wiki/ID3
 * 
 * The actual list as explained by the standard is available here but
 * has some ~17 or so genres missing which are on the French list:
 *
 *   http://www.id3.org/id3v2.3.0#head-129376727ebe5309c1de1888987d070288d7c7e7
 * 
 * Since the index is the most important thing here and this list is
 * not sorted alphabetically, all new IDs are only ever appended to
 * the list and that's why we can still use the French ID3 list over
 * the actual list on the standards website.
 */
static const char *const genre_names[] = {
	/* Standard genres */
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
	"AlternRock",
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
	"Gangsta",
	"Top 40",
	"Christian Rap",
	"Pop/Funk",
	"Jungle",
	"Native American",
	"Cabaret",
	"New Wave",
	"Psychadelic",
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

	/* Added on December 12, 1997 in cooperation with Winamp: */
	"Folk",
	"Folk-Rock",
	"National Folk",
	"Swing",
	"Fast Fusion",
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

	/* Added on January 26, 1998 to ensure compatibility with Winamp 1.7: */
	"Slow Jam",
	"Club",
	"Tango",
	"Samba",
	"Folklore",

	/* Added on April 13, 1998 to ensure compatibility with Winamp 1.90: */
	"Ballad",
	"Power Ballad",
	"Rhythmic Soul",
	"Freestyle",
	"Duet",
	"Punk Rock",
	"Drum Solo",
	"A capella",
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

	/* Added on Jun 1, 1998 to ensure compatibility with Winamp 1.91: */
	"Merengue",
	"Salsa",
	"Thrash Metal",
	"Anime",
	"JPop",
	"Synthpop"
};

static const guint sync_mask = 0xE0FF;
static const guint mpeg_ver_mask = 0x1800;
static const guint mpeg_layer_mask = 0x600;
static const guint bitrate_mask = 0xF00000;
static const guint freq_mask = 0xC0000;
static const guint ch_mask = 0xC0000000;
static const guint pad_mask = 0x20000;

static const guint bitrate_table[16][6] = {
	{   0,   0,   0,   0,   0,   0 },
	{  32,  32,  32,  32,   8,   8 },
	{  64,  48,  40,  48,  16,  16 },
	{  96,  56,  48,  56,  24,  24 },
	{ 128,  64,  56,  64,  32,  32 },
	{ 160,  80,  64,  80,  40,  40 },
	{ 192,  96,  80,  96,  48,  48 },
	{ 224, 112,  96, 112,  56,  56 },
	{ 256, 128, 112, 128,  64,  64 },
	{ 288, 160, 128, 144,  80,  80 },
	{ 320, 192, 160, 160,  96,  96 },
	{ 352, 224, 192, 176, 112, 112 },
	{ 384, 256, 224, 192, 128, 128 },
	{ 416, 320, 256, 224, 144, 144 },
	{ 448, 384, 320, 256, 160, 160 },
	{  -1,  -1,  -1,  -1,  -1,  -1 }
};

static const gint freq_table[4][3] = {
	{ 44100, 22050, 11025 },
	{ 48000, 24000, 12000 },
	{ 32000, 16000, 8000  }
};

static const gint spf_table[6] = { 
	48, 144, 144, 48, 144, 72 
};

static TrackerExtractData extract_data[] = {
	{ "audio/mpeg", extract_mp3 },
	{ "audio/x-mp3", extract_mp3 },
	{ NULL, NULL }
};

static gchar *
improve_handwritten_genre (gchar *genre)
{
	/* This function tries to make each first letter of each word
	 * upper case so we conform a bit more to the standards, for
	 * example, if it is "Fusion jazz", we want "Fussion Jazz" to
	 * make things more consistent.
	 */

	gunichar *str;
	gchar    *ret;

        gunichar *p;
	gboolean set_next;

	if (!genre) {
		return NULL;
	}

	str = g_utf8_to_ucs4 (genre, -1, NULL, NULL, NULL);

	if (!str) {
		return NULL;
	}

        for (p = str, set_next = TRUE; *p; p++) {
		GUnicodeBreakType t;

		t = g_unichar_break_type (*p);

		if (set_next) {
			*p = g_unichar_toupper (*p);
			set_next = FALSE;
		}

		switch (t) {
		case G_UNICODE_BREAK_MANDATORY:
		case G_UNICODE_BREAK_CARRIAGE_RETURN:
		case G_UNICODE_BREAK_LINE_FEED:
		case G_UNICODE_BREAK_COMBINING_MARK:
		case G_UNICODE_BREAK_SURROGATE:
		case G_UNICODE_BREAK_ZERO_WIDTH_SPACE:
		case G_UNICODE_BREAK_INSEPARABLE:
		case G_UNICODE_BREAK_NON_BREAKING_GLUE:
		case G_UNICODE_BREAK_CONTINGENT:
		case G_UNICODE_BREAK_SPACE:
		case G_UNICODE_BREAK_HYPHEN:
		case G_UNICODE_BREAK_EXCLAMATION:
		case G_UNICODE_BREAK_WORD_JOINER:
		case G_UNICODE_BREAK_NEXT_LINE:
		case G_UNICODE_BREAK_SYMBOL:
			set_next = TRUE;

		case G_UNICODE_BREAK_AFTER:
		case G_UNICODE_BREAK_BEFORE:
		case G_UNICODE_BREAK_BEFORE_AND_AFTER:
		case G_UNICODE_BREAK_NON_STARTER:
		case G_UNICODE_BREAK_OPEN_PUNCTUATION:
		case G_UNICODE_BREAK_CLOSE_PUNCTUATION:
		case G_UNICODE_BREAK_QUOTATION:
		case G_UNICODE_BREAK_IDEOGRAPHIC:
		case G_UNICODE_BREAK_NUMERIC:
		case G_UNICODE_BREAK_INFIX_SEPARATOR:
		case G_UNICODE_BREAK_ALPHABETIC:
		case G_UNICODE_BREAK_PREFIX:
		case G_UNICODE_BREAK_POSTFIX:
		case G_UNICODE_BREAK_COMPLEX_CONTEXT:
		case G_UNICODE_BREAK_AMBIGUOUS:
		case G_UNICODE_BREAK_UNKNOWN:
		case G_UNICODE_BREAK_HANGUL_L_JAMO:
		case G_UNICODE_BREAK_HANGUL_V_JAMO:
		case G_UNICODE_BREAK_HANGUL_T_JAMO:
		case G_UNICODE_BREAK_HANGUL_LV_SYLLABLE:
		case G_UNICODE_BREAK_HANGUL_LVT_SYLLABLE:
			break;
		}
        }

	ret = g_ucs4_to_utf8 (str, -1, NULL, NULL, NULL);

	g_free (str);

	return ret;

}

static char *
read_id3v1_buffer (int fd, goffset size)
{
	gchar *buffer;
	guint bytes_read;
	guint rc;

	if (size < 128) {
		return NULL;
	}

	if (lseek (fd, size - ID3V1_SIZE, SEEK_SET) < 0) {
		return NULL;
	}

	buffer = g_malloc (ID3V1_SIZE);

	if (!buffer) {
		return NULL;
	}

	bytes_read = 0;
	
	while (bytes_read < ID3V1_SIZE) {
		rc = read(fd,
			  buffer + bytes_read,
			  ID3V1_SIZE - bytes_read);
		if (rc == -1) {
			if (errno != EINTR) {
				g_free (buffer);
				return NULL;
			}
		} else if (rc == 0) {
			break;
		} else {
			bytes_read += rc;
		}
	}
	
	return buffer;
}

/* Convert from UCS-2 to UTF-8 checking the BOM.*/
static gchar *
ucs2_to_utf8 (const gchar *data, 
	      guint        len) 
{
        gchar *encoding = NULL;
        guint16 c;
	gboolean be;
        gchar *utf8 = NULL;

        memcpy (&c, data, 2);

	switch (c) {
        case 0xfeff:
        case 0xfffe:
		be = G_BYTE_ORDER == G_BIG_ENDIAN;
		be = c == 0xfeff ? be : !be;
		encoding = be ? "UCS-2BE" : "UCS-2LE";
                data += 2;
                len -= 2;
                break;
        default:
                encoding = "UCS-2";
                break;
        }

        utf8 = g_convert (data, len, "UTF-8", encoding, NULL, NULL, NULL);

        return utf8;
}

/* Get the genre codes from regular expressions */
static gboolean
get_genre_number (const char *str, guint *genre)
{
	static GRegex *regex1 = NULL;
	static GRegex *regex2 = NULL;
	GMatchInfo *info = NULL;
	gchar *result = NULL;

	if (!regex1) {
		regex1 = g_regex_new ("\\(([0-9]+)\\)", 0, 0, NULL);
	}

	if (!regex2) {
		regex2 = g_regex_new ("([0-9]+)\\z", 0, 0, NULL);
	}

	if (g_regex_match (regex1, str, 0, &info)) {
		result = g_match_info_fetch (info, 1);
		if (result) {
			*genre = atoi (result);
			g_free (result);
			g_match_info_free (info);
			return TRUE;
		}
	}

	g_match_info_free (info);

	if (g_regex_match (regex2, str, 0, &info)) {
		result = g_match_info_fetch (info, 1);
		if (result) {
			*genre = atoi (result);
			g_free (result);
			g_match_info_free (info);
			return TRUE;
		}	
	}

	g_match_info_free (info);

	return FALSE;
}

static const gchar *
get_genre_name (guint number)
{
	if (number >= G_N_ELEMENTS (genre_names)) {
		return NULL;
	}

	return genre_names[number];
}

static void
un_unsync (const unsigned char *source,
	   size_t               size,
	   unsigned char      **destination,
	   size_t              *dest_size)
{
	gchar *dest;
	size_t offset  = 0;
	size_t new_size;

	*destination = g_malloc0 (size);
	dest         = *destination;
	new_size     = size;

	while (offset < size) {
		*dest = source[offset];

		if (source[offset] == 0xFF && 
		    source[offset + 1] == 0x00) {
			offset++;
			new_size--;
		}

		dest++;
		offset++;
	}
	
	*dest_size = new_size;
}

static char*
get_encoding (const char *data, 
	      gssize      size, 
	      gboolean   *encoding_found)
{
	gchar *encoding = NULL;

	if (encoding_found) {
		*encoding_found = FALSE;
	}
	
#ifdef HAVE_ENCA
	const char **langs;
	size_t s, i;

	langs = enca_get_languages (&s);

	for (i = 0; i < s && !encoding; i++) {
		EncaAnalyser analyser;
		EncaEncoding eencoding;

		analyser = enca_analyser_alloc (langs[i]);
		eencoding = enca_analyse_const (analyser, data, size);

		if (enca_charset_is_known (eencoding.charset)) {
			if (encoding_found) {
				*encoding_found = TRUE;
			}

			encoding = g_strdup (enca_charset_name (eencoding.charset, 
								ENCA_NAME_STYLE_ICONV));
		}

		enca_analyser_free (analyser);
	}

	free (langs);
#endif

	if (!encoding) {
		encoding = g_strdup ("ISO-8859-1");
	}

	return encoding;
}

static gchar*
t_convert (const gchar  *str,
           gssize        len,
           const gchar  *to_codeset,
           const gchar  *from_codeset,
           gsize        *bytes_read,
           gsize        *bytes_written,
           GError      **error_out)
{
	GError *error = NULL;
	gchar *word;

	/* g_print ("%s for %s\n", from_codeset, str); */

	word = g_convert (str,
			  len,
			  to_codeset,
			  from_codeset,
			  bytes_read, 
			  bytes_written, 
			  &error);

	if (error) {
		gchar *encoding;

		encoding = get_encoding (str, len, NULL);
		g_free (word);

		word = g_convert (str,
				  len,
				  to_codeset,
				  encoding,
				  bytes_read, 
				  bytes_written, 
				  error_out);

		g_free (encoding);
		g_error_free (error);
	}

	return word;
}

static gboolean
get_id3 (const gchar *data,
	 size_t       size,
	 tag_info    *id3)
{
#ifdef HAVE_ENCA
	GString *s;
	gboolean encoding_was_found;
#endif /* HAVE_ENCA */
	gchar *encoding;
	const gchar *pos;

	if (!data) {
		return FALSE;
	}
	
	if (size < 128) {
		return FALSE;
	}

	pos = &data[size - 128];

	if (strncmp ("TAG", pos, 3) != 0) {
		return FALSE;
	}

	/* Now convert all the data separately */
	pos += 3;

	/* We don't use our magic t_convert here because we have a better way
	 * to collect a bit more data before we let enca loose on it for v1. */

#ifdef HAVE_ENCA
	/* Get the encoding for ALL the data we are extracting here */
	s = g_string_new_len (pos, 30);
	g_string_append_len (s, pos + 30, 30);
	g_string_append_len (s, pos + 60, 30);

	encoding = get_encoding (s->str, 90, &encoding_was_found);

	if (encoding_was_found) {
		id3->encoding = encoding;
	}

	g_string_free (s, TRUE);
#else  /* HAVE_ENCA */
	encoding = get_encoding (NULL, 0, NULL);
#endif /* HAVE_ENCA */

	id3->title = g_convert (pos, 30, "UTF-8", encoding, NULL, NULL, NULL);

	pos += 30;
	id3->artist = g_convert (pos, 30, "UTF-8", encoding, NULL, NULL, NULL);

	pos += 30;
	id3->album = g_convert (pos, 30, "UTF-8", encoding, NULL, NULL, NULL);

	pos += 30;
	id3->year = g_convert (pos, 4, "UTF-8", encoding, NULL, NULL, NULL);

	pos += 4;

	if (pos[28] != 0) {
		id3->comment = g_convert (pos, 30, "UTF-8", encoding, NULL, NULL, NULL);
		id3->trackno = NULL;
	} else {
		gchar buf[5];

		id3->comment = g_convert (pos, 28, "UTF-8", encoding, NULL, NULL, NULL);

		snprintf (buf, 5, "%d", pos[29]);
		id3->trackno = g_strdup (buf);
	}

	pos += 30;
	id3->genre = g_strdup (get_genre_name ((guint) pos[0]));

	if (!id3->genre) {
		id3->genre = g_strdup ("");
	}

#ifndef HAVE_ENCA
	g_free (encoding);
#endif /* HAVE_ENCA */

	return TRUE;
}

static gboolean
mp3_parse_header (const gchar *data,
		  size_t       size,
		  size_t       seek_pos,
		  GHashTable  *metadata,
		  file_data   *filedata)
{
	guint header;
	gchar mpeg_ver = 0;
	gchar layer_ver = 0;
	gint spfp8 = 0;
	guint padsize = 0;
	gint idx_num = 0;
	guint bitrate = 0;
	guint avg_bps = 0;
	gint vbr_flag = 0;
	guint length = 0;
	guint sample_rate = 0;
	gint ch = 0;
	guint frame_size;
	guint frames = 0;
	size_t pos;

	pos = seek_pos;

	memcpy (&header, &data[pos], sizeof (header));

	switch (header & mpeg_ver_mask) {
	    case 0x800:
		    mpeg_ver = MPEG_ERR;
		    break;
	    case 0x1000:
#ifdef ENABLE_DETAILED_METADATA
		    g_hash_table_insert (metadata,
					 g_strdup ("Audio:Codec"),
					 g_strdup ("MPEG"));
		    g_hash_table_insert (metadata,
					 g_strdup ("Audio:CodecVersion"),
					 g_strdup ("2"));
#endif /* ENABLE_DETAILED_METADATA */
		    mpeg_ver = MPEG_V2;
		    break;
	    case 0x1800:
#ifdef ENABLE_DETAILED_METADATA
		    g_hash_table_insert (metadata,
					 g_strdup ("Audio:Codec"),
					 g_strdup ("MPEG"));
		    g_hash_table_insert (metadata,
					 g_strdup ("Audio:CodecVersion"),
					 g_strdup ("1"));
#endif /* ENABLE_DETAILED_METADATA */
		    mpeg_ver = MPEG_V1;
		    break;
	    case 0:
#ifdef ENABLE_DETAILED_METADATA
		    g_hash_table_insert (metadata,
					 g_strdup ("Audio:Codec"),
					 g_strdup ("MPEG"));
		    g_hash_table_insert (metadata,
					 g_strdup ("Audio:CodecVersion"),
					 g_strdup ("2.5"));
#endif /* ENABLE_DETAILED_METADATA */
		    mpeg_ver = MPEG_V25;
		    break;
	    default:
		    break;
	}

	switch (header & mpeg_layer_mask) {
	    case 0x400:
		    layer_ver = LAYER_2;
		    padsize = 1;
		    break;
	    case 0x200:
		    layer_ver = LAYER_3;
		    padsize = 1;
		    break;
	    case 0x600:
		    layer_ver = LAYER_1;
		    padsize = 4;
		    break;
	    case 0:
		    layer_ver = LAYER_ERR;
	    default:
		    break;
	}

	if (!layer_ver || !mpeg_ver) {
		/* g_debug ("Unknown mpeg type: %d, %d", mpeg_ver, layer_ver); */
		/* Unknown mpeg type */
		return FALSE;
	}
	
	if (mpeg_ver<3) {
		idx_num = (mpeg_ver - 1) * 3 + layer_ver - 1;
	} else {
		idx_num = 2 + layer_ver;
	}

	spfp8 = spf_table[idx_num];
	
	if ((header & ch_mask) == ch_mask) {
		ch = 1;
#ifdef ENABLE_DETAILED_METADATA
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Channels"),
				     g_strdup ("1"));
#endif /* ENABLE_DETAILED_METADATA */
	} else {
		ch = 2; /* stereo non stereo select */
#ifdef ENABLE_DETAILED_METADATA
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Channels"),
				     g_strdup ("2"));
#endif /* ENABLE_DETAILED_METADATA */
	}
	
	/* We assume mpeg version, layer and channels are constant in frames */
	do {
		frames++;
		bitrate = 1000 * bitrate_table[(header & bitrate_mask) >> 20][idx_num];

		if (bitrate <= 0) {
			frames--;
			return FALSE;
		}

		sample_rate = freq_table[(header & freq_mask) >> 18][mpeg_ver - 1];
		if (sample_rate < 0) {
			/* Error in header */
			frames--;
			return FALSE;
		}

		frame_size = spfp8 * bitrate / (sample_rate ? sample_rate : 1) + padsize*((header & pad_mask) >> 17);
		avg_bps += bitrate / 1000;

		pos += frame_size;

		if (frames > MAX_FRAMES_SCAN) {
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

		if (!vbr_flag && frames > VBR_THRESHOLD) {
			break;
		}

		memcpy(&header, &data[pos], sizeof (header));
	} while ((header & sync_mask) == sync_mask);

	if (frames < 2) { /* At least 2 frames to check the right position */
		/* No valid frames */
		return FALSE;
	}

	avg_bps /= frames;

	if (filedata->duration ==0 ) {
		if ((!vbr_flag && frames > VBR_THRESHOLD) || frames > MAX_FRAMES_SCAN) {
			/* If not all frames scanned */
			length = (filedata->size - filedata->id3v2_size) / (avg_bps ? avg_bps : bitrate ? bitrate : 0xFFFFFFFF) / 125;
		} else{
			length = spfp8 * 8 * frames / (sample_rate ? sample_rate : 0xFFFFFFFF);
		}
 
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Duration"),
				     tracker_escape_metadata_printf ("%d", length));
	}

#ifdef ENABLE_DETAILED_METADATA
	g_hash_table_insert (metadata,
			     g_strdup ("Audio:Samplerate"),
			     tracker_escape_metadata_printf ("%d", sample_rate));

	g_hash_table_insert (metadata,
			     g_strdup ("Audio:Bitrate"),
			     tracker_escape_metadata_printf ("%d", avg_bps*1000));
#endif /* ENABLE_DETAILED_METADATA */

	return TRUE;
}

static void
mp3_parse (const gchar *data,
	   size_t       size,
	   size_t       offset,
	   GHashTable  *metadata,
	   file_data   *filedata)
{
	guint header;
	guint counter = 0;
	guint pos = offset;

	do {
		/* Seek for frame start */
		if (pos + sizeof (header) > size) {
			return;
		}

		memcpy (&header, &data[pos], sizeof (header));

		if ((header & sync_mask) == sync_mask) {
			/* Found header sync */
			if (mp3_parse_header (data, size, pos, metadata, filedata)) {
				return;
			}
		}

		pos++;
		counter++;
	} while (counter < MAX_MP3_SCAN_DEEP);
}

static void
get_id3v24_tags (const gchar *data,
		 size_t       size,
		 tag_info    *info,
		 GHashTable  *metadata,
		 file_data   *filedata)
{
	guint pos = 0;
	tag_matches tmap[] = {
		{ "TCOP", "File:Copyright" },
		{ "TDRC", "Audio:ReleaseDate" },
		{ "TCON", "Audio:Genre" },
		{ "TIT1", "Audio:Genre" },
#ifdef ENABLE_DETAILED_METADATA
		{ "TENC", "DC:Publishers" },
#endif /* ENABLE_DETAILED_METADATA */
		{ "TEXT", "Audio:Lyrics" },
		{ "TPE1", "Audio:Artist" },
		/* { "TPE2", "Audio:Artist" }, Not needed */
		{ "TPE3", "Audio:Performer" },
		/*	{"TOPE", "Audio:Artist"}, We don't want the original artist for now */
#ifdef ENABLE_DETAILED_METADATA
		{ "TPUB", "DC:Publishers" },
#endif /* ENABLE_DETAILED_METADATA */
		{ "TOAL", "Audio:Album" },
		{ "TALB", "Audio:Album" },
		{ "TLAN", "File:Language" },
		{ "TIT2", "Audio:Title" },
#ifdef ENABLE_DETAILED_METADATA
		{ "TIT3", "Audio:Comment" },
#endif /* ENABLE_DETAILED_METADATA */
		{ "TDRL", "Audio:ReleaseDate" },
		{ "TRCK", "Audio:TrackNo" },
		{ "PCNT", "Audio:PlayCount" },
		{ "TLEN", "Audio:Duration" },
		{ NULL, 0 },
	};

	while (pos < size) {
		size_t csize;
		gint i;
		unsigned short flags;

		if (pos + 10 > size) {
			return;
		}

		csize = (((data[pos+4] & 0x7F) << 21) |
			 ((data[pos+5] & 0x7F) << 14) |
			 ((data[pos+6] & 0x7F) << 7) |
			 ((data[pos+7] & 0x7F) << 0));

		if (pos + 10 + csize > size ||
		    csize > size ||
		    csize == 0) {
			break;
		}

		flags = (((unsigned char) (data[pos + 8]) << 8) + 
			 ((unsigned char) (data[pos + 9])));
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
					word = t_convert (&data[pos + 11],
							  csize - 1,
							  "UTF-8",
							  info->encoding ? info->encoding : "ISO-8859-1",
							  NULL, NULL, NULL);
					break;
				case 0x01 :
					word = g_convert (&data[pos + 11],
							  csize - 1,
							  "UTF-8",
							  info->encoding ? info->encoding : "UTF-16",
							  NULL, NULL, NULL);
					break;
				case 0x02 :
					word = g_convert (&data[pos + 11],
							  csize - 1,
							  "UTF-8",
							  info->encoding ? info->encoding : "UTF-16BE",
							  NULL, NULL, NULL);
					break;
				case 0x03 :
					word = strndup (&data[pos + 11], csize - 1);
					break;

				default:
					/* Bad encoding byte,
					 * try to convert from
					 * iso-8859-1
					 */
					word = t_convert (&data[pos + 11],
							  csize - 1,
							  "UTF-8",
							  info->encoding ? info->encoding : "ISO-8859-1",
							  NULL, NULL, NULL);
					break;
				}

				pos++;
				csize--;

				if (!tracker_is_empty_string (word)) {       
					if (strcmp (tmap[i].text, "TRCK") == 0) {
						gchar **parts;

						parts = g_strsplit (word, "/", 2);
						g_free (word);

						word = g_strdup (parts[0]);
						g_strfreev (parts);
					} else if (strcmp (tmap[i].text, "TCON") == 0) {
						gint genre;
						gchar *improved_genre;

						if (get_genre_number (word, &genre)) {
							g_free (word);
							word = g_strdup (get_genre_name (genre));
						} else {
							if (g_ascii_strcasecmp (word, "unknown") == 0) {
								g_free (word);
								break;
							} else {
								if ((improved_genre = improve_handwritten_genre (word)) != NULL) {
									g_free (word);
									word = improved_genre;
								}
							}
						}
					} else if (strcmp (tmap[i].text, "TLEN") == 0) {
						guint32 duration;

						duration = atoi (word);
						g_free (word);

						word = g_strdup_printf ("%d", duration / 1000);
						filedata->duration = duration / 1000;
					}

					g_hash_table_insert (metadata,
							     g_strdup (tmap[i].type),
							     tracker_escape_metadata (word));
				}

				g_free (word);

				break;
			}

			i++;
		}

		if (strncmp (&data[pos], "COMM", 4) == 0) {
			gchar       *word;
			gchar        text_encode;
			const gchar *text_language;
			const gchar *text_desc;
			const gchar *text;
			guint        offset;
			gint         text_desc_len;

			/* $xx */
			text_encode   =  data[pos + 10];

			/* $xx xx xx */
			text_language = &data[pos + 11];

			/* <text string according to encoding> $00 (00) */
			text_desc     = &data[pos + 14];
			text_desc_len = strlen (text_desc);

			/* <full text string according to encoding> */
			text          = &data[pos + 14 + text_desc_len + 1]; 
			
			offset = 4 + text_desc_len + 1;

			switch (text_encode) {
			case 0x00:
				word = t_convert (text,
						  csize - offset,
						  "UTF-8",
						  info->encoding ? info->encoding : "ISO-8859-1",
						  NULL, NULL, NULL);
				break;
			case 0x01:
				word = g_convert (text,
						  csize - offset,
						  "UTF-8",
						  info->encoding ? info->encoding : "UTF-16",
						  NULL, NULL, NULL);
				break;
			case 0x02:
				word = g_convert (text,
						  csize-offset,
						  "UTF-8",
						  info->encoding ? info->encoding : "UTF-16BE",
						  NULL, NULL, NULL);
				break;
			case 0x03:
				word = g_strndup (text, csize - offset);
				break;
				
			default:
				/* Bad encoding byte,
				 * try to convert from
				 * iso-8859-1
				 */
				word = t_convert (text,
						  csize - offset,
						  "UTF-8",
						  info->encoding ? info->encoding : "ISO-8859-1",
						  NULL, NULL, NULL);
				break;
			}

#ifdef ENABLE_DETAILED_METADATA
			if (!tracker_is_empty_string (word)) {
				g_hash_table_insert (metadata,
						     g_strdup ("Audio:Comment"),
						     tracker_escape_metadata (word));
			}
#endif /* ENABLE_DETAILED_METADATA */

			g_free (word);
		}


		/* Check for embedded images */
		if (strncmp (&data[pos], "APIC", 4) == 0) {
			gchar        text_type;
			const gchar *mime;
			gchar        pic_type;
			const gchar *desc;
			guint        offset;
			gint         mime_len;

			text_type =  data[pos + 10];
			mime      = &data[pos + 11];
			mime_len  = strlen (mime);
			pic_type  =  data[pos + 11 + mime_len + 1];
			desc      = &data[pos + 11 + mime_len + 1 + 1];

			if (pic_type == 3 || (pic_type == 0 && filedata->albumartsize == 0)) {
				offset = pos + 11 + mime_len + 2 + strlen (desc) + 1;

				filedata->albumartdata = g_malloc0 (csize);
				filedata->albumartmime = g_strdup (mime);
				memcpy (filedata->albumartdata, &data[offset], csize);
				filedata->albumartsize = csize;
			}
		}

		pos += 10 + csize;
	}
}

static void
get_id3v23_tags (const gchar *data,
		 size_t       size,
		 tag_info    *info,
		 GHashTable  *metadata,
		 file_data   *filedata)
{
	guint pos = 0;
	tag_matches tmap[] = {
		{ "TCOP", "File:Copyright" },
		{ "TDAT", "Audio:ReleaseDate" },
		{ "TCON", "Audio:Genre" },
		{ "TIT1", "Audio:Genre" },
#ifdef ENABLE_DETAILED_METADATA
		{ "TENC", "DC:Publishers" },
#endif /* ENABLE_DETAILED_METADATA */
		{ "TEXT", "Audio:Lyrics" },
		{ "TPE1", "Audio:Artist" },
		/* { "TPE2", "Audio:Artist" }, */
		{ "TPE3", "Audio:Performer" },
		/*	{"TOPE", "Audio:Artist"}, We don't want the original artist for now */
#ifdef ENABLE_DETAILED_METADATA
		{ "TPUB", "DC:Publishers" },
#endif /* ENABLE_DETAILED_METADATA */
		{ "TOAL", "Audio:Album" },
		{ "TALB", "Audio:Album" },
		{ "TLAN", "File:Language" },
		{ "TIT2", "Audio:Title" },
		{ "TYER", "Audio:ReleaseDate" },
		{ "TRCK", "Audio:TrackNo" },
		{ "PCNT", "Audio:PlayCount" },
		{ "TLEN", "Audio:Duration" },
		{ NULL, 0 },
	};

	while (pos < size) {
		size_t csize;
		gint i;
		unsigned short flags;

		if (pos + 10 > size) {
			return;
		}

		csize = (((unsigned char)(data[pos + 4]) << 24) |
			 ((unsigned char)(data[pos + 5]) << 16) |
			 ((unsigned char)(data[pos + 6]) << 8)  |
			 ((unsigned char)(data[pos + 7]) << 0) );

		if ((pos + 10 + csize > size) ||
		    (csize > size) ||
		    (csize == 0)) {
			break;
		}

		flags = (((unsigned char)(data[pos + 8]) << 8) + 
			 ((unsigned char)(data[pos + 9])));

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

				/* This byte describes the encoding
				 * try to convert strings to UTF-8 if
				 * it fails, then forget it./
				 */

				switch (data[pos + 10]) {
				case 0x00:
					word = t_convert (&data[pos + 11],
							  csize - 1,
							  "UTF-8",
							  info->encoding ? info->encoding : "ISO-8859-1",
							  NULL, NULL, NULL);
					break;
				case 0x01 :
/* 					word = g_convert (&data[pos + 11], */
/* 							  csize - 1, */
/* 							  "UTF-8", */
/* 							  "UCS-2", */
/* 							  NULL, NULL, NULL); */
					word = ucs2_to_utf8 (&data[pos + 11],
							     csize - 1);
					break;
				default:
					/* Bad encoding byte,
					 * try to convert from
					 * iso-8859-1
					 */
					word = t_convert (&data[pos + 11],
							  csize - 1,
							  "UTF-8",
							  info->encoding ? info->encoding : "ISO-8859-1",
							  NULL, NULL, NULL);
					break;
				}

				pos++;
				csize--;

				if (!tracker_is_empty_string (word)) {
					if (strcmp (tmap[i].text, "TRCK") == 0) {
						gchar **parts;

						parts = g_strsplit (word, "/", 2);
						g_free (word);

						word = g_strdup (parts[0]);
						g_strfreev (parts);
					} else if (strcmp (tmap[i].text, "TCON") == 0) {
						gint genre;
						gchar *improved_genre;

						if (get_genre_number (word, &genre)) {
							g_free (word);
							word = g_strdup (get_genre_name (genre));
						} else {
							if (g_ascii_strcasecmp (word, "unknown") == 0) {
								g_free (word);
								break;
							} else {
								if ((improved_genre = improve_handwritten_genre (word)) != NULL) {
									g_free (word);
									word = improved_genre;
								}
							} 
						}
					} else if (strcmp (tmap[i].text, "TLEN") == 0) {
						guint32 duration;

						duration = atoi (word);
						g_free (word);
						word =  g_strdup_printf ("%d", duration/1000);
						filedata->duration = duration/1000;
					}

					g_hash_table_insert (metadata,
							     g_strdup (tmap[i].type),
							     tracker_escape_metadata (word));
				}

				g_free (word);

				break;
			}

			i++;
		}

		if (strncmp (&data[pos], "COMM", 4) == 0) {
			gchar       *word;
			gchar        text_encode;
			const gchar *text_language;
			const gchar *text_desc;
			const gchar *text;
			guint        offset;
			gint         text_desc_len;
			
			/* $xx */
			text_encode   =  data[pos + 10]; 

			/* $xx xx xx */
			text_language = &data[pos + 11]; 

			/* <text string according to encoding> $00 (00) */
			text_desc     = &data[pos + 14]; 
			text_desc_len = strlen (text_desc);

			/* <full text string according to encoding> */
			text          = &data[pos + 14 + text_desc_len + 1];
			
			offset = 4 + text_desc_len + 1;

			switch (text_encode) {
			case 0x00:
				word = t_convert (text,
						  csize - offset,
						  "UTF-8",
						  info->encoding ? info->encoding : "ISO-8859-1",
						  NULL, NULL, NULL);
				break;
			case 0x01 :
/* 				word = g_convert (text, */
/* 						  csize-offset, */
/* 						  "UTF-8", */
/* 						  "UCS-2", */
/* 						  NULL, NULL, NULL); */
				word = ucs2_to_utf8 (&data[pos + 11],
						     csize - offset);
				break;
			default:
				/* Bad encoding byte,
				 * try to convert from
				 * iso-8859-1
				 */
				word = t_convert (text,
						  csize - offset,
						  "UTF-8",
						  info->encoding ? info->encoding : "ISO-8859-1",
						  NULL, NULL, NULL);
				break;
			}

#ifdef ENABLE_DETAILED_METADATA
			if (!tracker_is_empty_string (word)) {
				g_hash_table_insert (metadata,
						     g_strdup ("Audio:Comment"),
						     tracker_escape_metadata (word));
			}
#endif /* ENABLE_DETAILED_METADATA */

			g_free (word);
		}

		/* Check for embedded images */
		if (strncmp (&data[pos], "APIC", 4) == 0) {
			gchar        text_type;
			const gchar *mime;
			gchar        pic_type;
			const gchar *desc;
			guint        offset;
			gint         mime_len;

			text_type =  data[pos + 10];
			mime      = &data[pos + 11];
			mime_len  = strlen (mime);
			pic_type  =  data[pos + 11 + mime_len + 1];
			desc      = &data[pos + 11 + mime_len + 1 + 1];
			
			if (pic_type == 3 || (pic_type == 0 && filedata->albumartsize == 0)) {
				offset = pos + 11 + mime_len + 2 + strlen (desc) + 1;
				
				filedata->albumartdata = g_malloc0 (csize);
				filedata->albumartmime = g_strdup (mime);
				memcpy (filedata->albumartdata, &data[offset], csize);
				filedata->albumartsize = csize;
			}
		}

		pos += 10 + csize;
	}
}

static void
get_id3v20_tags (const gchar *data,
		 size_t	      size,
		 tag_info    *info,
		 GHashTable  *metadata,
		 file_data   *filedata)
{
	guint pos = 0;
	tag_matches tmap[] = {
		{ "TAL", "Audio:Album" },
		{ "TT1", "Audio:Artist" },
		{ "TT2", "Audio:Title" },
		{ "TT3", "Audio:Title" },
#ifdef ENABLE_DETAILED_METADATA
		{ "TXT", "Audio:Comment" },
		{ "TPB", "DC:Publishers" },
#endif /* ENABLE_DETAILED_METADATA */
		{ "WAF", "DC:Location" },
		{ "WAR", "DC:Location" },
		{ "WAS", "DC:Location" },
		{ "WAF", "DC:Location" },
		{ "WCM", "File:License" },
		{ "TYE", "Audio:ReleaseDate" },
		{ "TLA", "File:Lanuguage" },
		{ "TP1", "Audio:Artist" },
		/* { "TP2", "Audio:Artist" }, */
		{ "TP3", "Audio:Performer" },
		/* { "TEN", "Audio:Performer" }, */
		{ "TCO", "Audio:Genre" },
		{ "TCR", "File:Copyright" },
		{ "SLT", "Audio:Lyrics" },
		{ "TOA", "Audio:Artist" },
		{ "TOT", "Audio:Album" },
		{ "TOL", "Audio:Artist" },
#ifdef ENABLE_DETAILED_METADATA
		{ "COM", "Audio:Comment" },
#endif /* ENABLE_DETAILED_METADATA */
		{ "TLE", "Audio:Duration" },
		{ "TRK", "Audio:TrackNo" },
		{ NULL, 0 },
	};

	while (pos < size) {
		size_t csize;
		gint i;

		if (pos + 6 > size)  {
			return;
		}

		csize = (((unsigned char)(data[pos + 3]) << 16) + 
			 ((unsigned char)(data[pos + 4]) << 8) + 
			 ((unsigned char)(data[pos + 5]) ) );
		if ((pos + 6 + csize > size) ||
		    (csize > size) ||
		    (csize == 0)) {
			break;
		}

		i = 0;

		while (tmap[i].text != NULL) {
			if (strncmp (tmap[i].text, (const char*) &data[pos], 3) == 0) {
				gchar *word;

				/* This byte describes the encoding
				 * try to convert strings to UTF-8 if
				 * it fails, then forget it./
				 */
				switch (data[pos + 6]) {
				case 0x00:
					word = t_convert (&data[pos + 7],
							  csize - 1,
							  "UTF-8",
							  info->encoding ? info->encoding : "ISO-8859-1",
							  NULL, NULL, NULL);
					break;
				case 0x01 :
/* 					word = g_convert (&data[pos+7], */
/* 							  csize, */
/* 							  "UTF-8", */
/* 							  "UCS-2", */
/* 							  NULL, NULL, NULL); */
					word = ucs2_to_utf8 (&data[pos + 7],
							     csize - 1);
					break;
				default:
					/* Bad encoding byte,
					 * try to convert from
					 * iso-8859-1
					 */
					word = t_convert (&data[pos + 7],
							  csize - 1,
							  "UTF-8",
							  info->encoding ? info->encoding : "ISO-8859-1",
							  NULL, NULL, NULL);
					break;
				}

				pos++;
				csize--;

				if (!tracker_is_empty_string (word)) {
					if (strcmp (tmap[i].text, "COM") == 0) {
						gchar *s;

						s = g_strdup (word + strlen (word) + 1);
						g_free (word);
						word = s;
					} else if (strcmp (tmap[i].text, "TCO") == 0) {
						gint genre;
						gchar *improved_genre;

						if (get_genre_number (word, &genre)) {
							g_free (word);
							word = g_strdup (get_genre_name (genre));
						} else {
							if (g_ascii_strcasecmp (word, "unknown") == 0) {
								g_free (word);
								break;
							} else {
								if ((improved_genre = improve_handwritten_genre (word)) != NULL) {
									g_free (word);
									word = improved_genre;
								}
							}
						}
					} else if (strcmp (tmap[i].text, "TLE") == 0) {
						guint32 duration;

						duration = atoi (word);
						g_free (word);
						word = g_strdup_printf ("%d", duration/1000);
						filedata->duration = duration/1000;
					} else if (strcmp (tmap[i].text, "TRK") == 0) {
						gchar **parts;

						parts = g_strsplit (word, "/", 2);
						g_free (word);

						word = g_strdup (parts[0]);
						g_strfreev (parts);
					}
					g_hash_table_insert (metadata,
							     g_strdup (tmap[i].type),
							     tracker_escape_metadata (word));
				}

				g_free (word);

				break;
			}

			i++;
		}

		/* Check for embedded images */
		if (strncmp (&data[pos], "PIC", 3) == 0) {
			gchar        pic_type;
			const gchar *desc;
			const gchar *mime;

			mime      = &data[pos + 6 + 3 + 1];
			pic_type  =  data[pos + 6 + 3 + 1 + 3];
			desc      = &data[pos + 6 + 3 + 1 + 3 + 1];

			if (pic_type == 3 || (pic_type == 0 && filedata->albumartsize == 0)) {
				guint offset;

				offset = pos + 6 + 3 + 1 + 3  + 1 + strlen (desc) + 1;

				filedata->albumartmime = g_strdup (mime);
				filedata->albumartdata = g_malloc0 (csize);
				memcpy (filedata->albumartdata, &data[offset], csize);
				filedata->albumartsize = csize;
			}
		}

		pos += 6 + csize;
	}
}

static void
parse_id3v24 (const gchar *data,
	      size_t       size,
	      tag_info    *info,
	      GHashTable  *metadata,
	      file_data   *filedata,
	      size_t      *offset_delta)
{
	gint unsync;
	gint extendedHdr;
	gint experimental;
	gint footer;
	guint tsize;
	guint pos;
	guint ehdrSize;
	guint padding;

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

	if (tsize + 10 > size || experimental) {
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

		if (tsize + 10 + ehdrSize > size) {
			return;
		}
	}

	if (unsync) {
		size_t unsync_size;
		gchar *body;

		un_unsync (&data[pos], tsize, (unsigned char **) &body, &unsync_size);
		get_id3v24_tags (body, unsync_size, info, metadata, filedata);
		g_free (body);
	} else {
		get_id3v24_tags (&data[pos], tsize, info, metadata, filedata);
	}

	*offset_delta = tsize + 10;
}

static void
parse_id3v23 (const gchar *data,
	      size_t       size,
	      tag_info    *info,
	      GHashTable  *metadata,
	      file_data   *filedata,
	      size_t      *offset_delta)
{
	gint unsync;
	gint extendedHdr;
	gint experimental;
	guint tsize;
	guint pos;
	guint ehdrSize;
	guint padding;

	if (size < 16 ||
	    data[0] != 0x49 ||
	    data[1] != 0x44 ||
	    data[2] != 0x33 ||
	    data[3] != 0x03 ||
	    data[4] != 0x00) {
		return;
	}

	unsync = (data[5] & 0x80) > 0;
	extendedHdr = (data[5] & 0x40) > 0;
	experimental = (data[5] & 0x20) > 0;
	tsize = (((data[6] & 0x7F) << 21) |
		 ((data[7] & 0x7F) << 14) |
		 ((data[8] & 0x7F) << 7) |
		 ((data[9] & 0x7F) << 0));

	if (tsize + 10 > size || experimental) {
		return;
	}

	pos = 10;
	padding = 0;

	if (extendedHdr) {
		ehdrSize = (((unsigned char)(data[10]) << 24) |
			    ((unsigned char)(data[11]) << 16) |
			    ((unsigned char)(data[12]) << 8) |
			    ((unsigned char)(data[12]) << 0));

		padding	= (((unsigned char)(data[15]) << 24) |
			   ((unsigned char)(data[16]) << 16) |
			   ((unsigned char)(data[17]) << 8) |
			   ((unsigned char)(data[18]) << 0));

		pos += 4 + ehdrSize;

		if (tsize + 10 + ehdrSize + padding > size) {
			return;
		}

		if (padding < tsize) {
			tsize -= padding;
		} else {
			return;
		}
	}

	if (unsync) {
		size_t unsync_size;
		gchar *body;

		un_unsync (&data[pos], tsize, (unsigned char **) &body, &unsync_size);
		get_id3v23_tags (body, unsync_size, info, metadata, filedata);
		g_free (body);
	} else {
		get_id3v23_tags (&data[pos], tsize, info, metadata, filedata);
	}

	*offset_delta = tsize + 10;
}

static void
parse_id3v20 (const gchar *data,
	      size_t	   size,
	      tag_info    *info,
	      GHashTable  *metadata,
	      file_data   *filedata,
	      size_t      *offset_delta)
{
	gint unsync;
	guint tsize;
	guint pos;

	if (size < 16 ||
	    data[0] != 0x49 ||
	    data[1] != 0x44 ||
	    data[2] != 0x33 ||
	    data[3] != 0x02 ||
	    data[4] != 0x00) {
		return;
	}

	unsync = (data[5] & 0x80) > 0;
	tsize = (((data[6] & 0x7F) << 21) |
		 ((data[7] & 0x7F) << 14) |
		 ((data[8] & 0x7F) << 07) |
		 ((data[9] & 0x7F) << 00));

	if (tsize + 10 > size) {
		return;
	}

	pos = 10;

	if (unsync) {
		size_t  unsync_size;
		gchar  *body;

		un_unsync (&data[pos], tsize, (unsigned char **) &body, &unsync_size);
		get_id3v20_tags (body, unsync_size, info, metadata, filedata);
		g_free (body);
	} else {
		get_id3v20_tags (&data[pos], tsize, info, metadata, filedata);
	}

	*offset_delta = tsize + 10;
}

static goffset
parse_id3v2 (const gchar *data,
	     size_t	  size,
	     tag_info    *info,
	     GHashTable  *metadata,
	     file_data   *filedata)
{
	gboolean done = FALSE;
	size_t offset = 0;

	do {
		size_t offset_delta = 0;

		parse_id3v24 (data + offset, 
			      size - offset, 
			      info, 
			      metadata, 
			      filedata, 
			      &offset_delta);
		parse_id3v23 (data + offset, 
			      size - offset, 
			      info, 
			      metadata, 
			      filedata, 
			      &offset_delta);
		parse_id3v20 (data + offset, 
			      size - offset, 
			      info, metadata, 
			      filedata, 
			      &offset_delta);

		if (offset_delta == 0) {
			done = TRUE;
			filedata->id3v2_size = offset;
		} else {
			offset += offset_delta;
		}

	} while (!done);

	return offset;
}

static void
extract_mp3 (const gchar *filename,
	     GHashTable  *metadata)
{
	int fd;
	void *buffer;
	void *id3v1_buffer;
	goffset size;
	goffset buffer_size;
	tag_info info;
	goffset audio_offset;
	file_data filedata;

	info.title = NULL;
	info.artist = NULL;
	info.album = NULL;
	info.year = NULL;
	info.comment = NULL;
	info.genre = NULL;
	info.trackno = NULL;
	info.encoding = NULL;

	filedata.size = 0;
	filedata.id3v2_size = 0;
	filedata.duration = 0;
	filedata.albumartdata = NULL;
	filedata.albumartmime = NULL;
	filedata.albumartsize = 0;

	size = tracker_file_get_size (filename);

	if (size == 0) {
		return;
	}

	filedata.size = size;
	buffer_size = MIN (size, MAX_FILE_READ);

#if defined(__linux__)
	/* Can return -1 because of O_NOATIME, so we try again after
	 * without as a last resort. This can happen due to
	 * permissions.
	 */
	fd = open (filename, O_RDONLY | O_NOATIME);
	if (fd == -1) {
		fd = open (filename, O_RDONLY);
		
		if (fd == -1) {
			return;
		}
	}
#else
	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		return;
	}
#endif

#ifndef G_OS_WIN32
	/* We don't use GLib's mmap because size can not be specified */
	buffer = mmap (NULL, 
		       buffer_size, 
		       PROT_READ, 
		       MAP_PRIVATE, 
		       fd, 
		       0);
#endif

	id3v1_buffer = read_id3v1_buffer (fd, size);

	close (fd);

	if (buffer == NULL || buffer == (void*) -1) {
		return;
	}

	if (!get_id3 (id3v1_buffer, ID3V1_SIZE, &info)) {
		/* Do nothing ? */
	}

	g_free (id3v1_buffer);

	if (!tracker_is_empty_string (info.title)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Title"),
				     tracker_escape_metadata (info.title));
	}

	if (!tracker_is_empty_string (info.artist)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Artist"),
				     tracker_escape_metadata (info.artist));
	}

	if (!tracker_is_empty_string (info.album)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Album"),
				     tracker_escape_metadata (info.album));
	}

	if (!tracker_is_empty_string (info.year)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:ReleaseDate"),
				     tracker_escape_metadata (info.year));
	}

	if (!tracker_is_empty_string (info.genre)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Genre"),
				     tracker_escape_metadata (info.genre));
	}

#ifdef ENABLE_DETAILED_METADATA
	if (!tracker_is_empty_string (info.comment)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:Comment"),
				     tracker_escape_metadata (info.comment));
	}
#endif /* ENABLE_DETAILED_METADATA */

	if (!tracker_is_empty_string (info.trackno)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Audio:TrackNo"),
				     tracker_escape_metadata (info.trackno));		
	}

	/* Get other embedded tags */
	audio_offset = parse_id3v2 (buffer, 
				    buffer_size, 
				    &info, 
				    metadata, 
				    &filedata);

	/* Get mp3 stream info */
	mp3_parse (buffer, 
		   buffer_size, 
		   audio_offset, 
		   metadata, 
		   &filedata);

	g_free (info.title);
	g_free (info.year);
	g_free (info.album);
	g_free (info.artist);
	g_free (info.comment);
	g_free (info.trackno);
	g_free (info.genre);
	g_free (info.encoding);

#ifdef HAVE_GDKPIXBUF
	tracker_process_albumart (filedata.albumartdata, filedata.albumartsize, filedata.albumartmime,
				  /* g_hash_table_lookup (metadata, "Audio:Artist") */ NULL,
				  g_hash_table_lookup (metadata, "Audio:Album"),
				  g_hash_table_lookup (metadata, "Audio:AlbumTrackCount"),
				  filename);
#else
	tracker_process_albumart (NULL, 0, NULL,
				  /* g_hash_table_lookup (metadata, "Audio:Artist") */ NULL,
				  g_hash_table_lookup (metadata, "Audio:Album"),
				  g_hash_table_lookup (metadata, "Audio:AlbumTrackCount"),
				  filename);

#endif /* HAVE_GDKPIXBUF */

	g_free (filedata.albumartdata);
	g_free (filedata.albumartmime);

	/* Check that we have the minimum data. FIXME We should not need to do this */
	tracker_utils_default_check_filename (metadata,
					      "Audio:Title",
					      filename);

#ifndef G_OS_WIN32
	munmap (buffer, buffer_size);
#endif
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return extract_data;
}
