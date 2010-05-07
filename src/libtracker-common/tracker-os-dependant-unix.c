/*
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>

#include <glib.h>

#include "tracker-log.h"
#include "tracker-os-dependant.h"

/* Maximum here is a G_MAXLONG, so if you want to use > 2GB, you have
 * to set MEM_LIMIT to RLIM_INFINITY
 */
#ifdef __x86_64__
#define MEM_LIMIT 512 * 1024 * 1024
#else
#define MEM_LIMIT 80 * 1024 * 1024
#endif

#if defined(__OpenBSD__) && !defined(RLIMIT_AS)
#define RLIMIT_AS RLIMIT_DATA
#endif

#define DISABLE_MEM_LIMITS

gboolean
tracker_spawn (gchar **argv,
               gint    timeout,
               gchar **tmp_stdout,
               gchar **tmp_stderr,
               gint   *exit_status)
{
	GError      *error = NULL;
	GSpawnFlags  flags;
	gboolean     result;

	g_return_val_if_fail (argv != NULL, FALSE);
	g_return_val_if_fail (argv[0] != NULL, FALSE);
	g_return_val_if_fail (timeout >= 0, FALSE);

	flags = G_SPAWN_SEARCH_PATH;

	if (tmp_stderr == NULL)
		flags |= G_SPAWN_STDERR_TO_DEV_NULL;

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
	                       tmp_stderr,
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
                                   gint          timeout,
                                   GPid         *pid,
                                   GIOChannel  **stdin_channel,
                                   GIOChannel  **stdout_channel,
                                   GIOChannel  **stderr_channel)
{
	GError   *error = NULL;
	gboolean  result;
	gint      tmpstdin, tmpstdout, tmpstderr;

	g_return_val_if_fail (argv != NULL, FALSE);
	g_return_val_if_fail (argv[0] != NULL, FALSE);
	g_return_val_if_fail (timeout >= 0, FALSE);
	g_return_val_if_fail (pid != NULL, FALSE);

	/* Note: PID must be non-NULL because we're using the
	 *  G_SPAWN_DO_NOT_REAP_CHILD option, so an explicit call to
	 *  g_spawn_close_pid () will be needed afterwards */

	result = g_spawn_async_with_pipes (NULL,
	                                   (gchar **) argv,
	                                   NULL,
	                                   G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
	                                   tracker_spawn_child_func,
	                                   GINT_TO_POINTER (timeout),
	                                   pid,
	                                   stdin_channel ? &tmpstdin : NULL,
	                                   stdout_channel ? &tmpstdout : NULL,
	                                   stderr_channel ? &tmpstderr : NULL,
	                                   &error);

	if (error) {
		g_warning ("Could not spawn command:'%s', %s",
		           argv[0],
		           error->message);
		g_error_free (error);
	}

	if (stdin_channel) {
		*stdin_channel = result ? g_io_channel_unix_new (tmpstdin) : NULL;
	}

	if (stdout_channel) {
		*stdout_channel = result ? g_io_channel_unix_new (tmpstdout) : NULL;
	}

	if (stderr_channel) {
		*stderr_channel = result ? g_io_channel_unix_new (tmpstderr) : NULL;
	}

	return result;
}

void
tracker_spawn_child_func (gpointer user_data)
{
	struct rlimit cpu_limit;
	gint          timeout = GPOINTER_TO_INT (user_data);

	if (timeout > 0) {
		/* set cpu limit */
		getrlimit (RLIMIT_CPU, &cpu_limit);
		cpu_limit.rlim_cur = timeout;
		cpu_limit.rlim_max = timeout + 1;

		if (setrlimit (RLIMIT_CPU, &cpu_limit) != 0) {
			g_critical ("Failed to set resource limit for CPU");
		}

		/* Have this as a precaution in cases where cpu limit has not
		 * been reached due to spawned app sleeping.
		 */
		alarm (timeout + 2);
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
	default:
		/* By default a regular file */
		str[0] = '-';
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

#ifndef DISABLE_MEM_LIMITS

static guint
get_memory_total (void)
{
	GError      *error = NULL;
	const gchar *filename;
	gchar       *contents = NULL;
	glong        total = 0;

	filename = "/proc/meminfo";

	if (!g_file_get_contents (filename,
	                          &contents,
	                          NULL,
	                          &error)) {
		g_critical ("Couldn't get memory information:'%s', %s",
		            filename,
		            error ? error->message : "no error given");
		g_clear_error (&error);
	} else {
		gchar *start, *end, *p;

		start = "MemTotal:";
		end = "kB";

		p = strstr (contents, start);
		if (p) {
			p += strlen (start);
			end = strstr (p, end);

			if (end) {
				*end = '\0';
				total = 1024 * atol (p);
			}
		}

		g_free (contents);
	}

	if (!total) {
		/* Setting limit to an arbitary limit */
		total = RLIM_INFINITY;
	}

	return total;
}

#endif /* DISABLE_MEM_LIMITS */

gboolean
tracker_memory_setrlimits (void)
{
#ifndef DISABLE_MEM_LIMITS
	struct rlimit rl;
	glong         total;
	glong         limit;

	total = get_memory_total ();
	limit = CLAMP (MEM_LIMIT, 0, total);

	/* We want to limit the max virtual memory
	 * most extractors use mmap() so only virtual memory can be
	 * effectively limited.
	 */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = limit;

	if (setrlimit (RLIMIT_AS, &rl) == -1) {
		const gchar *str = g_strerror (errno);

		g_critical ("Could not set virtual memory limit with setrlimit(RLIMIT_AS), %s",
		            str ? str : "no error given");

		return FALSE;
	} else {
		getrlimit (RLIMIT_DATA, &rl);
		rl.rlim_cur = limit;

		if (setrlimit (RLIMIT_DATA, &rl) == -1) {
			const gchar *str = g_strerror (errno);

			g_critical ("Could not set heap memory limit with setrlimit(RLIMIT_DATA), %s",
			            str ? str : "no error given");

			return FALSE;
		} else {
			gchar *str1, *str2;

			str1 = g_format_size_for_display (total);
			str2 = g_format_size_for_display (limit);

			g_message ("Setting memory limitations: total is %s, virtual/heap set to %s",
			           str1,
			           str2);

			g_free (str2);
			g_free (str1);
		}
	}
#endif /* DISABLE_MEM_LIMITS */

	return TRUE;
}
