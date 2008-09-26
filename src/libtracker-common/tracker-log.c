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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <glib/gstdio.h>

#include "tracker-log.h"

static gboolean  initialized;
static GMutex	*mutex;
static FILE	*fd;
static gint	 verbosity;
static guint	 log_handler_id;

static inline void
log_output (const gchar    *domain,
	    GLogLevelFlags  log_level,
	    const gchar    *message)
{
	time_t	      now;
	gchar	      time_str[64];
	gchar	     *output;
	struct tm    *local_time;
	GTimeVal      current_time;
	const gchar  *log_level_str;
	static gsize  size = 0;

	g_return_if_fail (initialized == TRUE);
	g_return_if_fail (message != NULL && message[0] != '\0');

	/* Ensure file logging is thread safe */
	g_mutex_lock (mutex);

	/* Check log size, 10MiB limit */
	if (size > (10 << 20) && fd) {
		rewind (fd);
		ftruncate (fileno (fd), 0);
		size = 0;
	}

	g_get_current_time (&current_time);

	now = time ((time_t *) NULL);
	local_time = localtime (&now);
	strftime (time_str, 64, "%d %b %Y, %H:%M:%S:", local_time);

	switch (log_level) {
	case G_LOG_LEVEL_WARNING:
		log_level_str = "-Warning **";
		break;

	case G_LOG_LEVEL_CRITICAL:
		log_level_str = "-Critical **";
		break;

	case G_LOG_LEVEL_ERROR:
		log_level_str = "-Error **";
		break;

	default:
		log_level_str = NULL;
		break;
	}

	output = g_strdup_printf ("%s%s %s%s: %s",
				  log_level_str ? "\n" : "",
				  time_str,
				  domain,
				  log_level_str ? log_level_str : "",
				  message);

	if (G_UNLIKELY (fd == NULL)) {
		g_fprintf (stderr, "%s\n", output);
		fflush (stderr);
	} else {
		size += g_fprintf (fd, "%s\n", output);
		fflush (fd);
	}

	g_free (output);

	g_mutex_unlock (mutex);
}

static void
tracker_log_handler (const gchar    *domain,
		     GLogLevelFlags  log_level,
		     const gchar    *message,
		     gpointer	     user_data)
{
	if (((log_level & G_LOG_LEVEL_DEBUG) && verbosity < 3) ||
	    ((log_level & G_LOG_LEVEL_INFO) && verbosity < 2) ||
	    ((log_level & G_LOG_LEVEL_MESSAGE) && verbosity < 1)) {
		return;
	}

	log_output (domain, log_level, message);

	/* Now show the message through stdout/stderr as usual */
	g_log_default_handler (domain, log_level, message, user_data);
}

gboolean
tracker_log_init (const gchar *filename,
		  gint	       this_verbosity)
{
	g_return_val_if_fail (filename != NULL, FALSE);

	if (initialized) {
		return TRUE;
	}

	/* Open file */
	fd = g_fopen (filename, "a");
	if (!fd) {
		const gchar *error_string;

		error_string = g_strerror (errno);
		g_fprintf (stderr,
			   "Could not open log:'%s', %s\n",
			   filename,
			   error_string);
		g_fprintf (stderr,
			   "All logging will go to stderr\n");
	}

	verbosity = this_verbosity;
	mutex = g_mutex_new ();

	/* Add log handler function */
	log_handler_id = g_log_set_handler (NULL,
					    G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL,
					    tracker_log_handler,
					    NULL);

	g_log_set_default_handler (tracker_log_handler, NULL);

	initialized = TRUE;

	return TRUE;
}

void
tracker_log_shutdown (void)
{
	if (!initialized) {
		return;
	}

	if (fd) {
		fclose (fd);
	}

	g_log_remove_handler (NULL, log_handler_id);
	log_handler_id = 0;

	g_mutex_free (mutex);

	initialized = FALSE;
}
