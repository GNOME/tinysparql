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
 */

#include <libtracker-common/tracker-file-utils.h>

#include "tracker-writeback-file.h"

static gboolean tracker_writeback_file_update_metadata (TrackerWriteback *writeback,
                                                        GPtrArray        *values);

G_DEFINE_ABSTRACT_TYPE (TrackerWritebackFile, tracker_writeback_file, TRACKER_TYPE_WRITEBACK)



static void
tracker_writeback_file_class_init (TrackerWritebackFileClass *klass)
{
	TrackerWritebackClass *writeback_class = TRACKER_WRITEBACK_CLASS (klass);

	writeback_class->update_metadata = tracker_writeback_file_update_metadata;
}

static void
tracker_writeback_file_init (TrackerWritebackFile *writeback_file)
{
}

static gboolean
tracker_writeback_file_update_metadata (TrackerWriteback *writeback,
                                        GPtrArray        *values)
{
	TrackerWritebackFileClass *writeback_file_class;
	gboolean retval;
	GFile *file;
	const gchar *subjects[2] = { NULL, NULL };
	GStrv row;
	TrackerWritebackFile *self;

	writeback_file_class = TRACKER_WRITEBACK_FILE_GET_CLASS (writeback);
	self = TRACKER_WRITEBACK_FILE (writeback);

	if (!writeback_file_class->update_file_metadata) {
		g_critical ("%s doesn't implement update_file_metadata()",
		            G_OBJECT_TYPE_NAME (writeback));
		return FALSE;
	}

	/* Get the file from the first row */
	row = g_ptr_array_index (values, 0);
	file = g_file_new_for_uri (row[0]);

	tracker_file_lock (file);

	subjects[0] = row[0];

	tracker_miner_manager_writeback (tracker_writeback_get_miner_manager (),
	                                 "org.freedesktop.Tracker1.Miner.Files",
	                                 subjects);

	retval = (writeback_file_class->update_file_metadata) (TRACKER_WRITEBACK_FILE (writeback),
	                                                       file, values);

	tracker_file_unlock (file);

	g_object_unref (file);

	return retval;
}
