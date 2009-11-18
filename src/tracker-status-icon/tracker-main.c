/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <locale.h>

#include "tracker-status-icon.h"
#include "tomboykeybinder.h"

#include <glib.h>
#include <glib/gi18n.h>

static gboolean print_version;

/* Unused at the moment */
static GOptionEntry entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL},
	{ NULL }
};

int
main (int argc, char *argv[])
{
        GtkStatusIcon *icon;
	GOptionContext *context;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	setlocale (LC_ALL, "");

	context = g_option_context_new (_("- Miner status and control notification area icon"));

	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (print_version) {
		g_print ("%s\n", PACKAGE_STRING);
		return EXIT_SUCCESS;
	}

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

        gtk_init (&argc, &argv);
	tomboy_keybinder_init ();

	gtk_window_set_default_icon_name ("tracker");
	g_set_application_name ("tracker-status-icon");

        icon = tracker_status_icon_new ();

        gtk_main ();

        return EXIT_SUCCESS;
}
