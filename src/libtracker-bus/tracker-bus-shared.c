/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2010, Codeminded BVBA <philip@codeminded.be>
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

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib-object.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-bus-shared.h"

GVariant*
tracker_bus_message_to_variant (DBusMessage *message)
{
	GVariantBuilder builder;
	DBusMessageIter iter, subiter, subsubiter;

	/* TODO: This is probably wrong, especially the hashtable part */

	/*aaa{ss}*/

	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY); /* a */

	dbus_message_iter_init (message, &iter);
	dbus_message_iter_recurse (&iter, &subiter);

	while (dbus_message_iter_get_arg_type (&subiter) != DBUS_TYPE_INVALID) {

		g_variant_builder_open (&builder, G_VARIANT_TYPE_ARRAY); /* a */

		dbus_message_iter_recurse (&subiter, &subsubiter);

		while (dbus_message_iter_get_arg_type (&subsubiter) != DBUS_TYPE_INVALID) {
			DBusMessageIter s_subiter, s_subsubiter;

			dbus_message_iter_recurse (&subsubiter, &s_subiter);

			while (dbus_message_iter_get_arg_type (&s_subiter) != DBUS_TYPE_INVALID) {
				const gchar *key, *value;

				g_variant_builder_open (&builder, G_VARIANT_TYPE_ARRAY); /* a */
				g_variant_builder_open (&builder, G_VARIANT_TYPE_DICTIONARY); /* {ss} */

				dbus_message_iter_recurse (&s_subiter, &s_subsubiter);
				dbus_message_iter_get_basic (&s_subsubiter, &key);
				dbus_message_iter_next (&s_subsubiter);
				dbus_message_iter_get_basic (&s_subsubiter, &value);

				g_variant_builder_add (&builder, "{ss}", key, value);

				dbus_message_iter_next (&s_subiter);

				g_variant_builder_close (&builder);
				g_variant_builder_close (&builder);
			}

			dbus_message_iter_next (&subsubiter);
		}

		g_variant_builder_close (&builder);

		dbus_message_iter_next (&subiter);
	}

	return g_variant_builder_end (&builder);
}
