/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2009, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-albumart.h"
#include "tracker-dbus.h"

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

#define MAX_FILE_READ     1024 * 1024 * 5
#define MAX_MP3_SCAN_DEEP 16768

#define MAX_FRAMES_SCAN   512
#define VBR_THRESHOLD     16

#define ID3V1_SIZE        128

typedef struct {
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *recording_time;
	gchar *comment;
	gchar *genre;
	gchar *encoding;
	gint   track_number;
} id3tag;

typedef struct {
	gchar *album;
	gchar *comment;
	gchar *content_type;
	gchar *copyright;
	gchar *encoded_by;
	guint32 length;
	gchar *performer1;
	gchar *performer2;
	gchar *composer;
	gchar *publisher;
	gchar *recording_time;
	gchar *release_time;
	gchar *text, *toly;
	gchar *title1;
	gchar *title2;
	gchar *title3;
	gint track_number;
	gint track_count;
	gint set_number;
	gint set_count;
} id3v2tag;

typedef enum {
	ID3V2_UNKNOWN,
	ID3V2_COM,
	ID3V2_PIC,
	ID3V2_TAL,
	ID3V2_TCO,
	ID3V2_TCR,
	ID3V2_TEN,
	ID3V2_TLE,
	ID3V2_TPB,
	ID3V2_TP1,
	ID3V2_TP2,
	ID3V2_TT1,
	ID3V2_TT2,
	ID3V2_TT3,
	ID3V2_TXT,
	ID3V2_TYE,
} id3v2frame;

typedef enum {
	ID3V24_UNKNOWN,
	ID3V24_APIC,
	ID3V24_COMM,
	ID3V24_TALB,
	ID3V24_TCOM,
	ID3V24_TCON,
	ID3V24_TCOP,
	ID3V24_TDRC,
	ID3V24_TDRL,
	ID3V24_TENC,
	ID3V24_TEXT,
	ID3V24_TIT1,
	ID3V24_TIT2,
	ID3V24_TIT3,
	ID3V24_TLEN,
	ID3V24_TOLY,
	ID3V24_TPE1,
	ID3V24_TPE2,
	ID3V24_TPUB,
	ID3V24_TRCK,
	ID3V24_TPOS,
	ID3V24_TYER,
} id3v24frame;

typedef struct {
	size_t size;
	size_t id3v2_size;

	guint32 duration;

	const gchar *title;
	const gchar *performer;
	gchar *performer_uri;
	const gchar *lyricist;
	gchar *lyricist_uri;
	const gchar *album;
	gchar *album_uri;
	const gchar *genre;
	const gchar *text;
	const gchar *recording_time;
	const gchar *encoded_by;
	const gchar *copyright;
	const gchar *publisher;
	const gchar *comment;
	const gchar *composer;
	gchar *composer_uri;
	gint track_number;
	gint track_count;
	gint set_number;
	gint set_count;

	unsigned char *albumart_data;
	size_t albumart_size;
	gchar *albumart_mime;

	id3tag id3v1;
	id3v2tag id3v22;
	id3v2tag id3v23;
	id3v2tag id3v24;
} MP3Data;

static void extract_mp3 (const gchar           *uri,
                         TrackerSparqlBuilder  *preupdate,
                         TrackerSparqlBuilder  *metadata);

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

/* sorted array */
static const struct {
	const char *name;
	id3v24frame frame;
} id3v24_frames[] = {
	{ "APIC", ID3V24_APIC },
	{ "COMM", ID3V24_COMM },
	{ "TALB", ID3V24_TALB },
	{ "TCOM", ID3V24_TCOM },
	{ "TCON", ID3V24_TCON },
	{ "TCOP", ID3V24_TCOP },
	{ "TDRC", ID3V24_TDRC },
	{ "TDRL", ID3V24_TDRL },
	{ "TENC", ID3V24_TENC },
	{ "TEXT", ID3V24_TEXT },
	{ "TIT1", ID3V24_TIT1 },
	{ "TIT2", ID3V24_TIT2 },
	{ "TIT3", ID3V24_TIT3 },
	{ "TLEN", ID3V24_TLEN },
	{ "TOLY", ID3V24_TOLY },
	{ "TPE1", ID3V24_TPE1 },
	{ "TPE2", ID3V24_TPE2 },
	{ "TPOS", ID3V24_TPOS },
	{ "TPUB", ID3V24_TPUB },
	{ "TRCK", ID3V24_TRCK },
	{ "TYER", ID3V24_TYER },
};

/* sorted array */
static const struct {
	const char *name;
	id3v2frame frame;
} id3v2_frames[] = {
	{ "COM", ID3V2_COM },
	{ "PIC", ID3V2_PIC },
	{ "TAL", ID3V2_TAL },
	{ "TCO", ID3V2_TCO },
	{ "TCR", ID3V2_TCR },
	{ "TEN", ID3V2_TEN },
	{ "TLE", ID3V2_TLE },
	{ "TP1", ID3V2_TP1 },
	{ "TP2", ID3V2_TP2 },
	{ "TPB", ID3V2_TPB },
	{ "TT1", ID3V2_TT1 },
	{ "TT2", ID3V2_TT2 },
	{ "TT3", ID3V2_TT3 },
	{ "TXT", ID3V2_TXT },
	{ "TYE", ID3V2_TYE },
};

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

static const guint sync_mask = 0xE0FF;
static const guint mpeg_ver_mask = 0x1800;
static const guint mpeg_layer_mask = 0x600;
static const guint bitrate_mask = 0xF00000;
static const guint freq_mask = 0xC0000;
static const guint ch_mask = 0xC0000000;
static const guint pad_mask = 0x20000;

static guint bitrate_table[16][6] = {
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

static gint freq_table[4][3] = {
	{ 44100, 22050, 11025 },
	{ 48000, 24000, 12000 },
	{ 32000, 16000, 8000  }
};

static gint spf_table[6] = {
	48, 144, 144, 48, 144,  72
};

static TrackerExtractData extract_data[] = {
	{ "audio/mpeg", extract_mp3 },
	{ "audio/x-mp3", extract_mp3 },
	{ NULL, NULL }
};



static void
id3tag_free (id3tag *tags)
{
	g_free (tags->title);
	g_free (tags->artist);
	g_free (tags->album);
	g_free (tags->recording_time);
	g_free (tags->comment);
	g_free (tags->genre);
	g_free (tags->encoding);
}

static void
id3v2tag_free (id3v2tag *tags)
{
	g_free (tags->album);
	g_free (tags->comment);
	g_free (tags->content_type);
	g_free (tags->copyright);
	g_free (tags->performer1);
	g_free (tags->performer2);
	g_free (tags->composer);
	g_free (tags->publisher);
	g_free (tags->recording_time);
	g_free (tags->release_time);
	g_free (tags->encoded_by);
	g_free (tags->text);
	g_free (tags->toly);
	g_free (tags->title1);
	g_free (tags->title2);
	g_free (tags->title3);
}

static char *
read_id3v1_buffer (int     fd,
                   goffset size)
{
	char *buffer;
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
		rc = read (fd,
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
ucs2_to_utf8(const gchar *data, guint len)
{
	const gchar   *encoding = NULL;
	guint16  c;
	gboolean be;
	gchar   *utf8 = NULL;

	memcpy (&c, data, 2);

	switch (c) {
	case 0xfeff:
	case 0xfffe:
		be = (G_BYTE_ORDER == G_BIG_ENDIAN);
		be = (c == 0xfeff) ? be : !be;
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
	size_t offset;
	gchar *dest;
	size_t new_size;

	offset       = 0;
	*destination = g_malloc0 (size);
	dest         = *destination;
	new_size     = size;

	while (offset < size) {
		*dest = source[offset];

		if ((source[offset] == 0xFF) &&
		    (source[offset + 1] == 0x00)) {
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
#ifdef HAVE_ENCA
	const char **langs;
	size_t s, i;
#endif

	if (encoding_found) {
		*encoding_found = FALSE;
	}

#ifdef HAVE_ENCA
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
		/* Use Windows-1252 instead of ISO-8859-1 as the former is a
		   superset in terms of printable characters and some
		   applications use it to encode characters in ID3 tags */
		encoding = g_strdup ("Windows-1252");
	}

	return encoding;
}

static gchar *
convert_to_encoding (const gchar  *str,
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
         id3tag      *id3)
{
#ifdef HAVE_ENCA
	GString *s;
	gboolean encoding_was_found;
#endif /* HAVE_ENCA */
	gchar *encoding, *year;
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

	/* We don't use our magic convert_to_encoding here because we
	 * have a better way to collect a bit more data before we let
	 * enca loose on it for v1.
	 */
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
	year = g_convert (pos, 4, "UTF-8", encoding, NULL, NULL, NULL);
	if (year && atoi (year) > 0) {
		id3->recording_time = tracker_date_guess (year);
	}
	g_free (year);

	pos += 4;

	if (pos[28] != 0) {
		id3->comment = g_convert (pos, 30, "UTF-8", encoding, NULL, NULL, NULL);
		id3->track_number = 0;
	} else {
		gchar buf[5];

		id3->comment = g_convert (pos, 28, "UTF-8", encoding, NULL, NULL, NULL);

		snprintf (buf, 5, "%d", pos[29]);
		id3->track_number = atoi (buf);
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
mp3_parse_header (const gchar          *data,
                  size_t                size,
                  size_t                seek_pos,
                  const gchar          *uri,
                  TrackerSparqlBuilder *metadata,
                  MP3Data              *filedata)
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
	guint frame_size;
	guint frames = 0;
	size_t pos = 0;

	pos = seek_pos;

	memcpy (&header, &data[pos], sizeof (header));

	switch (header & mpeg_ver_mask) {
	case 0x1000:
		mpeg_ver = MPEG_V2;
		break;
	case 0x1800:
		mpeg_ver = MPEG_V1;
		break;
	case 0:
		mpeg_ver = MPEG_V25;
		break;
	default:
		/* unknown version */
		return FALSE;
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
	default:
		/* unknown layer */
		return FALSE;
	}

	if (mpeg_ver < 3) {
		idx_num = (mpeg_ver - 1) * 3 + layer_ver - 1;
	} else {
		idx_num = 2 + layer_ver;
	}

	spfp8 = spf_table[idx_num];

	/* We assume mpeg version, layer and channels are constant in frames */
	do {
		frames++;
		bitrate = 1000 * bitrate_table[(header & bitrate_mask) >> 20][idx_num];

		if (bitrate <= 0) {
			frames--;
			return FALSE;
		}

		sample_rate = freq_table[(header & freq_mask) >> 18][mpeg_ver - 1];

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

		if ((!vbr_flag) && (frames > VBR_THRESHOLD)) {
			break;
		}

		memcpy(&header, &data[pos], sizeof (header));
	} while ((header & sync_mask) == sync_mask);

	/* At least 2 frames to check the right position */
	if (frames < 2) {
		/* No valid frames */
		return FALSE;
	}

	tracker_sparql_builder_predicate (metadata, "nfo:codec");
	tracker_sparql_builder_object_string (metadata, "MPEG");

	tracker_sparql_builder_predicate (metadata, "nfo:channels");
	if ((header & ch_mask) == ch_mask) {
		tracker_sparql_builder_object_int64 (metadata, 1);
	} else {
		tracker_sparql_builder_object_int64 (metadata, 2);
	}

	avg_bps /= frames;

	if (filedata->duration == 0) {
		if ((!vbr_flag && frames > VBR_THRESHOLD) || (frames > MAX_FRAMES_SCAN)) {
			/* If not all frames scanned
			 * Note that bitrate is always > 0, checked before */
			length = (filedata->size - filedata->id3v2_size) / (avg_bps ? avg_bps : bitrate) / 125;
		} else{
			length = spfp8 * 8 * frames / (sample_rate ? sample_rate : 0xFFFFFFFF);
		}

		tracker_sparql_builder_predicate (metadata, "nfo:duration");
		tracker_sparql_builder_object_int64 (metadata, length);
	}

	tracker_sparql_builder_predicate (metadata, "nfo:sampleRate");
	tracker_sparql_builder_object_int64 (metadata, sample_rate);
	tracker_sparql_builder_predicate (metadata, "nfo:averageBitrate");
	tracker_sparql_builder_object_int64 (metadata, avg_bps*1000);

	return TRUE;
}

static void
mp3_parse (const gchar          *data,
           size_t                size,
           size_t                offset,
           const gchar          *uri,
           TrackerSparqlBuilder *metadata,
           MP3Data              *filedata)
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
			if (mp3_parse_header (data, size, pos, uri, metadata, filedata)) {
				return;
			}
		}

		pos++;
		counter++;
	} while (counter < MAX_MP3_SCAN_DEEP);
}

static gssize
id3v2_nul_size (const gchar encoding)
{
	switch (encoding) {
	case 0x01:
	case 0x02:
		/* UTF-16, string terminated by two NUL bytes */
		return 2;
	default:
		return 1;
	}
}

static gssize
id3v2_strlen (const gchar  encoding,
              const gchar *text,
              gssize       len)
{
	const gchar *pos;

	switch (encoding) {
	case 0x01:
	case 0x02:
		/* UTF-16, string terminated by two NUL bytes */
		pos = memmem (text, len, "\0\0", 2);
		if (pos != NULL) {
			return pos - text;
		} else {
			return len;
		}
	default:
		return strnlen (text, len);
	}
}

static gchar *
id3v24_text_to_utf8 (const gchar  encoding,
                     const gchar *text,
                     gssize       len)
{
	/* This byte describes the encoding
	 * try to convert strings to UTF-8
	 * if it fails, then forget it.
	 * For UTF-16 if size odd assume invalid 00 term.
	 */

	switch (encoding) {
	case 0x00:
		/* Use Windows-1252 instead of ISO-8859-1 as the former is a
		   superset in terms of printable characters and some
		   applications use it to encode characters in ID3 tags */
		return convert_to_encoding (text,
		                            len,
		                            "UTF-8",
		                            "Windows-1252",
		                            NULL, NULL, NULL);
	case 0x01 :
		return convert_to_encoding (text,
		                            len - len%2,
		                            "UTF-8",
		                            "UTF-16",
		                            NULL, NULL, NULL);
	case 0x02 :
		return convert_to_encoding (text,
		                            len - len%2,
		                            "UTF-8",
		                            "UTF-16BE",
		                            NULL, NULL, NULL);
	case 0x03 :
		return strndup (text, len);

	default:
		/* Bad encoding byte,
		 * try to convert from
		 * Windows-1252
		 */
		return convert_to_encoding (text,
		                            len,
		                            "UTF-8",
		                            "Windows-1252",
		                            NULL, NULL, NULL);
	}
}

static gchar *
id3v2_text_to_utf8 (const gchar  encoding,
                    const gchar *text,
                    gssize       len)
{
	/* This byte describes the encoding
	 * try to convert strings to UTF-8
	 * if it fails, then forget it
	 * For UCS2 if size odd assume invalid 00 term.
	 */

	switch (encoding) {
	case 0x00:
		/* Use Windows-1252 instead of ISO-8859-1 as the former is a
		   superset in terms of printable characters and some
		   applications use it to encode characters in ID3 tags */
		return convert_to_encoding (text,
		                            len,
		                            "UTF-8",
		                            "Windows-1252",
		                            NULL, NULL, NULL);
	case 0x01 :
		/*              return g_convert (text, */
		/*                                len, */
		/*                                "UTF-8", */
		/*                                "UCS-2", */
		/*                                NULL, NULL, NULL); */
		return ucs2_to_utf8 (text, len - len%2);

	default:
		/* Bad encoding byte,
		 * try to convert from
		 * Windows-1252
		 */
		return convert_to_encoding (text,
		                            len,
		                            "UTF-8",
		                            "Windows-1252",
		                            NULL, NULL, NULL);
	}
}

static id3v24frame
id3v24_get_frame (const gchar *name)
{
	gint l, r, m;

	/* use binary search */

	l = 0;
	r = G_N_ELEMENTS (id3v24_frames) - 1;
	m = 0;

	do {
		m = (l + r) / 2;
		if (strncmp (name, id3v24_frames[m].name, 4) < 0) {
			// left half
			r = m - 1;
		} else {
			// right half
			l = m + 1;
		}
	} while (l <= r && strncmp (id3v24_frames[m].name, name, 4) != 0);

	if (strncmp (id3v24_frames[m].name, name, 4) == 0) {
		return id3v24_frames[m].frame;
	} else {
		return ID3V24_UNKNOWN;
	}
}

static id3v2frame
id3v2_get_frame (const gchar *name)
{
	gint l, r, m;

	/* use binary search */

	l = 0;
	r = G_N_ELEMENTS (id3v2_frames) - 1;
	m = 0;

	do {
		m = (l + r) / 2;
		if (strncmp (name, id3v2_frames[m].name, 4) < 0) {
			// left half
			r = m - 1;
		} else {
			// right half
			l = m + 1;
		}
	} while (l <= r && strncmp (id3v2_frames[m].name, name, 4) != 0);

	if (strncmp (id3v2_frames[m].name, name, 4) == 0) {
		return id3v2_frames[m].frame;
	} else {
		return ID3V2_UNKNOWN;
	}
}

static void
get_id3v24_tags (id3v24frame           frame,
                 const gchar          *data,
                 size_t                csize,
                 id3tag               *info,
                 const gchar          *uri,
                 TrackerSparqlBuilder *metadata,
                 MP3Data              *filedata)
{
	id3v2tag *tag = &filedata->id3v24;
	guint pos = 0;

	switch (frame) {
	case ID3V24_APIC: {
		/* embedded image */
		gchar text_type;
		const gchar *mime;
		gchar pic_type;
		const gchar *desc;
		guint offset;
		gint mime_len;

		text_type =  data[pos + 0];
		mime      = &data[pos + 1];
		mime_len  = strnlen (mime, csize - 1);
		pic_type  =  data[pos + 1 + mime_len + 1];
		desc      = &data[pos + 1 + mime_len + 1 + 1];

		if (pic_type == 3 || (pic_type == 0 && filedata->albumart_size == 0)) {
			offset = pos + 1 + mime_len + 2;
			offset += id3v2_strlen (text_type, desc, csize - offset) + id3v2_nul_size (text_type);

			filedata->albumart_data = g_malloc0 (csize - offset);
			filedata->albumart_mime = g_strndup (mime, mime_len);
			memcpy (filedata->albumart_data, &data[offset], csize - offset);
			filedata->albumart_size = csize - offset;
		}
		break;
	}

	case ID3V24_COMM: {
		gchar *word;
		gchar text_encode;
		const gchar *text_language;
		const gchar *text_desc;
		const gchar *text;
		guint offset;
		gint text_desc_len;

		text_encode   =  data[pos + 0]; /* $xx */
		text_language = &data[pos + 1]; /* $xx xx xx */
		text_desc     = &data[pos + 4]; /* <text string according to encoding> $00 (00) */
		text_desc_len = id3v2_strlen (text_encode, text_desc, csize - 4);

		offset        = 4 + text_desc_len + id3v2_nul_size (text_encode);
		text          = &data[pos + offset]; /* <full text string according to encoding> */

		word = id3v24_text_to_utf8 (text_encode, text, csize - offset);

		if (!tracker_is_empty_string (word)) {
			g_strstrip (word);
			g_free (tag->comment);
			tag->comment = word;
		} else {
			g_free (word);
		}
		break;
	}

	default: {
		gchar *word;

		/* text frames */
		word = id3v24_text_to_utf8 (data[pos], &data[pos + 1], csize - 1);
		if (!tracker_is_empty_string (word)) {
			g_strstrip (word);
		}

		g_debug ("Frame is %d, word is %s", frame, word);

		switch (frame) {
		case ID3V24_TALB:
			tag->album = word;
			break;
		case ID3V24_TCON: {
			gint genre;

			if (get_genre_number (word, &genre)) {
				g_free (word);
				word = g_strdup (get_genre_name (genre));
			}
			if (word && strcasecmp (word, "unknown") != 0) {
				tag->content_type = word;
			} else {
				g_free (word);
			}
			break;
		}
		case ID3V24_TCOP:
			tag->copyright = word;
			break;
		case ID3V24_TDRC:
			tag->recording_time = tracker_date_guess (word);
			g_free (word);
			break;
		case ID3V24_TDRL:
			tag->release_time = tracker_date_guess (word);
			g_free (word);
			break;
		case ID3V24_TENC:
			tag->encoded_by = word;
			break;
		case ID3V24_TEXT:
			tag->text = word;
			break;
		case ID3V24_TOLY:
			tag->toly = word;
			break;
		case ID3V24_TCOM:
			tag->composer = word;
			break;
		case ID3V24_TIT1:
			tag->title1 = word;
			break;
		case ID3V24_TIT2:
			tag->title2 = word;
			break;
		case ID3V24_TIT3:
			tag->title3 = word;
			break;
		case ID3V24_TLEN:
			tag->length = atoi (word) / 1000;
			g_free (word);
			break;
		case ID3V24_TPE1:
			tag->performer1 = word;
			break;
		case ID3V24_TPE2:
			tag->performer2 = word;
			break;
		case ID3V24_TPUB:
			tag->publisher = word;
			break;
		case ID3V24_TRCK: {
			gchar **parts;

			parts = g_strsplit (word, "/", 2);
			if (parts[0]) {
				tag->track_number = atoi (parts[0]);
				if (parts[1]) {
					tag->track_count = atoi (parts[1]);
				}
			}
			g_strfreev (parts);
			g_free (word);

			break;
		}
		case ID3V24_TPOS: {
			gchar **parts;

			parts = g_strsplit (word, "/", 2);
			if (parts[0]) {
				tag->set_number = atoi (parts[0]);
				if (parts[1]) {
					tag->set_count = atoi (parts[1]);
				}
			}
			g_strfreev (parts);
			g_free (word);

			break;
		}
		case ID3V24_TYER:
			if (atoi (word) > 0) {
				tag->recording_time = tracker_date_guess (word);
			}
			g_free (word);
			break;
		default:
			g_free (word);
		}
	}
	}
}

static void
get_id3v23_tags (id3v24frame           frame,
                 const gchar          *data,
                 size_t                csize,
                 id3tag               *info,
                 const gchar          *uri,
                 TrackerSparqlBuilder *metadata,
                 MP3Data              *filedata)
{
	id3v2tag *tag = &filedata->id3v23;
	guint pos = 0;

	switch (frame) {
	case ID3V24_APIC: {
		/* embedded image */
		gchar text_type;
		const gchar *mime;
		gchar pic_type;
		const gchar *desc;
		guint offset;
		gint  mime_len;

		text_type =  data[pos + 0];
		mime      = &data[pos + 1];
		mime_len  = strnlen (mime, csize - 1);
		pic_type  =  data[pos + 1 + mime_len + 1];
		desc      = &data[pos + 1 + mime_len + 1 + 1];

		if (pic_type == 3 || (pic_type == 0 && filedata->albumart_size == 0)) {
			offset = pos + 1 + mime_len + 2;
			offset += id3v2_strlen (text_type, desc, csize - offset) + id3v2_nul_size (text_type);

			filedata->albumart_data = g_malloc0 (csize - offset);
			filedata->albumart_mime = g_strndup (mime, mime_len);
			memcpy (filedata->albumart_data, &data[offset], csize - offset);
			filedata->albumart_size = csize - offset;
		}
		break;
	}

	case ID3V24_COMM: {
		gchar *word;
		gchar text_encode;
		const gchar *text_language;
		const gchar *text_desc;
		const gchar *text;
		guint offset;
		gint text_desc_len;

		text_encode   =  data[pos + 0]; /* $xx */
		text_language = &data[pos + 1]; /* $xx xx xx */
		text_desc     = &data[pos + 4]; /* <text string according to encoding> $00 (00) */
		text_desc_len = id3v2_strlen (text_encode, text_desc, csize - 4);

		offset        = 4 + text_desc_len + id3v2_nul_size (text_encode);
		text          = &data[pos + offset]; /* <full text string according to encoding> */

		word = id3v2_text_to_utf8 (text_encode, text, csize - offset);

		if (!tracker_is_empty_string (word)) {
			g_strstrip (word);
			g_free (tag->comment);
			tag->comment = word;
		} else {
			g_free (word);
		}

		break;
	}

	default: {
		gchar *word;

		/* text frames */
		word = id3v2_text_to_utf8 (data[pos], &data[pos + 1], csize - 1);

		if (!tracker_is_empty_string (word)) {
			g_strstrip (word);
		}

		switch (frame) {
		case ID3V24_TALB:
			tag->album = word;
			break;
		case ID3V24_TCON: {
			gint genre;

			if (get_genre_number (word, &genre)) {
				g_free (word);
				word = g_strdup (get_genre_name (genre));
			}
			if (word && strcasecmp (word, "unknown") != 0) {
				tag->content_type = word;
			} else {
				g_free (word);
			}
			break;
		}
		case ID3V24_TCOP:
			tag->copyright = word;
			break;
		case ID3V24_TENC:
			tag->encoded_by = word;
			break;
		case ID3V24_TEXT:
			tag->text = word;
			break;
		case ID3V24_TOLY:
			tag->toly = word;
			break;
		case ID3V24_TCOM:
			tag->composer = word;
			break;
		case ID3V24_TIT1:
			tag->title1 = word;
			break;
		case ID3V24_TIT2:
			tag->title2 = word;
			break;
		case ID3V24_TIT3:
			tag->title3 = word;
			break;
		case ID3V24_TLEN:
			tag->length = atoi (word) / 1000;
			g_free (word);
			break;
		case ID3V24_TPE1:
			tag->performer1 = word;
			break;
		case ID3V24_TPE2:
			tag->performer2 = word;
			break;
		case ID3V24_TPUB:
			tag->publisher = word;
			break;
		case ID3V24_TRCK: {
			gchar **parts;

			parts = g_strsplit (word, "/", 2);
			if (parts[0]) {
				tag->track_number = atoi (parts[0]);
				if (parts[1]) {
					tag->track_count = atoi (parts[1]);
				}
			}
			g_strfreev (parts);
			g_free (word);

			break;
		}
		case ID3V24_TPOS: {
			gchar **parts;

			parts = g_strsplit (word, "/", 2);
			if (parts[0]) {
				tag->set_number = atoi (parts[0]);
				if (parts[1]) {
					tag->set_count = atoi (parts[1]);
				}
			}
			g_strfreev (parts);
			g_free (word);

			break;
		}
		case ID3V24_TYER:
			if (atoi (word) > 0) {
				tag->recording_time = tracker_date_guess (word);
			}
			g_free (word);
			break;
		default:
			g_free (word);
		}
	}
	}
}

static void
get_id3v20_tags (id3v2frame            frame,
                 const gchar          *data,
                 size_t                csize,
                 id3tag               *info,
                 const gchar          *uri,
                 TrackerSparqlBuilder *metadata,
                 MP3Data              *filedata)
{
	id3v2tag *tag = &filedata->id3v22;
	guint pos = 0;

	if (frame == ID3V2_PIC) {
		/* embedded image */
		gchar          text_type;
		gchar          pic_type;
		const gchar   *desc;
		guint          offset;
		const gchar   *mime;

		text_type =  data[pos + 0];
		mime      = &data[pos + 1];
		pic_type  =  data[pos + 1 + 3];
		desc      = &data[pos + 1 + 3 + 1];

		if (pic_type == 3 || (pic_type == 0 && filedata->albumart_size == 0)) {
			offset = pos + 1 + 3 + 1;
			offset += id3v2_strlen (text_type, desc, csize - offset) + id3v2_nul_size (text_type);

			filedata->albumart_mime = g_strndup (mime, 3);
			filedata->albumart_data = g_malloc0 (csize - offset);
			memcpy (filedata->albumart_data, &data[offset], csize - offset);
			filedata->albumart_size = csize - offset;
		}
	} else {
		/* text frames */
		gchar *word;

		word = id3v2_text_to_utf8 (data[pos], &data[pos + 1], csize - 1);
		if (!tracker_is_empty_string (word)) {
			g_strstrip (word);
		}

		switch (frame) {
		case ID3V2_COM:
			tag->comment = word;
			break;
		case ID3V2_TAL:
			tag->album = word;
			break;
		case ID3V2_TCO: {
			gint genre;

			if (get_genre_number (word, &genre)) {
				g_free (word);
				word = g_strdup (get_genre_name (genre));
			}

			if (word && strcasecmp (word, "unknown") != 0) {
				tag->content_type = word;
			} else {
				g_free (word);
			}

			break;
		}
		case ID3V2_TCR:
			tag->copyright = word;
			break;
		case ID3V2_TEN:
			tag->encoded_by = word;
			break;
		case ID3V2_TLE:
			tag->length = atoi (word) / 1000;
			g_free (word);
			break;
		case ID3V2_TPB:
			tag->publisher = word;
			break;
		case ID3V2_TP1:
			tag->performer1 = word;
			break;
		case ID3V2_TP2:
			tag->performer2 = word;
			break;
		case ID3V2_TT1:
			tag->title1 = word;
			break;
		case ID3V2_TT2:
			tag->title2 = word;
			break;
		case ID3V2_TT3:
			tag->title3 = word;
			break;
		case ID3V2_TXT:
			tag->text = word;
			break;
		case ID3V2_TYE:
			if (atoi (word) > 0) {
				tag->recording_time = tracker_date_guess (word);
			}
			g_free (word);
			break;
		default:
			g_free (word);
		}
	}
}

static void
parse_id3v24 (const gchar           *data,
              size_t                 size,
              id3tag                *info,
              const gchar           *uri,
              TrackerSparqlBuilder  *metadata,
              MP3Data               *filedata,
              size_t                *offset_delta)
{
	gint unsync;
	gint ext_header;
	gint experimental;
	gint footer;
	guint tsize;
	guint pos;
	guint ext_header_size;
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
	ext_header = (data[5] & 0x40) > 0;
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

	if (ext_header) {
		ext_header_size = (((data[10] & 0x7F) << 21) |
		                   ((data[11] & 0x7F) << 14) |
		                   ((data[12] & 0x7F) << 7) |
		                   ((data[13] & 0x7F) << 0));
		pos += ext_header_size;

		if (pos + tsize > size) {
			/* invalid size: extended header longer than tag */
			return;
		}
	}

	while (pos < size) {
		id3v24frame frame;
		size_t csize;
		unsigned short flags;

		if (pos + 10 > size) {
			return;
		}

		frame = id3v24_get_frame (&data[pos]);

		csize = (((data[pos+4] & 0x7F) << 21) |
		         ((data[pos+5] & 0x7F) << 14) |
		         ((data[pos+6] & 0x7F) << 7) |
		         ((data[pos+7] & 0x7F) << 0));

		flags = (((unsigned char) (data[pos + 8]) << 8) +
		         ((unsigned char) (data[pos + 9])));

		pos += 10;

		if (frame == ID3V24_UNKNOWN) {
			/* ignore unknown frames */
			pos += csize;
			continue;
		}

		if (pos + csize > size) {
			break;
		} else if (csize == 0) {
			continue;
		}

		if (((flags & 0x80) > 0) ||
		    ((flags & 0x40) > 0)) {
			pos += csize;
			continue;
		}

		if ((flags & 0x20) > 0) {
			/* The "group" identifier, skip a byte */
			pos++;
			csize--;
		}

		if ((flags & 0x02) || unsync) {
			size_t unsync_size;
			gchar *body;

			un_unsync (&data[pos], csize, (unsigned char **) &body, &unsync_size);
			get_id3v24_tags (frame, body, unsync_size, info, uri, metadata, filedata);
			g_free (body);
		} else {
			get_id3v24_tags (frame, &data[pos], csize, info, uri, metadata, filedata);
		}

		pos += csize;
	}

	*offset_delta = tsize + 10;
}

static void
parse_id3v23 (const gchar          *data,
              size_t                size,
              id3tag               *info,
              const gchar          *uri,
              TrackerSparqlBuilder *metadata,
              MP3Data              *filedata,
              size_t               *offset_delta)
{
	gint unsync;
	gint ext_header;
	gint experimental;
	guint tsize;
	guint pos;
	guint ext_header_size;
	guint padding;

	if ((size < 16) ||
	    (data[0] != 0x49) ||
	    (data[1] != 0x44) ||
	    (data[2] != 0x33) ||
	    (data[3] != 0x03) ||
	    (data[4] != 0x00)) {
		return;
	}

	unsync = (data[5] & 0x80) > 0;
	ext_header = (data[5] & 0x40) > 0;
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

	if (ext_header) {
		ext_header_size = (((unsigned char)(data[10]) << 24) |
		                   ((unsigned char)(data[11]) << 16) |
		                   ((unsigned char)(data[12]) << 8) |
		                   ((unsigned char)(data[12]) << 0));

		padding         = (((unsigned char)(data[15]) << 24) |
		                   ((unsigned char)(data[16]) << 16) |
		                   ((unsigned char)(data[17]) << 8) |
		                   ((unsigned char)(data[18]) << 0));

		pos += 4 + ext_header_size;

		if (padding < tsize)
			tsize -= padding;
		else {
			return;
		}

		if (pos + tsize > size) {
			/* invalid size: extended header longer than tag */
			return;
		}
	}

	while (pos < size) {
		id3v24frame frame;
		size_t csize;
		unsigned short flags;

		if (pos + 10 > size) {
			return;
		}

		frame = id3v24_get_frame (&data[pos]);

		csize = (((unsigned char)(data[pos + 4]) << 24) |
		         ((unsigned char)(data[pos + 5]) << 16) |
		         ((unsigned char)(data[pos + 6]) << 8)  |
		         ((unsigned char)(data[pos + 7]) << 0) );

		flags = (((unsigned char)(data[pos + 8]) << 8) +
		         ((unsigned char)(data[pos + 9])));

		pos += 10;

		if (frame == ID3V24_UNKNOWN) {
			/* ignore unknown frames */
			pos += csize;
			continue;
		}

		if (pos + csize > size) {
			break;
		} else if (csize == 0) {
			continue;
		}

		if (((flags & 0x80) > 0) || ((flags & 0x40) > 0)) {
			pos += csize;
			continue;
		}

		if ((flags & 0x20) > 0) {
			/* The "group" identifier, skip a byte */
			pos++;
			csize--;
		}

		if ((flags & 0x02) || unsync) {
			size_t unsync_size;
			gchar *body;

			un_unsync (&data[pos], csize, (unsigned char **) &body, &unsync_size);
			get_id3v23_tags (frame, body, unsync_size, info, uri, metadata, filedata);
			g_free (body);
		} else {
			get_id3v23_tags (frame, &data[pos], csize, info, uri, metadata, filedata);
		}

		pos += csize;
	}

	*offset_delta = tsize + 10;
}

static void
parse_id3v20 (const gchar          *data,
              size_t                size,
              id3tag               *info,
              const gchar          *uri,
              TrackerSparqlBuilder *metadata,
              MP3Data              *filedata,
              size_t               *offset_delta)
{
	gint unsync;
	guint tsize;
	guint pos;

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

	if (tsize + 10 > size)  {
		return;
	}
	pos = 10;

	while (pos < size) {
		id3v2frame frame;
		size_t csize;

		if (pos + 6 > size)  {
			return;
		}

		frame = id3v2_get_frame (&data[pos]);

		csize = (((unsigned char)(data[pos + 3]) << 16) +
		         ((unsigned char)(data[pos + 4]) << 8) +
		         ((unsigned char)(data[pos + 5]) ) );

		pos += 6;

		if (frame == ID3V2_UNKNOWN) {
			/* ignore unknown frames */
			pos += csize;
			continue;
		}

		if (pos + csize > size) {
			break;
		} else if (csize == 0) {
			continue;
		}

		/* Early versions do not have unsynch per frame */
		if (unsync) {
			size_t  unsync_size;
			gchar  *body;

			un_unsync (&data[pos], csize, (unsigned char **) &body, &unsync_size);
			get_id3v20_tags (frame, body, unsync_size, info, uri, metadata, filedata);
			g_free (body);
		} else {
			get_id3v20_tags (frame, &data[pos], csize, info, uri, metadata, filedata);
		}

		pos += csize;
	}

	*offset_delta = tsize + 10;
}

static goffset
parse_id3v2 (const gchar          *data,
             size_t                size,
             id3tag               *info,
             const gchar          *uri,
             TrackerSparqlBuilder *metadata,
             MP3Data              *filedata)
{
	gboolean done = FALSE;
	size_t offset = 0;

	do {
		size_t offset_delta = 0;
		parse_id3v24 (data + offset, size - offset, info, uri, metadata, filedata, &offset_delta);
		parse_id3v23 (data + offset, size - offset, info, uri, metadata, filedata, &offset_delta);
		parse_id3v20 (data + offset, size - offset, info, uri, metadata, filedata, &offset_delta);

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
extract_mp3 (const gchar          *uri,
             TrackerSparqlBuilder *preupdate,
             TrackerSparqlBuilder *metadata)
{
	gchar *filename;
	int fd;
	void *buffer;
	void *id3v1_buffer;
	goffset size;
	goffset  buffer_size;
	goffset audio_offset;
	MP3Data md = { 0 };

	filename = g_filename_from_uri (uri, NULL, NULL);

	size = tracker_file_get_size (filename);

	if (size == 0) {
		g_free (filename);
		return;
	}

	md.size = size;
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

#ifdef HAVE_POSIX_FADVISE
	posix_fadvise (fd, 0, 0, POSIX_FADV_DONTNEED);
#endif /* HAVE_POSIX_FADVISE */

	close (fd);

	if (buffer == NULL || buffer == (void*) -1) {
		g_free (filename);
		return;
	}

	if (!get_id3 (id3v1_buffer, ID3V1_SIZE, &md.id3v1)) {
		/* Do nothing? */
	}

	g_free (id3v1_buffer);

	/* Get other embedded tags */
	audio_offset = parse_id3v2 (buffer, buffer_size, &md.id3v1, uri, metadata, &md);

	md.title = tracker_coalesce_strip (4, md.id3v24.title2,
	                                   md.id3v23.title2,
	                                   md.id3v22.title2,
	                                   md.id3v1.title);

	md.lyricist = tracker_coalesce_strip (4, md.id3v24.text,
	                                      md.id3v23.toly,
	                                      md.id3v23.text,
	                                      md.id3v22.text);

	md.composer = tracker_coalesce_strip (3, md.id3v24.composer,
	                                      md.id3v23.composer,
	                                      md.id3v22.composer);

	md.performer = tracker_coalesce_strip (7, md.id3v24.performer1,
	                                       md.id3v24.performer2,
	                                       md.id3v23.performer1,
	                                       md.id3v23.performer2,
	                                       md.id3v22.performer1,
	                                       md.id3v22.performer2,
	                                       md.id3v1.artist);

	md.album = tracker_coalesce_strip (4, md.id3v24.album,
	                                   md.id3v23.album,
	                                   md.id3v22.album,
	                                   md.id3v1.album);

	md.genre = tracker_coalesce_strip (7, md.id3v24.content_type,
	                                   md.id3v24.title1,
	                                   md.id3v23.content_type,
	                                   md.id3v23.title1,
	                                   md.id3v22.content_type,
	                                   md.id3v22.title1,
	                                   md.id3v1.genre);

	md.recording_time = tracker_coalesce_strip (7, md.id3v24.recording_time,
	                                            md.id3v24.release_time,
	                                            md.id3v23.recording_time,
	                                            md.id3v23.release_time,
	                                            md.id3v22.recording_time,
	                                            md.id3v22.release_time,
	                                            md.id3v1.recording_time);

	md.publisher = tracker_coalesce_strip (3, md.id3v24.publisher,
	                                       md.id3v23.publisher,
	                                       md.id3v22.publisher);

	md.copyright = tracker_coalesce_strip (3, md.id3v24.copyright,
	                                       md.id3v23.copyright,
	                                       md.id3v22.copyright);

	md.comment = tracker_coalesce_strip (7, md.id3v24.title3,
	                                     md.id3v24.comment,
	                                     md.id3v23.title3,
	                                     md.id3v23.comment,
	                                     md.id3v22.title3,
	                                     md.id3v22.comment,
	                                     md.id3v1.comment);

	md.encoded_by = tracker_coalesce_strip (3, md.id3v24.encoded_by,
						md.id3v23.encoded_by,
						md.id3v22.encoded_by);

	if (md.id3v24.track_number != 0) {
		md.track_number = md.id3v24.track_number;
	} else if (md.id3v23.track_number != 0) {
		md.track_number = md.id3v23.track_number;
	} else if (md.id3v22.track_number != 0) {
		md.track_number = md.id3v22.track_number;
	} else if (md.id3v1.track_number != 0) {
		md.track_number = md.id3v1.track_number;
	}

	if (md.id3v24.track_count != 0) {
		md.track_count = md.id3v24.track_count;
	} else if (md.id3v23.track_count != 0) {
		md.track_count = md.id3v23.track_count;
	} else if (md.id3v22.track_count != 0) {
		md.track_count = md.id3v22.track_count;
	}

	if (md.id3v24.set_number != 0) {
		md.set_number = md.id3v24.set_number;
	} else if (md.id3v23.set_number != 0) {
		md.set_number = md.id3v23.set_number;
	} else if (md.id3v22.set_number != 0) {
		md.set_number = md.id3v22.set_number;
	}

	if (md.id3v24.set_count != 0) {
		md.set_count = md.id3v24.set_count;
	} else if (md.id3v23.set_count != 0) {
		md.set_count = md.id3v23.set_count;
	} else if (md.id3v22.set_count != 0) {
		md.set_count = md.id3v22.set_count;
	}

	if (md.performer) {
		md.performer_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", md.performer);

		tracker_sparql_builder_insert_open (preupdate, NULL);

		tracker_sparql_builder_subject_iri (preupdate, md.performer_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");
		tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (preupdate, md.performer);

		tracker_sparql_builder_insert_close (preupdate);
	}

	if (md.composer) {
		md.composer_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", md.composer);

		tracker_sparql_builder_insert_open (preupdate, NULL);

		tracker_sparql_builder_subject_iri (preupdate, md.composer_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");
		tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (preupdate, md.composer);

		tracker_sparql_builder_insert_close (preupdate);
	}

	if (md.lyricist) {
		md.lyricist_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", md.lyricist);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, md.lyricist_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");
		tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (preupdate, md.lyricist);
		tracker_sparql_builder_insert_close (preupdate);
	}

	if (md.album) {
		md.album_uri = tracker_sparql_escape_uri_printf ("urn:album:%s", md.album);

		tracker_sparql_builder_insert_open (preupdate, NULL);

		tracker_sparql_builder_subject_iri (preupdate, md.album_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:MusicAlbum");
		/* FIXME: nmm:albumTitle is now deprecated
		 * tracker_sparql_builder_predicate (preupdate, "nie:title");
		 */
		tracker_sparql_builder_predicate (preupdate, "nmm:albumTitle");
		tracker_sparql_builder_object_unvalidated (preupdate, md.album);

		tracker_sparql_builder_insert_close (preupdate);

		if (md.track_count > 0) {
			tracker_sparql_builder_delete_open (preupdate, NULL);
			tracker_sparql_builder_subject_iri (preupdate, md.album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_delete_close (preupdate);
			tracker_sparql_builder_where_open (preupdate);
			tracker_sparql_builder_subject_iri (preupdate, md.album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_where_close (preupdate);

			tracker_sparql_builder_insert_open (preupdate, NULL);

			tracker_sparql_builder_subject_iri (preupdate, md.album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
			tracker_sparql_builder_object_int64 (preupdate, md.track_count);

			tracker_sparql_builder_insert_close (preupdate);
		}
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
	tracker_sparql_builder_object (metadata, "nfo:Audio");

	if (md.title) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, md.title);
	}


	if (md.lyricist_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:lyricist");
		tracker_sparql_builder_object_iri (metadata, md.lyricist_uri);
		g_free (md.lyricist_uri);
	}

	if (md.performer_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:performer");
		tracker_sparql_builder_object_iri (metadata, md.performer_uri);
		g_free (md.performer_uri);
	}

	if (md.composer_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:composer");
		tracker_sparql_builder_object_iri (metadata, md.composer_uri);
		g_free (md.composer_uri);
	}

	if (md.album_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:musicAlbum");
		tracker_sparql_builder_object_iri (metadata, md.album_uri);
	}

	if (md.recording_time) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, md.recording_time);
	}

	if (md.genre) {
		tracker_sparql_builder_predicate (metadata, "nfo:genre");
		tracker_sparql_builder_object_unvalidated (metadata, md.genre);
	}

	if (md.copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, md.copyright);
	}

	if (md.comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, md.comment);
	}

	if (md.publisher) {
		tracker_sparql_builder_predicate (metadata, "nco:publisher");
		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");
		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, md.publisher);
		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (md.encoded_by) {
		tracker_sparql_builder_predicate (metadata, "nfo:encodedBy");
		tracker_sparql_builder_object_unvalidated (metadata, md.encoded_by);
	}

	if (md.track_number > 0) {
		tracker_sparql_builder_predicate (metadata, "nmm:trackNumber");
		tracker_sparql_builder_object_int64 (metadata, md.track_number);
	}

	if (md.album) {
		gchar *album_disc_uri;

		album_disc_uri = tracker_sparql_escape_uri_printf ("urn:album-disc:%s:Disc%d",
		                                                   md.album,
		                                                   md.set_number > 0 ? md.set_number : 1);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:MusicAlbumDisc");
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_delete_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_delete_close (preupdate);
		tracker_sparql_builder_where_open (preupdate);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_where_close (preupdate);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
		tracker_sparql_builder_object_int64 (preupdate, md.set_number > 0 ? md.set_number : 1);
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_delete_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_delete_close (preupdate);
		tracker_sparql_builder_where_open (preupdate);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_where_close (preupdate);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
		tracker_sparql_builder_object_iri (preupdate, md.album_uri);
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nmm:musicAlbumDisc");
		tracker_sparql_builder_object_iri (metadata, album_disc_uri);

		g_free (album_disc_uri);
	}

	g_free (md.album_uri);

	/* FIXME We use a hardcoded value here for now. In reality there's a second option MP3X */
	tracker_sparql_builder_predicate (metadata, "nmm:dlnaProfile");
	tracker_sparql_builder_object_string (metadata, "MP3");

	/* Get mp3 stream info */
	mp3_parse (buffer, buffer_size, audio_offset, uri, metadata, &md);

	tracker_albumart_process (md.albumart_data,
	                          md.albumart_size,
	                          md.albumart_mime,
	                          md.performer,
	                          md.album,
	                          filename);
	g_free (md.albumart_data);
	g_free (md.albumart_mime);

	id3v2tag_free (&md.id3v22);
	id3v2tag_free (&md.id3v23);
	id3v2tag_free (&md.id3v24);
	id3tag_free (&md.id3v1);

#ifndef G_OS_WIN32
	munmap (buffer, buffer_size);
#endif

	g_free (filename);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}

