#include "tracker-os-dependant.h"

#include <glib/gspawn.h>
#include <glib/gstring.h>

#include "mingw-compat.h"

gboolean
tracker_check_uri (const char* uri)
{
  return uri != NULL;
}

gboolean
tracker_spawn (char **argv, int timeout, char **tmp_stdout, int *exit_status)
{
  int length;

  int i = 0;
  while (argv[i] != NULL)
    ++i;

  length = i;

  char *new_argv[length+3];

  new_argv[0] = "cmd.exe";
  new_argv[1] = "/c";
  
  i = 0;
  while (argv[i] != NULL) {
    new_argv[i+2] = argv[i];
    ++i;
  }
  new_argv[i+2] = NULL;

	GSpawnFlags flags;
	GError *error = NULL;

	if (!tmp_stdout) {
		flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |  G_SPAWN_STDERR_TO_DEV_NULL;
	} else {
		flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL;
	}

	gboolean status = g_spawn_sync (NULL,
			  new_argv,
			  NULL,
			  flags,
			  NULL,
			  GINT_TO_POINTER (timeout),
			  tmp_stdout,
			  NULL,
			  exit_status,
			  &error);

	if (!status) {
	  tracker_log(error->message);
	  g_error_free(error);	
	}

	return status;
}

char *tracker_create_permission_string(struct stat finfo)
{
        char *str;
	int n, bit;

	/* create permissions string */
	str = g_strdup ("?rwxrwxrwx");

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

void
tracker_child_cb (gpointer user_data)
{}
