
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>

void
tracker_extract_imagemagick (gchar *filename, GHashTable *metadata)
{
	gchar         *argv[6];
	gchar         *identify;
	gchar        **lines;

	argv[0] = g_strdup ("identify");
	argv[1] = g_strdup ("-format");
	argv[2] = g_strdup ("%w;\\n%h;\\n%c;\\n");
	argv[3] = g_strdup ("-ping");
	argv[4] = g_strdup (filename);
	argv[5] = NULL;

	if(g_spawn_sync (NULL,
	                 argv,
	                 NULL,
	                 G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
	                 NULL,
	                 NULL,
	                 &identify,
	                 NULL,
	                 NULL,
	                 NULL)) {

		lines = g_strsplit (identify, ";\n", 4);
		g_hash_table_insert (metadata, g_strdup ("Image.Width"), g_strdup (lines[0]));
		g_hash_table_insert (metadata, g_strdup ("Image.Height"), g_strdup (lines[1]));
		g_hash_table_insert (metadata, g_strdup ("Image.Comments"), g_strdup (g_strescape (lines[2], "")));
	}
}

