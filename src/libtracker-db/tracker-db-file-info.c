/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-os-dependant.h>

#include "tracker-db-file-info.h"

static gint allocated;
static gint deallocated;

/*
 * TrackerDBWatch
 */
GType
tracker_db_watch_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_DB_WATCH_ROOT,
			  "TRACKER_DB_WATCH_ROOT",
			  "Watching Root" },
			{ TRACKER_DB_WATCH_SUBFOLDER,
			  "TRACKER_DB_WATCH_SUBFOLDER",
			  "Watching Subfolder" },
			{ TRACKER_DB_WATCH_SPECIAL_FOLDER,
			  "TRACKER_DB_WATCH_SPECIAL_FOLDER",
			  "Watching Special Folder" },
			{ TRACKER_DB_WATCH_SPECIAL_FILE,
			  "TRACKER_DB_WATCH_SPECIAL_FILE",
			  "Watching Special File" },
			{ TRACKER_DB_WATCH_NO_INDEX,
			  "TRACKER_DB_WATCH_NO_INDEX",
			  "Watching No Index" },
			{ TRACKER_DB_WATCH_OTHER,
			  "TRACKER_DB_WATCH_OTHER",
			  "Watching Other" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TrackerDBWatch", values);

		/* Since we don't reference this enum anywhere, we do
		 * it here to make sure it exists when we call
		 * g_type_class_peek(). This wouldn't be necessary if
		 * it was a param in a GObject for example.
		 *
		 * This does mean that we are leaking by 1 reference
		 * here and should clean it up, but it doesn't grow so
		 * this is acceptable.
		 */

		g_type_class_ref (etype);
	}

	return etype;
}

const gchar *
tracker_db_watch_to_string (TrackerDBWatch watch)
{
	GType	    type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	type = tracker_db_action_get_type ();
	enum_class = G_ENUM_CLASS (g_type_class_peek (type));
	enum_value = g_enum_get_value (enum_class, watch);

	if (!enum_value) {
		enum_value = g_enum_get_value (enum_class, TRACKER_DB_WATCH_OTHER);
	}

	return enum_value->value_nick;
}

/*
 * TrackerDBFileInfo
 */
TrackerDBFileInfo *
tracker_db_file_info_new (const char	  *uri,
			  TrackerDBAction  action,
			  gint		   counter,
			  TrackerDBWatch   watch)
{
	TrackerDBFileInfo *info;

	info = g_slice_new0 (TrackerDBFileInfo);

	info->action = action;
	info->uri = g_strdup (uri);

	info->counter = counter;
	info->file_id = 0;

	info->watch_type = watch;
	info->is_directory = FALSE;

	info->is_link = FALSE;
	info->link_id = 0;
	info->link_path = NULL;
	info->link_name = NULL;

	info->mime = NULL;
	info->file_size = 0;
	info->permissions = g_strdup ("-r--r--r--");
	info->mtime = 0;
	info->atime = 0;
	info->indextime = 0;
	info->offset = 0;
	info->aux_id = -1;

	info->is_hidden = FALSE;

	info->is_new = TRUE;
	info->service_type_id = -1;

	info->ref_count = 1;

	/* Keep a tally of how many we have created */
	allocated++;

	return info;
}

void
tracker_db_file_info_free (TrackerDBFileInfo *info)
{
	if (!info) {
		return;
	}

	if (info->uri) {
		g_free (info->uri);
	}

	if (info->moved_to_uri) {
		g_free (info->moved_to_uri);
	}

	if (info->link_path) {
		g_free (info->link_path);
	}

	if (info->link_name) {
		g_free (info->link_name);
	}

	if (info->mime) {
		g_free (info->mime);
	}

	if (info->permissions) {
		g_free (info->permissions);
	}

	g_slice_free (TrackerDBFileInfo, info);

	/* Keep a tally of how many we have removed */
	deallocated++;
}

/* Ref count TrackerDBFileInfo instances */
TrackerDBFileInfo *
tracker_db_file_info_ref (TrackerDBFileInfo *info)
{
	if (info) {
		g_atomic_int_inc (&info->ref_count);
	}

	return info;
}

TrackerDBFileInfo *
tracker_db_file_info_unref (TrackerDBFileInfo *info)
{
	if (!info) {
		return NULL;
	}

	if g_atomic_int_dec_and_test (&info->ref_count) {
		tracker_db_file_info_free (info);
		return NULL;
	}

	return info;
}

#if 0
static TrackerDBFileInfo *
db_file_info_get_pending (guint32	   file_id,
			  const gchar	  *uri,
			  const gchar	  *mime,
			  gint		   counter,
			  TrackerDBAction  action,
			  gboolean	   is_directory)
{
	TrackerDBFileInfo *info;

	info = g_slice_new0 (TrackerDBFileInfo);

	info->action = action;
	info->uri = g_strdup (uri);

	info->counter = counter;
	info->file_id = file_id;

	info->is_directory = is_directory;

	info->is_link = FALSE;
	info->link_id = 0;
	info->link_path = NULL;
	info->link_name = NULL;

	if (mime) {
		info->mime = g_strdup (mime);
	} else {
		info->mime = NULL;
	}

	info->file_size = 0;
	info->permissions = g_strdup ("-r--r--r--");
	info->mtime = 0;
	info->atime = 0;
	info->indextime = 0;
	info->offset = 0;

	info->service_type_id = -1;
	info->is_new = TRUE;

	info->ref_count = 1;

	allocated++;

	return info;
}
#endif

TrackerDBFileInfo *
tracker_db_file_info_get (TrackerDBFileInfo *info)
{
	struct stat  finfo;
	gchar	    *str, *uri_in_locale;

	if (!info || !info->uri) {
		return info;
	}

	uri_in_locale = g_filename_from_utf8 (info->uri, -1, NULL, NULL, NULL);

	if (uri_in_locale) {
		if (g_lstat (uri_in_locale, &finfo) == -1) {
			g_free (uri_in_locale);

			return info;
		}

	} else {
		g_warning ("URI:'%s' could not be converted to locale format",
			   info->uri);
		return NULL;
	}

	info->is_directory = S_ISDIR (finfo.st_mode);
	info->is_link = S_ISLNK (finfo.st_mode);

	if (info->is_link && !info->link_name) {
		str = g_file_read_link (uri_in_locale, NULL);

		if (str) {
			char *link_uri;

			link_uri = g_filename_to_utf8 (str, -1, NULL, NULL, NULL);
			info->link_name = g_path_get_basename (link_uri);
			info->link_path = g_path_get_dirname (link_uri);
			g_free (link_uri);
			g_free (str);
		}
	}

	g_free (uri_in_locale);

	if (!info->is_directory) {
		info->file_size = (guint32) finfo.st_size;
	} else {
		if (info->watch_type == TRACKER_DB_WATCH_OTHER) {
			info->watch_type = TRACKER_DB_WATCH_SUBFOLDER;
		}
	}

	g_free (info->permissions);
	info->permissions = tracker_create_permission_string (finfo);

	info->mtime =  finfo.st_mtime;
	info->atime =  finfo.st_atime;

	return info;
}

gboolean
tracker_db_file_info_is_valid (TrackerDBFileInfo *info)
{
	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (info->uri != NULL, FALSE);

	if (!g_utf8_validate (info->uri, -1, NULL)) {
		g_warning ("Expected UTF-8 validation of TrackerDBFileInfo URI");
		return FALSE;
	}

	if (info->action == TRACKER_DB_ACTION_IGNORE) {
		return FALSE;
	}

	return TRUE;
}

