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

#ifdef HAVE_RAPTOR
#include <raptor.h>
#endif

#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-turtle.h>

#include "tracker-data-backup.h"

#ifdef HAVE_RAPTOR

typedef struct BackupRestoreData BackupRestoreData;

struct BackupRestoreData {
	TrackerDataBackupRestoreFunc func;
	gpointer user_data;
};

/*
 * (uri, metadataid, value)
 */
static void
extended_result_set_to_turtle (TrackerDBResultSet  *result_set,
			       TurtleFile          *turtle_file)
{
	TrackerField *field;
	gboolean valid = TRUE;

	while (valid) {
		gchar *uri, *service_type, *str;
		gint metadata_id;

		tracker_db_result_set_get (result_set,
					   0, &uri,
					   1, &service_type,
					   2, &metadata_id,
					   3, &str,
					   -1);

		field = tracker_ontology_get_field_by_id (metadata_id);

		if (!field) {
			g_critical ("Field id %d in database but not in tracker-ontology",
				    metadata_id);
			g_free (str);
			g_free (service_type);
			g_free (uri);
			return;
		}

		g_debug ("Inserting in turtle <%s, %s, %s>",
			 uri, tracker_field_get_name (field), str);
		tracker_turtle_add_triple (turtle_file, uri, field, str);

		g_free (str);
		g_free (service_type);
		g_free (uri);

		valid = tracker_db_result_set_iter_next (result_set);
	}
}

static void
restore_backup_triple (gpointer                user_data,
		       const raptor_statement *triple)
{
	BackupRestoreData *data;

	data = (BackupRestoreData *) user_data;

	g_debug ("Turtle loading <%s, %s, %s>",
		 (gchar *)triple->subject,
		 (gchar *)triple->predicate,
		 (gchar *)triple->object);

	(data->func) ((const gchar *) triple->subject,
		      (const gchar *) triple->predicate,
		      (const gchar *) triple->object,
		      data->user_data);
}

#endif /* HAVE_RAPTOR */

gboolean
tracker_data_backup_save (const gchar  *turtle_filename,
			  GError      **error)
{
#ifdef HAVE_RAPTOR
	TrackerDBResultSet *data;
	TrackerService *service;
	TurtleFile *turtle_file;

	/* TODO: temporary location */
	if (g_file_test (turtle_filename, G_FILE_TEST_EXISTS)) {
		g_unlink (turtle_filename);
	}

	turtle_file = tracker_turtle_open (turtle_filename);

	g_message ("Saving metadata backup in turtle file");

	service = tracker_ontology_get_service_by_name ("Files");
	data = tracker_data_query_backup_metadata (service);

	if (data) {
		extended_result_set_to_turtle (data, turtle_file);
		g_object_unref (data);
	}

	tracker_turtle_close (turtle_file);

	return TRUE;
#else
	g_set_error (error, 0, 0,
		     "Turtle files are not supported, could not save backup");

	return FALSE;
#endif /* HAVE_RAPTOR */
}

gboolean
tracker_data_backup_restore (const gchar                   *turtle_filename,
			     TrackerDataBackupRestoreFunc   restore_func,
			     gpointer                       user_data,
			     GError                       **error)
{
#ifdef HAVE_RAPTOR
	BackupRestoreData data;

	data.func = restore_func;
	data.user_data = user_data;

	g_message ("Restoring metadata backup from turtle file");

	if (!g_file_test (turtle_filename, G_FILE_TEST_EXISTS)) {
		g_set_error (error, 0, 0,
			     "Turtle file does not exist");
		return FALSE;
	}

	tracker_turtle_process (turtle_filename,
				"/",
				(TurtleTripleCallback) restore_backup_triple,
				&data);
	return TRUE;
#else
	g_set_error (error, 0, 0,
		     "Turtle files are not supported, could not restore backup");

	return FALSE;
#endif /* HAVE_RAPTOR */
}
