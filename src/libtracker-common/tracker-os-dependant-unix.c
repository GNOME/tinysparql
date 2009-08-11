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

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <fcntl.h>

#include <glib.h>

#include "tracker-log.h"
#include "tracker-os-dependant.h"

#define MEM_LIMIT 100 * 1024 * 1024

#if defined(__OpenBSD__) && !defined(RLIMIT_AS)
#define RLIMIT_AS RLIMIT_DATA
#endif

#undef DISABLE_MEM_LIMITS

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
	g_return_val_if_fail (timeout >= 0, FALSE);

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
	gint	  tmpstdin, tmpstdout, tmpstderr;

	g_return_val_if_fail (argv != NULL, FALSE);
	g_return_val_if_fail (argv[0] != NULL, FALSE);
	g_return_val_if_fail (timeout >= 0, FALSE);
	g_return_val_if_fail (pid != NULL, FALSE);

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
	gint	      timeout = GPOINTER_TO_INT (user_data);

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

static glong
get_process_memory_usage (void)
{
	gchar *contents = NULL;
	GError *error;
	glong memory = 0;

	if (!g_file_get_contents ("/proc/self/status",
				  &contents,
				  NULL,
				  &error)) {
		g_critical ("Could not get process current memory usage: %s", error->message);
		g_error_free (error);
	} else {
		gchar *p, *end;

		p = contents;
		end = strchr (p, '\n');

		while (p) {
			if (end) {
				*end = '\0';
			}

			if (g_str_has_prefix (p, "VmSize:")) {
				gchar *line_end;

				/* Get VmSize since we actually deal with RLIMIT_AS anyway */
				p += strlen ("VmSize:");
				line_end = strstr (p, "kB");
				*line_end = '\0';

				memory = strtol (p, NULL, 10);
				p = line_end + 1;
			}

			if (end) {
				p = end + 1;
				end = strchr (p, '\n');
			} else {
				p = NULL;
			}
		}

		g_free (contents);
	}

	return memory * 1024;
}

static void
tracker_memory_set_oom_adj (void)
{
	const gchar *str = "15";
	gboolean success = FALSE;
	int fd;

	fd = open ("/proc/self/oom_adj", O_WRONLY);

	if (fd != -1) {
		if (write (fd, str, strlen (str)) > 0) {
			success = TRUE;
		}

		close (fd);
	}

	if (success) {
		g_debug ("OOM score has been set to %s", str);
	} else {
		g_critical ("Could not adjust OOM score");
	}
}

gboolean
tracker_memory_setrlimits (void)
{
#ifndef DISABLE_MEM_LIMITS
	struct rlimit  rl;
	glong          buffer;
	glong          total;
	glong          limit;
	glong          current, ideal;
	gchar         *str1, *str2, *str3;

	total = get_memory_total ();
	current = get_process_memory_usage ();
	buffer = MEM_LIMIT;

#ifdef __x86_64__
	/* We multiply the memory limit here because otherwise it
	 * generally isn't enough. 
	 */
	buffer *= 12;
#endif /* __x86_64__ */

	ideal = current + buffer;

	limit = CLAMP (ideal, 0, total);

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
	}

	getrlimit (RLIMIT_DATA, &rl);
	rl.rlim_cur = limit;
	
	if (setrlimit (RLIMIT_DATA, &rl) == -1) {
		const gchar *str = g_strerror (errno);
		
		g_critical ("Could not set heap memory limit with setrlimit(RLIMIT_DATA), %s",
			    str ? str : "no error given");
		
		return FALSE;
	}

	str1 = g_format_size_for_display (total);
	str2 = g_format_size_for_display (limit);
	str3 = g_format_size_for_display (buffer);
	
	g_message ("Setting memory limitations: total is %s, virtual/heap set to %s (%s buffer)",
		   str1,
		   str2,
		   str3);
	
	g_free (str3);
	g_free (str2);
	g_free (str1);
#endif /* DISABLE_MEM_LIMITS */

	tracker_memory_set_oom_adj ();

	return TRUE;
}
