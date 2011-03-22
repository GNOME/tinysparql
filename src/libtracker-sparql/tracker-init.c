#include "config.h"

#ifdef HAVE_MAEMO

/* Information about this chunk of code at
 * src/libtracker-common/tracker-locale.c */

#include <glib.h>
#include <gconf/gconf-client.h>

__attribute__ ((constructor))
static void
init_gconf_client (void)
{
	g_type_init ();
	gconf_client_get_default ();
}

#endif /* HAVE_MAEMO */

