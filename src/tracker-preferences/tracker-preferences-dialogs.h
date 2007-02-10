#ifndef __TRACKER_PREFERENCES_DIALOGS_H__
#define __TRACKER_PREFERENCES_DIALOGS_H__

/* TODO :: Include only the headers that are needed */
#include <gtk/gtk.h>

gchar *
tracker_preferences_select_folder (void);

gchar *
tracker_preferences_select_pattern (void);

static void
activate_dialog_entry (GtkEntry * entry, gpointer user_data);

#endif
