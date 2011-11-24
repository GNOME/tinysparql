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

#include "tracker-miner-locale.h"
#ifdef HAVE_MEEGOTOUCH
#include "tracker-miner-applications-meego.h"
#endif

#define TRACKER_MINER_APPLICATIONS_LOCALE_FILE "miner-applications-locale.txt"

static gchar *
miner_applications_locale_get_filename (void)
{
	gchar *data_dir;
	gchar *filename;

	/* Locate locale file */
	data_dir = g_build_filename (g_get_user_cache_dir (),
	                             "tracker",
	                             NULL);
	filename = g_build_filename (data_dir, TRACKER_MINER_APPLICATIONS_LOCALE_FILE, NULL);

	g_free (data_dir);

	return filename;
}

static gchar *
miner_applications_locale_get_previous (void)
{
	gchar *locale_file, *locale = NULL;

	locale_file = miner_applications_locale_get_filename ();

	if (G_LIKELY (g_file_test (locale_file, G_FILE_TEST_EXISTS))) {
		gchar *contents;

		/* Check locale is correct */
		if (G_LIKELY (g_file_get_contents (locale_file, &contents, NULL, NULL))) {
			if (contents &&
			    contents[0] == '\0') {
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
		g_message ("  Could not find locale file:'%s'", locale_file);
	}

	g_free (locale_file);

	return locale;
}

static gchar *
miner_applications_locale_get_current (void)
{
	gchar *current_locale;

#ifdef HAVE_MEEGOTOUCH
	/* If we have meegotouch enabled, take the correct locale as the one from
	 * meegotouch. */
	current_locale = tracker_miner_applications_meego_get_locale ();
#else
	/* Get current tracker LANG locale */
	current_locale = tracker_locale_get (TRACKER_LOCALE_LANGUAGE);
#endif

	return current_locale;
}

void
tracker_miner_applications_locale_set_current (void)
{
	GError *error = NULL;
	gchar *locale_file, *locale = NULL;

	locale_file = miner_applications_locale_get_filename ();

	g_message ("  Creating locale file '%s'", locale_file);

	locale = miner_applications_locale_get_current ();

	if (locale == NULL) {
		locale = g_strdup ("");
	}

	if (!g_file_set_contents (locale_file, locale, -1, &error)) {
		g_message ("  Could not set file contents, %s",
		           error ? error->message : "no error given");
		g_clear_error (&error);
	}

	g_free (locale);
	g_free (locale_file);
}

gboolean
tracker_miner_applications_locale_changed (void)
{
	gchar *previous_locale;
	gchar *current_locale;
	gboolean changed;

	current_locale = miner_applications_locale_get_current ();

	/* Get previous locale */
	previous_locale = miner_applications_locale_get_previous ();

	/* Note that having both to NULL is actually valid, they would default
	 * to the unicode collation without locale-specific stuff. */
	if (g_strcmp0 (previous_locale, current_locale) != 0) {
		g_message ("Locale change detected from '%s' to '%s'...",
		           previous_locale, current_locale);
		changed = TRUE;
	} else {
		g_message ("Current and previous locales match: '%s'", previous_locale);
		changed = FALSE;
	}

	g_free (previous_locale);
	g_free (current_locale);
	return changed;
}
