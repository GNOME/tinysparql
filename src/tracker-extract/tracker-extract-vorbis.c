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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <glib.h>

#include <vorbis/vorbisfile.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"

#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define NMM_PREFIX TRACKER_NMM_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

static void extract_vorbis (const char *uri, 
                            TrackerSparqlBuilder  *metadata);

static struct {
	gchar *name;
	gchar *meta_name;
	gboolean writable;
	const gchar *urn;
	const gchar *rdf_type;
	const gchar *predicate;
} tags[] = {
	 {"title", NIE_PREFIX "title", FALSE, NULL, NULL, NULL},
	 {"artist", NMM_PREFIX "performer", FALSE, "artist", NCO_PREFIX "Contact", NCO_PREFIX "fullname" },
	 {"album", NIE_PREFIX "title", FALSE, NULL, NULL, NULL},
	 {"albumartist", NMM_PREFIX "performer", FALSE, "artist", NCO_PREFIX "Contact", NCO_PREFIX "fullname" },
	 {"trackcount", NMM_PREFIX "albumTrackCount", FALSE, NULL, NULL, NULL},
	 {"tracknumber", NMM_PREFIX "trackNumber", FALSE, NULL, NULL, NULL},
	 {"DiscNo", NMM_PREFIX "setNumber", FALSE, NULL, NULL, NULL},
	 {"Performer", NMM_PREFIX "performer", FALSE, "artist", NCO_PREFIX "Contact", NCO_PREFIX "fullname" },
	 {"TrackGain", "Audio.TrackGain", FALSE, NULL, NULL, NULL},
	 {"TrackPeakGain", "Audio.TrackPeakGain", FALSE, NULL, NULL, NULL},
	 {"AlbumGain", NMM_PREFIX "albumGain", FALSE, NULL, NULL, NULL},
	 {"AlbumPeakGain", NMM_PREFIX "albumPeakGain", FALSE, NULL, NULL, NULL},
	 {"date", NIE_PREFIX "contentCreated", FALSE, NULL, NULL, NULL},
	 {"comment", NIE_PREFIX "comment", FALSE, NULL, NULL, NULL},
	 {"genre", NFO_PREFIX "genre", FALSE, NULL, NULL, NULL},
	 {"Codec", NFO_PREFIX "codec", FALSE, NULL, NULL, NULL},
	 {"CodecVersion", "Audio.CodecVersion", FALSE, NULL, NULL, NULL},
	 {"Samplerate", NFO_PREFIX "sampleRate", FALSE, NULL, NULL, NULL},
	 {"Channels", NFO_PREFIX "channels", FALSE, NULL, NULL, NULL},
	 {"MBAlbumID", "Audio.MBAlbumID", FALSE, NULL, NULL, NULL},
	 {"MBArtistID", "Audio.MBArtistID", FALSE, NULL, NULL, NULL},
	 {"MBAlbumArtistID", "Audio.MBAlbumArtistID", FALSE, NULL, NULL, NULL},
	 {"MBTrackID", "Audio.MBTrackID", FALSE, NULL, NULL, NULL},
	 {"Lyrics", NIE_PREFIX "plainTextContent", FALSE, NULL, NULL, NULL},
	 {"Copyright", NIE_PREFIX "copyright", FALSE, NULL, NULL, NULL},
	 {"License", NIE_PREFIX "license", FALSE, NULL, NULL, NULL},
	 {"Organization", "File.Organization", FALSE, NULL, NULL, NULL},
	 {"Location", "File.Location", FALSE, NULL, NULL, NULL},
	 {"Publisher", DC_PREFIX "publisher", FALSE, "publisher", NCO_PREFIX "Contact", NCO_PREFIX "fullname" },
	 {NULL, NULL, FALSE, NULL, NULL, NULL},
};

static TrackerExtractData extract_data[] = {
	{ "audio/x-vorbis+ogg", extract_vorbis },
	{ "application/ogg", extract_vorbis },
	{ NULL, NULL }
};

static gchar *
ogg_get_comment (vorbis_comment *vc, 
                 gchar          *label)
{
	gchar *tag;
	gchar *utf_tag;

	if (vc && (tag = vorbis_comment_query (vc, label, 0)) != NULL) {
		utf_tag = g_locale_to_utf8 (tag, -1, NULL, NULL, NULL);
		/*g_free (tag);*/

		return utf_tag;
	} else {
		return NULL;
	}
}

#if 0

static gboolean
ogg_is_writable (const gchar *meta)
{
	gint i;

        for (i = 0; tags[i].name != NULL; i++) {
		if (g_strcmp0 (tags[i].meta_name, meta) == 0) {
			return tags[i].writable;
		}
	}

	return FALSE;
}

static gboolean
ogg_write (const char *meta_name,
           const char *value)
{
	/* to do */
	return FALSE;
}

#endif

static void
extract_vorbis (const char *uri,
                TrackerSparqlBuilder *metadata)
{
	FILE	       *f;
	OggVorbis_File  vf;
	gint	        i;
	vorbis_comment *comment;
	vorbis_info    *vi;
	unsigned int    bitrate;
	gint            time;
	gchar *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);
	f = tracker_file_open (filename, "r", FALSE);
	g_free (filename);

	if (!f) {
                return;
        }

	if (ov_open (f, &vf, NULL, 0) < 0) {
		tracker_file_close (f, FALSE);
		return;
	}

	if ((comment = ov_comment (&vf, -1)) != NULL) {
		tracker_statement_list_insert (metadata, uri, 
		                               RDF_TYPE, 
		                               NMM_PREFIX "MusicPiece");

		tracker_statement_list_insert (metadata, uri, 
		                               RDF_TYPE, 
		                               NFO_PREFIX "Audio");

                for (i = 0; tags[i].name != NULL; i++) {
                        gchar *str;
                        
                        str = ogg_get_comment (comment, tags[i].name);
                        
                        if (str) {
				if (tags[i].urn) {
					gchar *canonical_uri = tags[i].urn[0]!=':'?tracker_uri_printf_escaped ("urn:%s:%s", tags[i].urn, str):g_strdup(tags[i].urn);
					tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, tags[i].rdf_type);
					tracker_statement_list_insert (metadata, canonical_uri, tags[i].predicate, str);
					tracker_statement_list_insert (metadata, uri, tags[i].meta_name, canonical_uri);
					g_free (canonical_uri);
				} else {
					tracker_statement_list_insert (metadata, uri, tags[i].meta_name, str);
				}
				g_free (str);
                        }
                }
                
                vorbis_comment_clear (comment);
                
                if ((vi = ov_info (&vf, 0)) != NULL ) {
                        bitrate = vi->bitrate_nominal / 1000;

			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "averageBitrate", bitrate);
			tracker_statement_list_insert_with_int (metadata, uri, "Audio.CodecVersion", vi->version);
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "channels", vi->channels);
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "sampleRate", vi->rate);
                }
                
                /* Duration */
                if ((time = ov_time_total (&vf, -1)) != OV_EINVAL) {
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "duration", time);
                }
                
		tracker_statement_list_insert (metadata, uri, NFO_PREFIX "codec", "vorbis");
        }

        /* NOTE: This calls fclose on the file */
	ov_clear (&vf);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return extract_data;
}
