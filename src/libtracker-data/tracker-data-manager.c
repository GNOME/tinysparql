/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
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

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <zlib.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-nfs-lock.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-data-manager.h"

#define ZLIBBUFSIZ 8192

typedef struct {
	TrackerConfig	*config;
	TrackerLanguage *language;
} TrackerDBPrivate;

/* Private */
static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerDBPrivate *private;

	private = data;

	if (private->config) {
		g_object_unref (private->config);
	}

	if (private->language) {
		g_object_unref (private->language);
	}

	g_free (private);
}


void
tracker_data_manager_init (TrackerConfig   *config,
			   TrackerLanguage *language,
			   TrackerDBIndex  *file_index,
			   TrackerDBIndex  *email_index)
{
	TrackerDBPrivate *private;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (TRACKER_IS_LANGUAGE (language));
	g_return_if_fail (TRACKER_IS_DB_INDEX (file_index));
	g_return_if_fail (TRACKER_IS_DB_INDEX (email_index));

	private = g_static_private_get (&private_key);
	if (private) {
		g_warning ("Already initialized (%s)",
			   __FUNCTION__);
		return;
	}

	private = g_new0 (TrackerDBPrivate, 1);

	private->config = g_object_ref (config);
	private->language = g_object_ref (language);

	g_static_private_set (&private_key,
			      private,
			      private_free);
}

void
tracker_data_manager_shutdown (void)
{
	TrackerDBPrivate *private;

	private = g_static_private_get (&private_key);
	if (!private) {
		g_warning ("Not initialized (%s)",
			   __FUNCTION__);
		return;
	}

	g_static_private_free (&private_key);
}

TrackerConfig *
tracker_data_manager_get_config (void)
{
	TrackerDBPrivate   *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, NULL);

	return private->config;
}

TrackerLanguage *
tracker_data_manager_get_language (void)
{
	TrackerDBPrivate   *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, NULL);

	return private->language;
}

gboolean
tracker_data_manager_exec_no_reply (TrackerDBInterface *iface,
				    const gchar	     *query,
				    ...)
{
	TrackerDBResultSet *result_set;
	va_list		    args;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), FALSE);
	g_return_val_if_fail (query != NULL, FALSE);

	tracker_nfs_lock_obtain ();

	va_start (args, query);
	result_set = tracker_db_interface_execute_vquery (iface, NULL, query, args);
	va_end (args);

	if (result_set) {
		g_object_unref (result_set);
	}

	tracker_nfs_lock_release ();

	return TRUE;
}

TrackerDBResultSet *
tracker_data_manager_exec (TrackerDBInterface *iface,
			   const gchar	    *query,
			   ...)
{
	TrackerDBResultSet *result_set;
	va_list		    args;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (query != NULL, NULL);

	tracker_nfs_lock_obtain ();

	va_start (args, query);
	result_set = tracker_db_interface_execute_vquery (iface,
							  NULL,
							  query,
							  args);
	va_end (args);

	tracker_nfs_lock_release ();

	return result_set;
}

TrackerDBResultSet *
tracker_data_manager_exec_proc (TrackerDBInterface *iface,
			        const gchar	   *procedure,
				...)
{
	TrackerDBResultSet *result_set;
	va_list		    args;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (procedure != NULL, NULL);

	va_start (args, procedure);
	result_set = tracker_db_interface_execute_vprocedure (iface,
							      NULL,
							      procedure,
							      args);
	va_end (args);

	return result_set;
}

gint
tracker_data_manager_get_db_option_int (const gchar *option)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	gchar		   *str;
	gint		    value = 0;

	g_return_val_if_fail (option != NULL, 0);

	/* Here it doesn't matter which one we ask, as long as it has common.db
	 * attached. The service ones are cached connections, so we can use
	 * those instead of asking for an individual-file connection (like what
	 * the original code had) */

	/* iface = tracker_db_manager_get_db_interfaceX (TRACKER_DB_COMMON); */

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	result_set = tracker_data_manager_exec_proc (iface, "GetOption", option, NULL);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &str, -1);

		if (str) {
			value = atoi (str);
			g_free (str);
		}

		g_object_unref (result_set);
	}

	return value;
}

void
tracker_data_manager_set_db_option_int (const gchar *option,
					gint	     value)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	gchar		   *str;

	g_return_if_fail (option != NULL);

	/* Here it doesn't matter which one we ask, as long as it has common.db
	 * attached. The service ones are cached connections, so we can use
	 * those instead of asking for an individual-file connection (like what
	 * the original code had) */

	/* iface = tracker_db_manager_get_db_interfaceX (TRACKER_DB_COMMON); */

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	str = tracker_gint_to_string (value);
	result_set = tracker_data_manager_exec_proc (iface, "SetOption", option, str, NULL);
	g_free (str);

	if (result_set) {
		g_object_unref (result_set);
	}
}
