/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-dbus.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-manager.h>

#include <libtracker-data/tracker-data.h>

#include "tracker-dbus.h"
#include "tracker-marshal.h"
#include "tracker-store.h"
#include "tracker-statistics.h"

#define TRACKER_STATISTICS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_STATISTICS, TrackerStatisticsPrivate))

G_DEFINE_TYPE(TrackerStatistics, tracker_statistics, G_TYPE_OBJECT)

static void
tracker_statistics_class_init (TrackerStatisticsClass *klass)
{
}

static void
tracker_statistics_init (TrackerStatistics *object)
{
}

TrackerStatistics *
tracker_statistics_new (void)
{
	return g_object_new (TRACKER_TYPE_STATISTICS, NULL);
}

static gint
cache_sort_func (gconstpointer a,
                 gconstpointer b)
{
	const GStrv *strv_a = (GStrv *) a;
	const GStrv *strv_b = (GStrv *) b;

	g_return_val_if_fail (strv_a != NULL, 0);
	g_return_val_if_fail (strv_b != NULL, 0);

	return g_strcmp0 (*strv_a[0], *strv_b[0]);
}

void
tracker_statistics_get (TrackerStatistics      *object,
                        DBusGMethodInvocation  *context,
                        GError                **error)
{
	TrackerClass **classes, *cl;
	guint                     request_id;
	GPtrArray                *values;
	guint                     i, n_classes;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_block_hooks ();
	tracker_dbus_request_new (request_id, context, "%s()", __FUNCTION__);

	values = g_ptr_array_new ();

	classes = tracker_ontologies_get_classes (&n_classes);

	for (i = 0; i < n_classes; i++) {
		GStrv        strv;

		cl = classes[i];

		if (tracker_class_get_count (cl) == 0) {
			/* skip classes without resources */
			continue;
		}

		strv = g_new (gchar*, 3);
		strv[0] = g_strdup (tracker_class_get_name (cl));
		strv[1] = g_strdup_printf ("%d", tracker_class_get_count (cl));
		strv[2] = NULL;

		g_ptr_array_add (values, strv);
	}

	/* Sort result so it is alphabetical */
	g_ptr_array_sort (values, cache_sort_func);

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context, values);

	g_ptr_array_foreach (values, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (values, TRUE);

	tracker_dbus_request_unblock_hooks ();
}
