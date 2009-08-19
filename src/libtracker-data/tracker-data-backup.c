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

#include <libtracker-data/tracker-data-query.h>

#include "tracker-data-backup.h"

typedef struct {
	TrackerBackupFinished callback;
	gpointer user_data;
	GDestroyNotify destroy;
} UnImplementedInfo;

GQuark
tracker_data_backup_error_quark (void)
{
	return g_quark_from_static_string ("tracker-data-backup-error-quark");
}

static gboolean
unimplemented (gpointer user_data)
{
	UnImplementedInfo *info = user_data;

	g_warning ("tracker_data_backup_save is unimplemented");

	if (info->callback) {
		GError *error = NULL;

		g_set_error (&error,
		             TRACKER_DB_BACKUP_ERROR,
		             TRACKER_DB_BACKUP_ERROR_UNKNOWN,
		             "tracker_data_backup_save is unimplemented");

		info->callback (error, info->user_data);
		g_clear_error (&error);
	}

	if (info->destroy) {
		info->destroy (info->user_data);
	}

}

void
tracker_data_backup_save (GFile *turtle_file,
                          TrackerBackupFinished callback,
                          gpointer user_data,
                          GDestroyNotify destroy)
{
	UnImplementedInfo *info = g_new(UnImplementedInfo, 1);

	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
	                 callback,
	                 info,
	                 NULL);
}


