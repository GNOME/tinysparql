/* Tracker - indexer and metadata database engine
 * Copyright (C) 2007, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <glib.h>
#include <glib/gspawn.h>
#include <glib/gstring.h>

#include "tracker-os-dependant.h"


#define MAX_MEM 128
#define MAX_MEM_AMD64 512


gboolean
tracker_check_uri (const gchar *uri)
{
        return uri && uri[0] == G_DIR_SEPARATOR;
}


gboolean
tracker_spawn (gchar **argv, gint timeout, gchar **tmp_stdout, gint *exit_status)
{
	GSpawnFlags flags;

	if (!tmp_stdout) {
		flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL;
	} else {
		flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL;
	}

	return g_spawn_sync (NULL,
                             argv,
                             NULL,
                             flags,
                             tracker_child_cb,
                             GINT_TO_POINTER (timeout),
                             tmp_stdout,
                             NULL,
                             exit_status,
                             NULL);
}


gchar *
tracker_create_permission_string (struct stat finfo)
{
        gchar   *str;
	gint    n, bit;

	/* create permissions string */
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


static gboolean
set_memory_rlimits (void)
{
	struct rlimit   rl;
	gboolean        fail = FALSE;

	/* We want to limit the max virtual memory
	 * most extractors use mmap() so only virtual memory can be effectively limited */
#ifdef __x86_64__
	/* many extractors on AMD64 require 512M of virtual memory, so we limit heap too */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = MAX_MEM_AMD64 * 1024 * 1024;
	fail |= setrlimit (RLIMIT_AS, &rl);

	getrlimit (RLIMIT_DATA, &rl);
	rl.rlim_cur = MAX_MEM * 1024 * 1024;
	fail |= setrlimit (RLIMIT_DATA, &rl);
#else
	/* on other architectures, 128M of virtual memory seems to be enough */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = MAX_MEM * 1024 * 1024;
	fail |= setrlimit (RLIMIT_AS, &rl);
#endif

	if (fail) {
		g_printerr ("Error trying to set memory limit\n");
	}

	return !fail;
}


void
tracker_child_cb (gpointer user_data)
{
	struct rlimit   cpu_limit;
	gint            timeout = GPOINTER_TO_INT (user_data);

	/* set cpu limit */
	getrlimit (RLIMIT_CPU, &cpu_limit);
	cpu_limit.rlim_cur = timeout;
	cpu_limit.rlim_max = timeout+1;

	if (setrlimit (RLIMIT_CPU, &cpu_limit) != 0) {
		g_printerr ("ERROR: trying to set resource limit for cpu\n");
	}

	set_memory_rlimits ();

	/* Set child's niceness to 19 */
        errno = 0;
        /* nice() uses attribute "warn_unused_result" and so complains if we do not check its
           returned value. But it seems that since glibc 2.2.4, nice() can return -1 on a
           successful call so we have to check value of errno too. Stupid... */
        if (nice (19) == -1 && errno) {
                g_printerr ("ERROR: trying to set nice value\n");
        }

	/* have this as a precaution in cases where cpu limit has not been reached due to spawned app sleeping */
	alarm (timeout+2);

}
