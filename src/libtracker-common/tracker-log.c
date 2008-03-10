/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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
#include <glib/gstdio.h> 

#ifdef OS_WIN32
#include <conio.h>
#include "mingw-compat.h"
#else
#include <sys/resource.h>
#endif

#include "tracker-log.h"

typedef struct {
	GMutex   *mutex;
	gchar    *filename;
	gint      verbosity;
	gboolean  abort_on_error;
} TrackerLog;

static TrackerLog *log = NULL;

static inline void
log_output (const char *message)
{
	FILE		*fd;
	time_t	  	 now;
	gchar	 	 time_str[64];
	gchar            usec_str[20];
	gchar		*output;
	struct tm	*local_time;
	GTimeVal	 current_time;
	static size_t    size = 0;

	g_return_if_fail (log != NULL);
	g_return_if_fail (message != NULL && message[0] != '\0');

	g_print ("%s\n", message);

	/* Ensure file logging is thread safe */
	g_mutex_lock (log->mutex);

	fd = g_fopen (log->filename, "a");
	if (!fd) {
		g_warning ("Could not open log: '%s'", log->filename);
		g_mutex_unlock (log->mutex);
		return;
	}

	/* Check log size, 10MiB limit */
	if (size > (10 << 20)) {
		rewind (fd);
		ftruncate (fileno (fd), 0);
		size = 0;
	}

	g_get_current_time (&current_time);

	now = time ((time_t *) NULL);
	local_time = localtime (&now);
	strftime (time_str, 64, "%d %b %Y, %H:%M:%S:", local_time);
	g_sprintf (usec_str, "%03ld", current_time.tv_usec / 1000); 

	output = g_strdup_printf ("%s%s - %s", 
				  time_str, 
				  usec_str, 
				  message);

	size += g_fprintf (fd, "%s\n", output);
	g_free (output);

	fclose (fd);

	g_mutex_unlock (log->mutex);
}

void
tracker_log_init (const gchar   *filename, 
		  gint           verbosity,
                  gboolean       abort_on_error) 
{
	g_return_if_fail (filename != NULL);
	
	log = g_new0 (TrackerLog, 1);

	log->verbosity = verbosity;

	log->filename = g_strdup (filename);

	log->mutex = g_mutex_new ();
	log->abort_on_error = abort_on_error;
}

void
tracker_log_term (void) 
{
	g_return_if_fail (log != NULL);

	g_mutex_free (log->mutex);
	g_free (log->filename);

	g_free (log);
}

void
tracker_log (const char *message, ...)
{
	va_list  args;
	gchar	*str;

	g_return_if_fail (log != NULL);

	if (log->verbosity < 1) {
		return;
	}

	va_start (args, message);
	str = g_strdup_vprintf (message, args);
	va_end (args);

	log_output (str);
	g_free (str);
}

void
tracker_info (const char *message, ...)
{
	va_list  args;
	gchar	*str;

	g_return_if_fail (log != NULL);

	if (log->verbosity < 2) {
		return;
	}

	va_start (args, message);
	str = g_strdup_vprintf (message, args);
	va_end (args);

	log_output (str);
	g_free (str);
}

void
tracker_debug (const char *message, ...)
{
	va_list  args;
	gchar	*str;

	g_return_if_fail (log != NULL);

	if (log->verbosity < 3) {
		return;
	}

	va_start (args, message);
	str = g_strdup_vprintf (message, args);
	va_end (args);

	log_output (str);
	g_free (str);
}

void
tracker_error (const char *message, ...)
{
	va_list  args;
	gchar	*str;

	g_return_if_fail (log != NULL);

	va_start (args, message);
	str = g_strdup_vprintf (message, args);
	va_end (args);

	log_output (str);
	g_free (str);

	if (log->abort_on_error) {
		g_assert (FALSE);
	}
}
