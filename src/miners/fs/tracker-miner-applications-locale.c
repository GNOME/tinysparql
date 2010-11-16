/*
 * Copyright (C) 2010 Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>

#include <libtracker-common/tracker-locale.h>

#include "tracker-miner-applications-locale.h"
#ifdef HAVE_MEEGOTOUCH
#include "tracker-miner-applications-meego.h"
#endif

#define TRACKER_MINER_APPLICATIONS_LOCALE_FILE "miner-applications-locale.txt"

static gchar *
miner_applications_locale_get_previous (const gchar *locale_file)
{
	gchar *locale = NULL;

	if (G_LIKELY (g_file_test (locale_file, G_FILE_TEST_EXISTS))) {
		gchar *contents;

		/* Check locale is correct */
		if (G_LIKELY (g_file_get_contents (locale_file, &contents, NULL, NULL))) {
			if (contents && strlen (contents) == 0) {
				g_critical ("  Empty locale file found at '%s'", locale_file);
				g_free (contents);
			} else {
				/* Re-use contents */
				locale = contents;
			}
		} else {
			g_critical ("  Could not get content of file '%s'", locale_file);
		}
	} else {
		g_critical ("  Could not find locale file:'%s'", locale_file);
	}

	return locale;
}

static void
miner_applications_locale_set_current (const gchar *locale_file,
                                       const gchar *locale)
{
	GError *error = NULL;
	gchar  *str;

	g_message ("  Creating locale file '%s'", locale_file);

	str = g_strdup_printf ("%s", locale ? locale : "");

	if (!g_file_set_contents (locale_file, str, -1, &error)) {
		g_message ("  Could not set file contents, %s",
		           error ? error->message : "no error given");
		g_clear_error (&error);
	}

	g_free (str);
}

gboolean
tracker_miner_applications_locale_changed (void)
{
	gchar *previous_locale;
	gchar *current_locale;
	gboolean changed;
	gchar *data_dir;
	gchar *filename;

	/* Locate previous locale file */
	data_dir = g_build_filename (g_get_user_cache_dir (),
	                             "tracker",
	                             NULL);
	filename = g_build_filename (data_dir, TRACKER_MINER_APPLICATIONS_LOCALE_FILE, NULL);

	/* Get current tracker LANG locale */
	current_locale = tracker_locale_get (TRACKER_LOCALE_LANGUAGE);

#ifdef HAVE_MEEGOTOUCH
	/* If we have meegotouch enabled, sanity check to compare with our
	 * tracker locale */
	{
		/* Get also current meegotouch locale, which should be EQUAL to
		 * the tracker locale */
		gchar *current_mlocale;

		current_mlocale = tracker_miner_applications_meego_get_locale ();

		if (g_strcmp0 (current_locale, current_mlocale) != 0) {
			g_critical ("Wrong locale settings (tracker locale '%s' vs MLocale '%s')",
			            current_locale, current_mlocale);
		}

		g_free (current_mlocale);
	}
#endif

	/* Get previous locale */
	previous_locale = miner_applications_locale_get_previous (filename);

	/* Note that having both to NULL is actually valid, they would default
	 * to the unicode collation without locale-specific stuff. */
	if (g_strcmp0 (previous_locale, current_locale) != 0) {
		g_message ("Locale change detected from '%s' to '%s'...",
		           previous_locale, current_locale);
		/* Store the new one now */
		miner_applications_locale_set_current (filename, current_locale);
		changed = TRUE;
	} else {
		g_message ("Current and previous locales match: '%s'", previous_locale);
		changed = FALSE;
	}

	g_free (previous_locale);
	g_free (current_locale);
	g_free (filename);
	g_free (data_dir);
	return changed;
}
