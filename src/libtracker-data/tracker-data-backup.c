/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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
#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <raptor.h>

#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-turtle.h>

#include "tracker-data-backup.h"

gboolean
tracker_data_backup_save (GFile   *turtle_file,
			  GError **error)
{
#if 0
	TrackerDBResultSet *data;
	TrackerClass *service;
	TurtleFile *turtle_file;

	/* TODO: temporary location */
	if (g_file_test (turtle_filename, G_FILE_TEST_EXISTS)) {
		g_unlink (turtle_filename);
	}

	turtle_file = tracker_turtle_open (turtle_filename);

	g_message ("Saving metadata backup in turtle file");

	/* TODO */

	service = tracker_ontology_get_service_by_name ("Files");
	data = tracker_data_query_backup_metadata (service);

	if (data) {
		extended_result_set_to_turtle (data, turtle_file);
		g_object_unref (data);
	}

	tracker_turtle_close (turtle_file);

#endif

	g_warning ("tracker_data_backup_save is unimplemented");

	return TRUE;
}

