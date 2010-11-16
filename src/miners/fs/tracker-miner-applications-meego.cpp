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

#include "tracker-miner-applications-meego.h"

/* The meego desktop files are using the qt translation system to get
 * localized strings using catalog and string ids. QApplication and
 * MLocale are needed for loading the translation catalogs. The
 * returned string is a multi-string one which has parts of different
 * lenght separated by '\x9C' unicode escape sequences.
 *
 * FIXME: This is insane, try to get rid of at least some of the extra
 * layers here.
 */
gchar *
tracker_miner_applications_meego_translate (const gchar *catalogue,
					    const gchar *id)
{
	char *argv[] = { "dummy", NULL };
	int argc = 1;

	/* We need the app for loading translations */
	QApplication app (argc, argv, FALSE);

	/* Get the system default locale */
	MLocale locale;

	/* Load the catalog from disk */
	locale.installTrCatalog (catalogue);
	MLocale::setDefault (locale);

	GStrv split;
	split = g_strsplit (qtTrId (id). toUtf8 ().data (), "\x9C", 2);

	gchar *ret = g_strdup (split[0]);
	g_strfreev (split);

	return ret;
}
