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

#ifdef HAVE_MAEMO
#include "tracker-locale-gconfdbus.h"
#endif /* HAVE_MAEMO */

/* Current locales in use. They will be stored in heap and available throughout
 * the whole program execution, so will be reported as still reachable by Valgrind.
 */
static gchar *current_locales[TRACKER_LOCALE_LAST];

static const gchar *locale_names[TRACKER_LOCALE_LAST] = {
	"TRACKER_LOCALE_LANGUAGE",
	"TRACKER_LOCALE_TIME",
	"TRACKER_LOCALE_COLLATE",
	"TRACKER_LOCALE_NUMERIC",
	"TRACKER_LOCALE_MONETARY"
};

/* Already initialized? */
static gboolean initialized;
static GStaticRecMutex locales_mutex = G_STATIC_REC_MUTEX_INIT;

const gchar*
tracker_locale_get_name (guint i)
{
	g_return_val_if_fail (i < TRACKER_LOCALE_LAST, NULL);
	return locale_names[i];
}

void
tracker_locale_set (TrackerLocaleID  id,
                    const gchar     *value)
{
	g_static_rec_mutex_lock (&locales_mutex);

	if (current_locales[id]) {
		g_debug ("Locale '%s' was changed from '%s' to '%s'",
		         locale_names[id],
		         current_locales[id],
		         value);
		g_free (current_locales[id]);
	} else {
		g_debug ("Locale '%s' was set to '%s'",
		         locale_names[id],
		         value);
	}

	/* Store the new one */
	current_locales[id] = g_strdup (value);

	/* And also set the new one in the corresponding envvar */
	switch (id) {
	case TRACKER_LOCALE_LANGUAGE:
		g_setenv ("LANG", value, TRUE);
		break;
	case TRACKER_LOCALE_TIME:
		setlocale (LC_TIME, value);
		break;
	case TRACKER_LOCALE_COLLATE:
		setlocale (LC_COLLATE, value);
		break;
	case TRACKER_LOCALE_NUMERIC:
		setlocale (LC_NUMERIC, value);
		break;
	case TRACKER_LOCALE_MONETARY:
		setlocale (LC_MONETARY, value);
		break;
	case TRACKER_LOCALE_LAST:
		/* Make compiler happy */
		g_warn_if_reached ();
		break;
	}

	g_static_rec_mutex_unlock (&locales_mutex);
}


static void
locale_init (void)
{
	guint i;

#ifdef HAVE_MAEMO
	tracker_locale_gconfdbus_init ();
#endif /* HAVE_MAEMO */

	/* Initialize those not retrieved from gconf, or if not in maemo */
	for (i = 0; i < TRACKER_LOCALE_LAST; i++) {
		if (!current_locales[i]) {
			const gchar *env_locale;

			switch (i) {
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
			case TRACKER_LOCALE_LAST:
				/* Make compiler happy. The for loop avoids
				 * this from happening. */
				break;
			}

			if (!env_locale) {
				g_warning ("Locale '%d' is not set, defaulting to C locale", i);
				tracker_locale_set (i, "C");
			} else {
				tracker_locale_set (i, env_locale);
			}
		}
	}

	/* So we're initialized */
	initialized = TRUE;
}

gchar *
tracker_locale_get (TrackerLocaleID id)
{
	gchar *locale;

	g_static_rec_mutex_lock (&locales_mutex);

	/* Initialize if not already done */
	if (!initialized) {
		locale_init ();
	}

	/* Always return a duplicated string, as the locale may change at any
	 * moment */
	locale = g_strdup (current_locales[id]);

	g_static_rec_mutex_unlock (&locales_mutex);

	return locale;
}

gpointer
tracker_locale_notify_add (TrackerLocaleID         id,
                           TrackerLocaleNotifyFunc func,
                           gpointer                user_data,
                           GFreeFunc               destroy_notify)
{
	return NULL;
}

void
tracker_locale_notify_remove (gpointer notification_id)
{
}
