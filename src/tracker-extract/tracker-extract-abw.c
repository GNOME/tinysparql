#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <glib.h>

void tracker_extract_abw (gchar *filename, GHashTable *metadata)
{
	FILE    *f;
	gchar   *line;
	gsize    length = 0;
   
	if ((f = fopen (filename, "r"))) {
		line = NULL;
		getline (&line, &length, f);
		while (!feof (f)) {
			if (g_str_has_suffix (line, "</m>\n")) {
				line[strlen(line) - 5] = '\0';
			}
			if (g_str_has_prefix (line, "<m key=\"dc.title\">")) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc.Title"), g_strdup (line+18));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.subject\">")) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc.Subject"), g_strdup (line+20));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.creator\">")) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc.Author"), g_strdup (line+20));
			}
			else if (g_str_has_prefix (line, "<m key=\"abiword.keywords\">")) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc.Keywords"), g_strdup (line+26));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.description\">")) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc.Comments"), g_strdup (line+24));
			}
			g_free (line);
			line = NULL;
			getline (&line, &length, f);
		}
		g_free (line);
	}
	fclose (f);
}

