#ifndef _TRACKER_OS_DEPENDANT_H_
#define _TRACKER_OS_DEPENDANT_H_

#include <glib/gstdio.h>

gboolean tracker_check_uri (const char* uri);
gboolean tracker_spawn (char **argv, int timeout, char **tmp_stdout, int *exit_status);
void     tracker_child_cb (gpointer user_data);
char*    tracker_create_permission_string (struct stat finfo);

#endif
