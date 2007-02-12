#ifndef __TRACKER_PREFERENCES_UTILS_H__
#define __TRACKER_PREFERENCES_UTILS_H__

#include <glib.h>

gchar *
get_claws_command (void);

gchar *
get_thunderbird_command (void);

gchar *
get_evolution_command (void);

gchar *
get_kmail_command (void);

gboolean
evolution_available (void);

gboolean
thunderbird_available (void);

gboolean
kmail_available (void);

gboolean
convert_available (void);
#endif
