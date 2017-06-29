/*
 * Copyright (C) 2010 Nokia <ivan.frade@nokia.com>
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

#include <locale.h>
#include <string.h>

#include <glib.h>

#include "tracker-locale.h"

static const gchar *locale_names[TRACKER_LOCALE_LAST] = {
	"LANG",
	"LC_TIME",
	"LC_COLLATE",
	"LC_NUMERIC",
	"LC_MONETARY"
};

static GRecMutex locales_mutex;

static const gchar *
tracker_locale_get_unlocked (TrackerLocaleID id)
{
	const gchar *env_locale = NULL;

	switch (id) {
	case TRACKER_LOCALE_LANGUAGE:
		env_locale = g_getenv ("LANG");
		break;
	case TRACKER_LOCALE_TIME:
		env_locale = setlocale (LC_TIME, NULL);
		break;
	case TRACKER_LOCALE_COLLATE:
		env_locale = setlocale (LC_COLLATE, NULL);
		break;
	case TRACKER_LOCALE_NUMERIC:
		env_locale = setlocale (LC_NUMERIC, NULL);
		break;
	case TRACKER_LOCALE_MONETARY:
		env_locale = setlocale (LC_MONETARY, NULL);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return env_locale;
}

void
tracker_locale_sanity_check (void)
{
	guint i;

	g_rec_mutex_lock (&locales_mutex);

	for (i = 0; i < TRACKER_LOCALE_LAST; i++) {
		const gchar *env_locale = NULL;

		env_locale = tracker_locale_get_unlocked (i);

		if (!env_locale) {
			g_warning ("Locale '%s' is not set, defaulting to C locale", locale_names[i]);
		}
	}

	g_rec_mutex_unlock (&locales_mutex);
}

gchar *
tracker_locale_get (TrackerLocaleID id)
{
	const gchar *env_locale = NULL;
	gchar *locale;

	g_rec_mutex_lock (&locales_mutex);

	env_locale = tracker_locale_get_unlocked (id);

	/* Always return a duplicated string, as the locale may change at any
	 * moment */
	locale = g_strdup (env_locale);

	g_rec_mutex_unlock (&locales_mutex);

	return locale;
}
