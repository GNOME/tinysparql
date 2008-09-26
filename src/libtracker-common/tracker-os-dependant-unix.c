/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>

#include <glib.h>
#include <glib/gspawn.h>
#include <glib/gstring.h>

#include "tracker-log.h"
#include "tracker-os-dependant.h"

#define MAX_MEM       128
#define MAX_MEM_AMD64 512

gboolean
tracker_spawn (gchar **argv,
	       gint    timeout,
	       gchar **tmp_stdout,
	       gint   *exit_status)
{
	GError	    *error = NULL;
	GSpawnFlags  flags;
	gboolean     result;

	g_return_val_if_fail (argv != NULL, FALSE);
	g_return_val_if_fail (argv[0] != NULL, FALSE);
	g_return_val_if_fail (timeout > 0, FALSE);

	flags = G_SPAWN_SEARCH_PATH |
		G_SPAWN_STDERR_TO_DEV_NULL;

	if (!tmp_stdout) {
		flags = flags | G_SPAWN_STDOUT_TO_DEV_NULL;
	}

	result = g_spawn_sync (NULL,
			       argv,
			       NULL,
			       flags,
			       tracker_spawn_child_func,
			       GINT_TO_POINTER (timeout),
			       tmp_stdout,
			       NULL,
			       exit_status,
			       &error);

	if (error) {
		g_warning ("Could not spawn command:'%s', %s",
			   argv[0],
			   error->message);
		g_error_free (error);
	}

	return result;
}

gboolean
tracker_spawn_async_with_channels (const gchar **argv,
				   gint		 timeout,
				   GPid		*pid,
				   GIOChannel  **stdin_channel,
				   GIOChannel  **stdout_channel,
				   GIOChannel  **stderr_channel)
{
	GError	 *error = NULL;
	gboolean  result;
	gint	  stdin, stdout, stderr;

	g_return_val_if_fail (argv != NULL, FALSE);
	g_return_val_if_fail (argv[0] != NULL, FALSE);
	g_return_val_if_fail (timeout > 0, FALSE);
	g_return_val_if_fail (pid != NULL, FALSE);

	result = g_spawn_async_with_pipes (NULL,
					   (gchar **) argv,
					   NULL,
					   G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
					   tracker_spawn_child_func,
					   GINT_TO_POINTER (timeout),
					   pid,
					   stdin_channel ? &stdin : NULL,
					   stdout_channel ? &stdout : NULL,
					   stderr_channel ? &stderr : NULL,
					   &error);

	if (error) {
		g_warning ("Could not spawn command:'%s', %s",
			   argv[0],
			   error->message);
		g_error_free (error);
	}

	if (stdin_channel) {
		*stdin_channel = result ? g_io_channel_unix_new (stdin) : NULL;
	}

	if (stdout_channel) {
		*stdout_channel = result ? g_io_channel_unix_new (stdout) : NULL;
	}

	if (stderr_channel) {
		*stderr_channel = result ? g_io_channel_unix_new (stderr) : NULL;
	}

	return result;
}

void
tracker_spawn_child_func (gpointer user_data)
{
	struct rlimit cpu_limit;
	gint	      timeout = GPOINTER_TO_INT (user_data);

	/* set cpu limit */
	getrlimit (RLIMIT_CPU, &cpu_limit);
	cpu_limit.rlim_cur = timeout;
	cpu_limit.rlim_max = timeout + 1;

	if (setrlimit (RLIMIT_CPU, &cpu_limit) != 0) {
		g_critical ("Failed to set resource limit for CPU");
	}

	tracker_memory_setrlimits ();

	/* Set child's niceness to 19 */
	errno = 0;

	/* nice() uses attribute "warn_unused_result" and so complains
	 * if we do not check its returned value. But it seems that
	 * since glibc 2.2.4, nice() can return -1 on a successful call
	 * so we have to check value of errno too. Stupid...
	 */
	if (nice (19) == -1 && errno) {
		g_warning ("Failed to set nice value");
	}

	/* Have this as a precaution in cases where cpu limit has not
	 * been reached due to spawned app sleeping.
	 */
	alarm (timeout + 2);
}

gchar *
tracker_create_permission_string (struct stat finfo)
{
	gchar *str;
	gint   n, bit;

	/* Create permissions string */
	str = g_strdup ("?rwxrwxrwx");

	switch (finfo.st_mode & S_IFMT) {
		case S_IFSOCK: str[0] = 's'; break;
		case S_IFIFO:  str[0] = 'p'; break;
		case S_IFLNK:  str[0] = 'l'; break;
		case S_IFCHR:  str[0] = 'c'; break;
		case S_IFBLK:  str[0] = 'b'; break;
		case S_IFDIR:  str[0] = 'd'; break;
		case S_IFREG:  str[0] = '-'; break;
	}

	for (bit = 0400, n = 1; bit; bit >>= 1, ++n) {
		if (!(finfo.st_mode & bit)) {
			str[n] = '-';
		}
	}

	if (finfo.st_mode & S_ISUID) {
		str[3] = (finfo.st_mode & S_IXUSR) ? 's' : 'S';
	}

	if (finfo.st_mode & S_ISGID) {
		str[6] = (finfo.st_mode & S_IXGRP) ? 's' : 'S';
	}

	if (finfo.st_mode & S_ISVTX) {
		str[9] = (finfo.st_mode & S_IXOTH) ? 't' : 'T';
	}

	return str;
}

gboolean
tracker_memory_setrlimits (void)
{
	struct rlimit rl;
	gboolean      fail = FALSE;

	/* We want to limit the max virtual memory
	 * most extractors use mmap() so only virtual memory can be
	 * effectively limited.
	 */
#ifdef __x86_64__
	/* Many extractors on AMD64 require 512M of virtual memory, so
	 * we limit heap too.
	 */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = MAX_MEM_AMD64 * 1024 * 1024;
	fail |= setrlimit (RLIMIT_AS, &rl);

	getrlimit (RLIMIT_DATA, &rl);
	rl.rlim_cur = MAX_MEM * 1024 * 1024;
	fail |= setrlimit (RLIMIT_DATA, &rl);
#else  /* __x86_64__ */
	/* On other architectures, 128M of virtual memory seems to be
	 * enough.
	 */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = MAX_MEM * 1024 * 1024;
	fail |= setrlimit (RLIMIT_AS, &rl);
#endif /* __x86_64__ */

	if (fail) {
		g_critical ("Error trying to set memory limit");
	}

	return !fail;
}
