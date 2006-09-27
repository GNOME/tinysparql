#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <glib.h>

void tracker_extract_ps (gchar *filename, GHashTable *metadata)
{
	FILE        *f;
	gchar       *line;
	gsize        length = 0;
	gboolean     pageno_atend = FALSE;
	gboolean     header_finished = FALSE;
   
	if((f = fopen (filename, "r"))) {
		line = NULL;
		getline (&line, &length, f);
		while (!feof (f)) {
			line[strlen(line) - 1] = '\0';  /* overwrite \n char */
			if (!header_finished
					&& strncmp (line, "%%Copyright:", 12) == 0) {
				g_hash_table_insert (metadata,
					g_strdup ("File.Other"), g_strdup (line+13));
			}
			else if (!header_finished
					&& strncmp (line, "%%Title:", 8) == 0) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc.Title"), g_strdup (line+9));
			}
			else if (!header_finished
					&& strncmp (line, "%%Creator:", 10) == 0) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc.Author"), g_strdup (line+11));
			}
			else if (!header_finished
					&& strncmp (line, "%%CreationDate:", 15) == 0) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc.Created"), g_strdup (line+16));
			}
			else if (strncmp (line, "%%Pages:", 8) == 0) {
				if (strcmp (line+9, "(atend)") == 0)
					pageno_atend = TRUE;
				else
					g_hash_table_insert (metadata,
						g_strdup ("Doc.PageCount"), g_strdup (line+9));
			}
			else if (strncmp (line, "%%EndComments", 14) == 0) {
				header_finished = TRUE;
				if (!pageno_atend)
					break;
			}
			g_free (line);
			line = NULL;
			getline (&line, &length, f);
		}
		g_free (line);
	}
	fclose (f);
}
