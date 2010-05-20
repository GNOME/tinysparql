/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <glib.h>
#include <gio/gio.h>
#include <totem-pl-parser.h>

static gchar    **filenames = NULL;

typedef struct {
	gint   tracks ;
	gchar *playlist;
} PlaylistData;

static GOptionEntry entries[] = {
	{ G_OPTION_REMAINING, 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_FILENAME_ARRAY, &filenames,
	  "FILE",
	  NULL
	},
	{ NULL }
};

static void
print_header ()
{
	g_print ("@prefix nmo: <http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#>.\n");
	g_print ("@prefix nfo: <http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>.\n");
	g_print ("@prefix xsd: <http://www.w3.org/2001/XMLSchema#>.\n");
	g_print ("\n");
}

static void
print_playlist_entry (const gchar *uri) {
	g_print ("<%s> a nmm:Playlist .\n\n", uri);
}

static void
entry_parsed (TotemPlParser *parser, const gchar *uri, GHashTable *metadata, gpointer user_data)
{
	PlaylistData *playlist_data = (PlaylistData *)user_data;

	playlist_data->tracks += 1;

	//uri = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_URI);
	g_print ("<%s> nfo:hasMediaFileListEntry [ \n",
	         playlist_data->playlist);
	g_print ("\t a nfo:MediaFileListEntry ; \n");
	g_print ("\t nfo:listPosition %d ; \n", playlist_data->tracks);
	g_print ("\t nfo:entryContent <%s> ] .\n\n", uri);
}

gint
main (gint argc, gchar **argv)
{
	GFile *file;
	GOptionContext *context = NULL;
	gchar *uri;
	PlaylistData   playlist_data = { 0, NULL};
	TotemPlParser *pl;
	TotemPlParserResult result;
	GError *error = NULL;

	g_type_init ();

	context = g_option_context_new ("- Parse a playlist and show info");

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (!g_option_context_parse (context, &argc, &argv, &error) || !filenames) {
		gchar *help;

		g_printerr ("%s\n\n",
		            "Playlist filename is mandatory");

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return -1;
	}

	file = g_file_new_for_commandline_arg (filenames[0]);
	uri = g_file_get_uri (file);

	print_header ();
	print_playlist_entry (uri);
	playlist_data.playlist = uri;

	pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", FALSE, "disable-unsafe", TRUE, NULL);
	g_signal_connect (G_OBJECT (pl), "entry-parsed", G_CALLBACK (entry_parsed), &playlist_data);



	result = totem_pl_parser_parse (pl, uri, FALSE);

	switch (result) {
	case TOTEM_PL_PARSER_RESULT_SUCCESS:
		break;
	case TOTEM_PL_PARSER_RESULT_IGNORED:
		g_print ("Error: Ignored (%s)\n", uri);
		break;
	case TOTEM_PL_PARSER_RESULT_ERROR:
		g_print ("Error: Failed parsing (%s)\n", uri);
		break;
	case TOTEM_PL_PARSER_RESULT_UNHANDLED:
		g_print ("Error: Unhandled type (%s)\n", uri);
		break;
	default:
		g_print ("Undefined result!?!?!");
	}

	g_object_unref (pl);

	return 0;
}
