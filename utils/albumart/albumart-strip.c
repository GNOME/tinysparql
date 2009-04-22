/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include <glib/gi18n.h>

#include <libtracker-common/tracker-albumart.h>

static gchar	    **text;

static GOptionEntry   entries[] = {
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_STRING_ARRAY, &text,
	  N_("album or artist"),
	  N_("EXPRESSION")
	},
	{ NULL }
};

int
main (int argc, char *argv[])
{
	GOptionContext  *context;
        gchar           *summary;
	gchar          **p;

	setlocale (LC_ALL, "");

	context = g_option_context_new (_("- Test albumart text stripping"));
	summary = g_strconcat (_("You can use this utility to check album/artist strings, for example:"),
			       "\n",
			       "\n",
			       "  \"[I_am_da_man dwz m1 mp3ieez] Jhonny Dee - "
			       "The tale of bla {Moo 2009, we are great}\"",
			       NULL);
	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_free (summary);

	if (!text || !*text) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _("No album/artist text was provided"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	for (p = text; *p; p++) {
		gchar *output;

		g_print ("\n");

		g_print ("%s:\n", _("Converted to"));

		output = tracker_albumart_strip_invalid_entities (*p);
		g_print ("  %s\n", output);

		g_free (output);
	}
       
        return EXIT_SUCCESS;
}
