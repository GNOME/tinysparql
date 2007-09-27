#include "tm-utils.h"

gchar *
version_int_to_string(guint version)
{
   int micro = version % 10;
   int minor = (version % 1000 - micro) / 100;
   int major = (version - minor - micro) / 10000;

   return g_strdup_printf("%d.%d.%d", major, minor, micro, NULL);
}
