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
	  N_("genre"),
	  N_("EXPRESSION")
	},
	{ NULL }
};

static void
improve_handwritten_genre (gchar *genre)
{
	/* This function tries to make each first letter of each word
	 * upper case so we conform a bit more to the standards, for
	 * example, if it is "Fusion jazz", we want "Fussion Jazz" to
	 * make things more consistent.
	 */
        gchar *p;
	gunichar c;
	gboolean set_next;

	if (!genre) {
		return;
	}

	c = g_utf8_get_char (genre);
	*genre = g_unichar_toupper (c);

        for (p = genre, set_next = FALSE; *p; p = g_utf8_next_char (p)) {
		GUnicodeBreakType t;

                c = g_utf8_get_char (p);
		t = g_unichar_break_type (c);

		if (set_next) {
			*p = g_unichar_toupper (c);
			set_next = FALSE;
		}

		switch (t) {
		case G_UNICODE_BREAK_MANDATORY:
		case G_UNICODE_BREAK_CARRIAGE_RETURN:
		case G_UNICODE_BREAK_LINE_FEED:
		case G_UNICODE_BREAK_COMBINING_MARK:
		case G_UNICODE_BREAK_SURROGATE:
		case G_UNICODE_BREAK_ZERO_WIDTH_SPACE:
		case G_UNICODE_BREAK_INSEPARABLE:
		case G_UNICODE_BREAK_NON_BREAKING_GLUE:
		case G_UNICODE_BREAK_CONTINGENT:
		case G_UNICODE_BREAK_SPACE:
		case G_UNICODE_BREAK_HYPHEN:
		case G_UNICODE_BREAK_EXCLAMATION:
		case G_UNICODE_BREAK_WORD_JOINER:
		case G_UNICODE_BREAK_NEXT_LINE:
		case G_UNICODE_BREAK_SYMBOL:
			set_next = TRUE;

		case G_UNICODE_BREAK_AFTER:
		case G_UNICODE_BREAK_BEFORE:
		case G_UNICODE_BREAK_BEFORE_AND_AFTER:
		case G_UNICODE_BREAK_NON_STARTER:
		case G_UNICODE_BREAK_OPEN_PUNCTUATION:
		case G_UNICODE_BREAK_CLOSE_PUNCTUATION:
		case G_UNICODE_BREAK_QUOTATION:
		case G_UNICODE_BREAK_IDEOGRAPHIC:
		case G_UNICODE_BREAK_NUMERIC:
		case G_UNICODE_BREAK_INFIX_SEPARATOR:
		case G_UNICODE_BREAK_ALPHABETIC:
		case G_UNICODE_BREAK_PREFIX:
		case G_UNICODE_BREAK_POSTFIX:
		case G_UNICODE_BREAK_COMPLEX_CONTEXT:
		case G_UNICODE_BREAK_AMBIGUOUS:
		case G_UNICODE_BREAK_UNKNOWN:
		case G_UNICODE_BREAK_HANGUL_L_JAMO:
		case G_UNICODE_BREAK_HANGUL_V_JAMO:
		case G_UNICODE_BREAK_HANGUL_T_JAMO:
		case G_UNICODE_BREAK_HANGUL_LV_SYLLABLE:
		case G_UNICODE_BREAK_HANGUL_LVT_SYLLABLE:
			break;
		}
        }
}

int
main (int argc, char *argv[])
{
	GOptionContext  *context;
        gchar           *summary;
	gchar          **p;

	setlocale (LC_ALL, "");

	context = g_option_context_new (_("- Test MP3 handwritten genre conversion to leading uppercase"));
	summary = g_strconcat (_("You can use this to check genre strings get converted correctly, for example:"),
			       "\n",
			       "\n",
			       "  \"fOo-bar bAz ping/pong sliff's & sloffs\"",
			       "\n",
			       "\n",
			       _("Should be converted to"),
			       "\n",
			       "\n",
			       "  \"FOo-Bar BAz Ping/Pong Sliff's & Sloffs\"",
			       NULL);
	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_free (summary);

	if (!text || !*text) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _("No genre text was provided"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	for (p = text; *p; p++) {
		g_print ("\n");

		g_print ("%s:\n", _("Converted to"));

		improve_handwritten_genre (*p);
		g_print ("  %s\n", *p);
	}
       
        return EXIT_SUCCESS;
}
