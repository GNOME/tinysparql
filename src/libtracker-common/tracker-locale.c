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
#include <gconf/gconf-client.h>
#endif /* HAVE_MAEMO */

#ifdef HAVE_MAEMO
/* Mutex to sync access to the current locale values */
static GStaticMutex locales_mutex = G_STATIC_MUTEX_INIT;
static GStaticMutex subscribers_mutex = G_STATIC_MUTEX_INIT;
#define LOCK_LOCALES g_static_mutex_lock (&locales_mutex)
#define UNLOCK_LOCALES g_static_mutex_unlock (&locales_mutex)
#define LOCK_SUBSCRIBERS g_static_mutex_lock (&subscribers_mutex)
#define UNLOCK_SUBSCRIBERS g_static_mutex_unlock (&subscribers_mutex)
#else
/* If not using gconf locales, no need to acquire/release any lock */
#define LOCK_LOCALES
#define UNLOCK_LOCALES
#endif /* HAVE_MAEMO */

/* Current locales in use. They will be stored in heap and available throughout
 * the whole program execution, so will be reported as still reachable by Valgrind.
 */
static gchar *current_locales[TRACKER_LOCALE_LAST];

static gchar *locale_names[TRACKER_LOCALE_LAST] = {
	"TRACKER_LOCALE_LANGUAGE",
	"TRACKER_LOCALE_TIME",
	"TRACKER_LOCALE_COLLATE",
	"TRACKER_LOCALE_NUMERIC",
	"TRACKER_LOCALE_MONETARY"
};

/* Already initialized? */
static gboolean initialized;

#ifdef HAVE_MAEMO

/* If this envvar is set, even if we have meegotouch locales in gconf,
 * we'll still use envvars */
#define TRACKER_DISABLE_MEEGOTOUCH_LOCALE_ENV "TRACKER_DISABLE_MEEGOTOUCH_LOCALE"

/* Base dir for all gconf locale values */
#define MEEGOTOUCH_LOCALE_DIR "/meegotouch/i18n"

/* gconf keys for tracker locales, as defined in:
 * http://apidocs.meego.com/mtf/i18n.html
 */
static const gchar *gconf_locales[TRACKER_LOCALE_LAST] = {
	MEEGOTOUCH_LOCALE_DIR "/language",
	MEEGOTOUCH_LOCALE_DIR "/lc_time",
	MEEGOTOUCH_LOCALE_DIR "/lc_collate",
	MEEGOTOUCH_LOCALE_DIR "/lc_numeric",
	MEEGOTOUCH_LOCALE_DIR "/lc_monetary"
};

/* Structure to hold the notification data of each subscriber */
typedef struct {
	TrackerLocaleID id;
	TrackerLocaleNotifyFunc func;
	gpointer user_data;
	GFreeFunc destroy_notify;
} TrackerLocaleNotification;

/* List of subscribers which want to get notified of locale changes */
static GSList *subscribers;

/* The gconf client, which will be created once at initialization and
 * the reference will never be destroyed, so will be reported as still
 * reachable by Valgrind */
static GConfClient *client;


static void
tracker_locale_set (TrackerLocaleID  id,
                    const gchar     *value)
{
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
}

static void
tracker_locale_gconf_notify_cb (GConfClient *client,
                                guint cnxn_id,
                                GConfEntry *entry,
                                gpointer user_data)
{
	guint i;
	GConfValue *value;
	GSList *li;
	const gchar *string_value;

	/* Find the proper locale to change */
	for (i = 0; i < TRACKER_LOCALE_LAST; i++) {
		if (strcmp (gconf_locales[i], gconf_entry_get_key (entry)) == 0) {
			break;
		}
	}

	/* Oh, not found? */
	if (i == TRACKER_LOCALE_LAST) {
		g_debug ("Skipping change on gconf key '%s' as not really needed",
		         gconf_entry_get_key (entry));
		return;
	}

	/* Ensure a proper value was set */
	value = gconf_entry_get_value (entry);
	if (!value) {
		g_warning ("Locale value for '%s' cannot be NULL, not changing %s",
		           gconf_locales[i],
		           locale_names[i]);
		return;
	}

	/* It must be a string */
	if (value->type != GCONF_VALUE_STRING) {
		g_warning ("Locale value for '%s' must be a string, not changing %s",
		           gconf_locales[i],
		           locale_names[i]);
		return;
	}

	string_value = gconf_value_get_string (value);

	/* String must have a length > 0 */
	if (!string_value ||
	    strlen (string_value) == 0) {
		g_warning ("Locale value for '%s' must not be empty, not changing %s",
		           gconf_locales[i],
		           locale_names[i]);
		return;
	}

	/* Protect the locale change with the lock */
	LOCK_LOCALES;
	tracker_locale_set (i, gconf_value_get_string (value));
	UNLOCK_LOCALES;

	/* Now, if any subscriber, notify the locale change.
	 * NOTE!!!! The callback MUST NOT perform any action
	 * that may change the list of subscribers, or the
	 * program will get locked. */
	LOCK_SUBSCRIBERS;
	for (li = subscribers; li; li = g_slist_next (li)) {
		TrackerLocaleNotification *data = li->data;

		if (i == data->id) {
			g_debug ("Notifying locale '%s' change to subscriber '%p'",
			         locale_names[i],
			         data);
			data->func (i, data->user_data);
		}
	}
	UNLOCK_SUBSCRIBERS;
}

#endif /* HAVE_MAEMO */

static void
tracker_locale_init (void)
{
	guint i;

#ifdef HAVE_MAEMO
	if (g_getenv (TRACKER_DISABLE_MEEGOTOUCH_LOCALE_ENV)) {
		g_message ("Retrieving locale from GConf is DISABLED");
	} else {
		GError *error = NULL;

		g_message ("Retrieving locale from GConf is ENABLED");

		/* Get default gconf client to query the locale values */
		client = gconf_client_get_default ();

		/* We want to be notified when locales are changed in gconf */
		gconf_client_add_dir (client,
		                      MEEGOTOUCH_LOCALE_DIR,
		                      GCONF_CLIENT_PRELOAD_ONELEVEL,
		                      &error);
		if (error) {
			g_warning ("Cannot add dir '%s' in gconf client: '%s'",
			           MEEGOTOUCH_LOCALE_DIR,
			           error->message);
			g_clear_error (&error);
		} else {
			/* Request notifications */
			gconf_client_notify_add (client,
			                         MEEGOTOUCH_LOCALE_DIR,
			                         tracker_locale_gconf_notify_cb,
			                         NULL,
			                         NULL,
			                         &error);
			if (error) {
				g_warning ("Cannot request notifications under dir '%s' in "
				           "gconf client: '%s'",
				           MEEGOTOUCH_LOCALE_DIR,
				           error->message);
				g_clear_error (&error);
			}
		}

		/* And initialize all, should have been all preloaded in the
		 * client already */
		for (i = 0; i < TRACKER_LOCALE_LAST; i++) {
			GConfValue *val;

			/* Get the gconf key, if any */
			val = gconf_client_get (client,
			                        gconf_locales[i],
			                        &error);
			if (!val) {
				g_warning ("Couldn't get value for key '%s'"
				           " from gconf: '%s'"
				           " Defaulting to environment locale.",
				           gconf_locales[i],
				           error ? error->message : "no such key");
				g_clear_error (&error);
			} else if (val->type != GCONF_VALUE_STRING) {
				g_warning ("Wrong type for '%s' key in gconf..."
				           " Defaulting to environment locale.",
				           gconf_locales[i]);
				gconf_value_free (val);
			} else {
				/* Set the new locale */
				tracker_locale_set (i, gconf_value_get_string (val));
				gconf_value_free (val);
			}
		}
	}
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

	/* Lock even before checking if initialized, so that initialization is
	 * not done twice */
	LOCK_LOCALES;

	/* Initialize if not already done */
	if (!initialized)
		tracker_locale_init ();

	/* Always return a duplicated string, as the locale may change at any
	 * moment */
	locale = g_strdup (current_locales[id]);

	UNLOCK_LOCALES;

	return locale;
}

gpointer
tracker_locale_notify_add (TrackerLocaleID         id,
                           TrackerLocaleNotifyFunc func,
                           gpointer                user_data,
                           GFreeFunc               destroy_notify)
{
#ifdef HAVE_MAEMO
	TrackerLocaleNotification *data;

	g_assert (func);

	data = g_slice_new (TrackerLocaleNotification);
	data->id = id;
	data->func = func;
	data->user_data = user_data;
	data->destroy_notify = destroy_notify;

	/* Lock list, we cannot! use the same lock as for locales here... */
	LOCK_SUBSCRIBERS;
	subscribers = g_slist_prepend (subscribers, data);
	UNLOCK_SUBSCRIBERS;

	return data;
#else
	/* If not using gconf locales, this is a no-op... */
	return NULL;
#endif /* HAVE_MAEMO */
}

void
tracker_locale_notify_remove (gpointer notification_id)
{
#ifdef HAVE_MAEMO
	GSList *li;

	LOCK_SUBSCRIBERS;
	li = g_slist_find (subscribers, notification_id);
	if (li) {
		TrackerLocaleNotification *data = li->data;

		/* Remove item from list of subscribers */
		subscribers = g_slist_delete_link (subscribers, li);

		/* Call the provided destroy_notify if any. */
		if (data->destroy_notify)
			data->destroy_notify (data->user_data);
		/* And fully dispose the notification data */
		g_slice_free (TrackerLocaleNotification, data);
	}
	UNLOCK_SUBSCRIBERS;
#else
	/* If not using gconf locales, this is a no-op... */
#endif /* HAVE_MAEMO */
}
