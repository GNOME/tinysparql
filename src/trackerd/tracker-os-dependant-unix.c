#include "tracker-os-dependant.h"

#include <sys/resource.h>

#include <glib/gspawn.h>
#include <glib/gstring.h>

#include <glib.h>

#include <unistd.h>



#define MAX_MEM 128
#define MAX_MEM_AMD64 512

gboolean
tracker_check_uri (const char* uri)
{
  return (uri && (uri[0] == G_DIR_SEPARATOR));
}

gboolean
tracker_spawn (char **argv, int timeout, char **tmp_stdout, int *exit_status)
{
	GSpawnFlags flags;

	if (!tmp_stdout) {
		flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |  G_SPAWN_STDERR_TO_DEV_NULL;
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

char *
tracker_create_permission_string(struct stat finfo)
{
        char *str;
	int n, bit;

	/* create permissions string */
	str = g_strdup ("?rwxrwxrwx");

	switch (finfo.st_mode & S_IFMT) {
		case S_IFSOCK: str[0] = 's'; break;
		case S_IFIFO: str[0] = 'p'; break;
		case S_IFLNK: str[0] = 'l'; break;
		case S_IFCHR: str[0] = 'c'; break;
		case S_IFBLK: str[0] = 'b'; break;
		case S_IFDIR: str[0] = 'd'; break;
		case S_IFREG: str[0] = '-'; break;
	}

	for (bit = 0400, n = 1 ; bit ; bit >>= 1, ++n) {
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

	struct	rlimit rl;
	gboolean fail = FALSE;

	/* We want to limit the max virtual memory
	 * most extractors use mmap() so only virtual memory can be effectively limited */
#ifdef __x86_64__
	/* many extractors on AMD64 require 512M of virtual memory, so we limit heap too */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = MAX_MEM_AMD64*1024*1024;
	fail |= setrlimit (RLIMIT_AS, &rl);

	getrlimit (RLIMIT_DATA, &rl);
	rl.rlim_cur = MAX_MEM*1024*1024;
	fail |= setrlimit (RLIMIT_DATA, &rl);
#else
	/* on other architectures, 128M of virtual memory seems to be enough */
	getrlimit (RLIMIT_AS, &rl);
	rl.rlim_cur = MAX_MEM*1024*1024;
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
	struct  rlimit cpu_limit;
	int	timeout = GPOINTER_TO_INT (user_data);

	/* set cpu limit */
	getrlimit (RLIMIT_CPU, &cpu_limit);
	cpu_limit.rlim_cur = timeout;
	cpu_limit.rlim_max = timeout+1;

	if (setrlimit (RLIMIT_CPU, &cpu_limit) != 0) {
		g_print ("ERROR: trying to set resource limit for cpu\n");
	}

	set_memory_rlimits();

	/* Set child's niceness to 19 */
	nice (19);
}
