/*
 * Copyright (C) 2010, Nokia (ivan.frade@nokia.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <QApplication>
#include <MLocale>

#include <glib-object.h>

#include "tracker-miner-meego.h"

static QCoreApplication *app = NULL;
/*
 * MeeGo application functions...
 */
void
tracker_miner_applications_meego_init (void)
{
	char *argv[] = { "dummy", NULL };
	int argc = 1;

	/* We need the app for loading translations */
	app = new QApplication (argc, argv, FALSE);
}

void
tracker_miner_applications_meego_shutdown (void)
{
	delete app;
}

/* The meego desktop files are using the qt translation system to get
 * localized strings using catalog and string ids. QApplication and
 * MLocale are needed for loading the translation catalogs. The
 * returned string is a multi-string one which has parts of different
 * length separated by '\x9C' unicode escape sequences.
 *
 * FIXME: This is insane, try to get rid of at least some of the extra
 * layers here.
 */
gchar *
tracker_miner_applications_meego_translate (const gchar  *catalogue,
                                            const gchar  *id)
{
	/* Get the system default locale */
	MLocale locale;

	/* Load the catalog from disk if not already there */
	if(!locale.isInstalledTrCatalog (catalogue)) {
		locale.installTrCatalog (catalogue);
		MLocale::setDefault (locale);
	}

	gchar *ret = g_strdup (qtTrId (id).toUtf8 ().data ());

	/* We only want the first string of the multi-string, so if
	 * the separator character is found (encoded as C2:9C in UTF-8),
	 * we just end the string in that point */
	gchar *next_string = strstr (ret, "\xC2\x9C");
	if (next_string) {
		*next_string = '\0';
	}

	return ret;
}

/*
 * MeeGo general functions...
 */
gchar *
tracker_miner_meego_get_locale (void)
{
	/* Get the system default locale */
	MLocale locale;

	return g_strdup (locale.name ().toAscii ().data ());
}
