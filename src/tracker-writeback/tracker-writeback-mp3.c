/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <id3.h>

#include <libtracker-common/tracker-ontology.h>

#include "tracker-writeback-file.h"

#define TRACKER_TYPE_WRITEBACK_MP3 (tracker_writeback_mp3_get_type ())

typedef struct TrackerWritebackMP3 TrackerWritebackMP3;
typedef struct TrackerWritebackMP3Class TrackerWritebackMP3Class;

struct TrackerWritebackMP3 {
	TrackerWritebackFile parent_instance;
};

struct TrackerWritebackMP3Class {
	TrackerWritebackFileClass parent_class;
};

static GType                tracker_writeback_mp3_get_type             (void) G_GNUC_CONST;
static gboolean             writeback_mp3_update_file_metadata (TrackerWritebackFile *wbf,
                                                                GFile                *file,
                                                                GPtrArray            *values,
                                                                TrackerClient        *client);
static const gchar * const *writeback_mp3_content_types        (TrackerWritebackFile *wbf);

G_DEFINE_DYNAMIC_TYPE (TrackerWritebackMP3, tracker_writeback_mp3, TRACKER_TYPE_WRITEBACK_FILE);

static void
tracker_writeback_mp3_class_init (TrackerWritebackMP3Class *klass)
{
	TrackerWritebackFileClass *writeback_file_class = TRACKER_WRITEBACK_FILE_CLASS (klass);

	writeback_file_class->update_file_metadata = writeback_mp3_update_file_metadata;
	writeback_file_class->content_types = writeback_mp3_content_types;
}

static void
tracker_writeback_mp3_class_finalize (TrackerWritebackMP3Class *klass)
{
}

static void
tracker_writeback_mp3_init (TrackerWritebackMP3 *wbm)
{
}

static const gchar * const *
writeback_mp3_content_types (TrackerWritebackFile *wbf)
{
	static const gchar *content_types[] = {
		"audio/mpeg",
		"audio/x-mp3",
		NULL
	};

	return content_types;
}

static gboolean
writeback_mp3_update_file_metadata (TrackerWritebackFile *writeback_file,
                                    GFile                *file,
                                    GPtrArray            *values,
                                    TrackerClient        *client)
{
	gchar *path;
	guint n;

	path = g_file_get_path (file);

	for (n = 0; n < values->len; n++) {
		const GStrv row = g_ptr_array_index (values, n);

		if (g_strcmp0 (row[1], TRACKER_NIE_PREFIX "title") == 0) {
			ID3Tag *tag;
			ID3Frame *frame;

			tag = ID3Tag_New ();
			ID3Tag_Link (tag, path);

			frame = ID3Tag_FindFrameWithID (tag, ID3FID_TITLE);
			if (frame) {
				ID3Field *field;

				field = ID3Frame_GetField (frame, ID3FN_TEXT);
				ID3Field_SetASCII (field, row[3]);
			} else {
				ID3Field *field;

				frame = ID3Frame_NewID (ID3FID_TITLE);
				field = ID3Frame_GetField (frame, ID3FN_TEXT);
				ID3Field_SetASCII (field, row[3]);
				ID3Tag_AddFrame (tag, frame);
			}

			ID3Tag_Update (tag);
			/* Apparently this ain't needed (and crashes)
			 * ID3Frame_Delete (frame);
			 */
			ID3Tag_Delete (tag);
		}

		/* TODO: Add more */
	}

	g_free (path);

	return TRUE;
}

TrackerWriteback *
writeback_module_create (GTypeModule *module)
{
	tracker_writeback_mp3_register_type (module);

	return g_object_new (TRACKER_TYPE_WRITEBACK_MP3, NULL);
}

const gchar * const *
writeback_module_get_rdf_types (void)
{
	static const gchar *rdftypes[] = {
		TRACKER_NFO_PREFIX "Audio",
		NULL
	};

	return rdftypes;
}
