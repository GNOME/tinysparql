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

#ifdef HAVE_VORBIS

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <vorbis/vorbisfile.h>

#include "tracker-extract.h"

/*#include "tracker-utils.h"*/

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


static char*
get_comment (vorbis_comment *vc, char *label)
{
	char *tag;
	char *utf_tag;

	if (vc && (tag = vorbis_comment_query (vc, label, 0)) != NULL) {

		utf_tag = g_locale_to_utf8 (tag, -1, NULL, NULL, NULL);

		/*g_free (tag);*/

		return utf_tag;

	} else {
		return NULL;
	}

}


gboolean
tracker_metadata_ogg_is_writable (const char *meta)
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
tracker_metadata_ogg_write (const char *meta_name, const char *value)
{
	/* to do */
	return FALSE;
}


void
tracker_extract_vorbis (const char *filename, GHashTable *metadata)
{
	gint	       fd;
	FILE	       *oggFile;
	OggVorbis_File vf;
	gint	       i;

#if defined(__linux__)
	if ((fd = g_open (filename, (O_RDONLY | O_NOATIME))) == -1) {
#else
	if ((fd = g_open (filename, O_RDONLY)) == -1) {
#endif
		return;
	}

	oggFile = fdopen (fd, "r");

	if (!oggFile) {
		close (fd);
		return;
	}

	if ( ov_open (oggFile, &vf, NULL, 0) < 0 ) {
		fclose (oggFile);
		return;
	}

	char *tmpComment;

	vorbis_comment *comment;

	if ((comment  = ov_comment (&vf, -1)) == NULL) {
		ov_clear (&vf);
		return;
	}

	i = 0;
	while (tags[i].name != NULL) {
		tmpComment = get_comment (comment, tags[i].name);

		if (tmpComment) {
			g_hash_table_insert (metadata, g_strdup (tags[i].meta_name), tmpComment);
		}

		i++;
	}

	vorbis_comment_clear(comment);

	/* Bitrate */

	vorbis_info *vi;
	unsigned int bitrate;
	char *str_bitrate;

	if ( ( vi = ov_info(&vf, 0)) != NULL ) {
		bitrate = vi->bitrate_nominal/1000;
		str_bitrate = g_strdup_printf ("%d", bitrate);
		g_hash_table_insert (metadata, g_strdup ("Audio.Bitrate"), str_bitrate);
		g_hash_table_insert (metadata, g_strdup ("Audio.CodecVersion"), g_strdup_printf ("%d", vi->version));
		g_hash_table_insert (metadata, g_strdup ("Audio.Channels"), g_strdup_printf ("%d", vi->channels));
		g_hash_table_insert (metadata, g_strdup ("Audio.Samplerate"), g_strdup_printf ("%ld", vi->rate));
	}



	/* Duration */

	int time;
	char *str_time;
	if ( ( time = ov_time_total(&vf, -1) ) != OV_EINVAL ) {
		str_time = g_strdup_printf ("%d", time);
		g_hash_table_insert (metadata, g_strdup ("Audio.Duration"), str_time);
	}

	g_hash_table_insert (metadata, g_strdup ("Audio.Codec"), g_strdup ("vorbis"));

	ov_clear(&vf);

}

#else
#warning "Not building ogg/vorbis metadata extractor"
#endif	/* HAVE_VORBIS */
