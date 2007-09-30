#include "tm-utils.h"

gchar *
version_int_to_string (guint version)
{
   gint micro = version % 10;
   gint minor = (version % 1000 - micro) / 100;
   gint major = (version - minor - micro) / 10000;

   return g_strdup_printf ("%d.%d.%d", major, minor, micro, NULL);
}
