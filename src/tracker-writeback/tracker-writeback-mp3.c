/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors: Philip Van Hoof <philip@codeminded.be>
 */

#include <id3.h>

#include <libtracker-common/tracker-ontology.h>

#include "tracker-writeback.h"

#define TRACKER_TYPE_WRITEBACK_MP3    (tracker_writeback_mp3_get_type ())

typedef struct TrackerWritebackMP3 TrackerWritebackMP3;
typedef struct TrackerWritebackMP3Class TrackerWritebackMP3Class;

struct TrackerWritebackMP3 {
        TrackerWriteback parent_instance;
};

struct TrackerWritebackMP3Class {
        TrackerWritebackClass parent_class;
};

static GType    tracker_writeback_mp3_get_type        (void) G_GNUC_CONST;
static gboolean tracker_writeback_mp3_update_metadata (TrackerWriteback *writeback,
                                                         GPtrArray        *values);

G_DEFINE_DYNAMIC_TYPE (TrackerWritebackMP3, tracker_writeback_mp3, TRACKER_TYPE_WRITEBACK);

static void
tracker_writeback_mp3_class_init (TrackerWritebackMP3Class *klass)
{
        TrackerWritebackClass *writeback_class = TRACKER_WRITEBACK_CLASS (klass);

        writeback_class->update_metadata = tracker_writeback_mp3_update_metadata;
}

static void
tracker_writeback_mp3_class_finalize (TrackerWritebackMP3Class *klass)
{
}

static void
tracker_writeback_mp3_init (TrackerWritebackMP3 *mp3)
{
}

static gboolean
tracker_writeback_mp3_update_metadata (TrackerWriteback *writeback,
                                       GPtrArray        *values)
{
	guint n;
	const gchar *uri = NULL;
	GFile *file = NULL;
	gchar *path = NULL;

	for (n = 0; n < values->len; n++) {
		const GStrv row = g_ptr_array_index (values, n);

		if (uri == NULL) {
			GFileInfo *file_info;
			const gchar *mime_type;

			uri = row[0];
			file = g_file_new_for_uri (uri);

			file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
			                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
			                               NULL, NULL);

			if (!file_info) {
				g_object_unref (file);
				return FALSE;
			}

			mime_type = g_file_info_get_content_type (file_info);

			if (g_strcmp0 (mime_type, "audio/mpeg") == 0 ||
			    g_strcmp0 (mime_type, "audio/x-mp3") == 0) {
				g_object_unref (file_info);
				path = g_file_get_path (file);
			} else {
				g_object_unref (file);
				g_object_unref (file_info);
				return FALSE;
			}
		}

		if (g_strcmp0 (row[1], TRACKER_NIE_PREFIX "title") == 0) {
			ID3Tag *tag = ID3Tag_New ();
			ID3Frame *frame;

			ID3Tag_Link (tag, path);

 			frame = ID3Tag_FindFrameWithID (tag, ID3FID_TITLE);
			if (frame) {
				ID3Field *field;
				field = ID3Frame_GetField (frame, ID3FN_TEXT);
				ID3Field_SetASCII (field, row[2]);
			} else {
				ID3Field *field;
				frame = ID3Frame_NewID (ID3FID_TITLE);
				field = ID3Frame_GetField (frame, ID3FN_TEXT);
				ID3Field_SetASCII (field, row[2]);
				ID3Tag_AddFrame (tag, frame);
			}

			ID3Tag_Update (tag);
			// ID3Frame_Delete (frame);
			ID3Tag_Delete (tag);
		}
	}

	if (path)
		g_free (path);

	if (file)
		g_object_unref (file);

	return TRUE;
}

TrackerWriteback *
writeback_module_create (GTypeModule *module)
{
	tracker_writeback_mp3_register_type (module);

	return g_object_new (TRACKER_TYPE_WRITEBACK_MP3, NULL);
}

const gchar**
writeback_module_get_rdftypes (void)
{
	static const gchar *rdftypes[] = { TRACKER_NFO_PREFIX "Document",
	                                   TRACKER_NMM_PREFIX "MusicPiece",
	                                   NULL };

	return rdftypes;
}
