/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <libtracker-data/tracker-data-manager.h>

#include "tracker-dbus.h"
#include "tracker-marshal.h"
#include "tracker-store.h"
#include "tracker-statistics.h"

#define TRACKER_STATISTICS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_STATISTICS, TrackerStatisticsPrivate))

/* In seconds (5 minutes for now) */
#define CACHE_LIFETIME 300

typedef struct {
	GHashTable       *cache;
	guint             cache_timeout_id;
} TrackerStatisticsPrivate;

enum {
	UPDATED,
	LAST_SIGNAL
};

static void        tracker_statistics_finalize (GObject       *object);
static gboolean    cache_timeout     (gpointer       user_data);
static GHashTable *cache_get_latest  (void);

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE(TrackerStatistics, tracker_statistics, G_TYPE_OBJECT)

static void
tracker_statistics_class_init (TrackerStatisticsClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_statistics_finalize;

	signals[UPDATED] =
		g_signal_new ("updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      TRACKER_TYPE_G_STRV_ARRAY);

	g_type_class_add_private (object_class, sizeof (TrackerStatisticsPrivate));
}

static void
tracker_statistics_init (TrackerStatistics *object)
{
	TrackerStatisticsPrivate *priv;

	priv = TRACKER_STATISTICS_GET_PRIVATE (object);

	/* Do first time stats lookup */
	priv->cache = cache_get_latest ();

	priv->cache_timeout_id = 
		g_timeout_add_seconds (CACHE_LIFETIME,
				       cache_timeout,
				       object);
}

static void
tracker_statistics_finalize (GObject *object)
{
	TrackerStatistics	     *statistics;
	TrackerStatisticsPrivate *priv;

	statistics = TRACKER_STATISTICS (object);
	priv = TRACKER_STATISTICS_GET_PRIVATE (statistics);

	if (priv->cache_timeout_id != 0) {
		g_source_remove (priv->cache_timeout_id);
	}

	if (priv->cache) {
		g_hash_table_unref (priv->cache);
	}

	G_OBJECT_CLASS (tracker_statistics_parent_class)->finalize (object);
}

TrackerStatistics *
tracker_statistics_new (void)
{
	return g_object_new (TRACKER_TYPE_STATISTICS, NULL);
}

/*
 * Functions
 */
static GHashTable *
cache_get_latest (void)
{
	GHashTable    *values;
	TrackerClass **classes;
	TrackerClass **cl;
	TrackerDBInterface *iface;

	g_message ("Requesting statistics from database for an accurate signal");

	values = g_hash_table_new_full (g_str_hash,
					g_str_equal,
					g_free,
					NULL);

	classes = tracker_ontology_get_classes ();

	iface = tracker_db_manager_get_db_interface ();

	for (cl = classes; *cl; cl++) {
		TrackerDBStatement *stmt;
		TrackerDBResultSet *result_set;
		gint count;

		if (g_str_has_prefix (tracker_class_get_name (*cl), "xsd:")) {
			/* xsd classes do not derive from rdfs:Resource and do not use separate tables */
			continue;
		}

		stmt = tracker_db_interface_create_statement (iface, "SELECT COUNT(1) FROM \"%s\"", tracker_class_get_name (*cl));
		result_set = tracker_db_statement_execute (stmt, NULL);

		tracker_db_result_set_get (result_set, 0, &count, -1);

		g_hash_table_insert (values, 
				     g_strdup (tracker_class_get_name (*cl)),
				     GINT_TO_POINTER (count));

		g_object_unref (result_set);
		g_object_unref (stmt);
	}

	return values;
}

static gboolean 
cache_timeout (gpointer user_data)
{
	g_message ("Statistics cache has expired, updating...");

	tracker_statistics_signal ();

	return TRUE;
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
			GError		      **error)
{
	TrackerClass **classes;
	TrackerClass **cl;
	TrackerStatisticsPrivate *priv;
	guint		          request_id;
	GPtrArray                *values;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_block_hooks ();
	tracker_dbus_request_new (request_id, __FUNCTION__);

	priv = TRACKER_STATISTICS_GET_PRIVATE (object);

	values = g_ptr_array_new ();

	classes = tracker_ontology_get_classes ();

	for (cl = classes; *cl; cl++) {
		GStrv        strv;

		if (tracker_class_get_count (*cl) == 0) {
			/* skip classes without resources */
			continue;
		}

		strv = g_new (gchar*, 3);
		strv[0] = g_strdup (tracker_class_get_name (*cl));
		strv[1] = g_strdup_printf ("%d", tracker_class_get_count (*cl));
		strv[2] = NULL;

		g_ptr_array_add (values, strv);
	}

	/* Sort result so it is alphabetical */
	g_ptr_array_sort (values, cache_sort_func);

	dbus_g_method_return (context, values);

	g_ptr_array_foreach (values, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (values, TRUE);

	tracker_dbus_request_success (request_id);
	tracker_dbus_request_unblock_hooks ();
}

void
tracker_statistics_signal (void)
{
	GObject		     *statistics;
	TrackerStatisticsPrivate *priv;
	GHashTable           *stats;
	GHashTableIter        iter;
	gpointer              key, value;
	GPtrArray            *values;

	statistics = tracker_dbus_get_object (TRACKER_TYPE_STATISTICS);
	priv = TRACKER_STATISTICS_GET_PRIVATE (statistics);

	/* Get latest */
	stats = cache_get_latest ();

	/* There are 3 situations here:
	 *  - 1. No new stats
	 *       Action: Do nothing
	 *  - 2. New stats and old stats
	 *       Action: Check what has changed and emit new stats
	 */

	g_message ("Checking for statistics changes and signalling clients...");

	/* Situation #1 */
	if (g_hash_table_size (stats) < 1) {
		g_hash_table_unref (stats);
		g_message ("  No new statistics, doing nothing");
		return;
	}

	/* Situation #2 */
	values = g_ptr_array_new ();

	g_hash_table_iter_init (&iter, stats);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar  *service_type;
		gpointer      data;
		gint          old_count, new_count;
		
		service_type = key;
		new_count = GPOINTER_TO_INT (value);
			
		data = g_hash_table_lookup (priv->cache, service_type);
		old_count = GPOINTER_TO_INT (data);
		
		if (old_count != new_count) {
			GStrv strv;

			g_message ("  Updating '%s' with new count:%d, old count:%d, diff:%d", 
				   service_type,
				   new_count,
				   old_count,
				   new_count - old_count);
			
			strv = g_new (gchar*, 3);
			strv[0] = g_strdup (service_type);
			strv[1] = g_strdup_printf ("%d", new_count);
			strv[2] = NULL;
			
			g_ptr_array_add (values, strv);
		}
	}

	if (values->len > 0) {
		/* Make sure we sort the results first */
		g_ptr_array_sort (values, cache_sort_func);
		
		g_signal_emit (statistics, signals[UPDATED], 0, values);
	} else {
		g_message ("  No changes in the statistics");
	}
	
	g_ptr_array_foreach (values, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (values, TRUE);

	g_hash_table_unref (priv->cache);
	priv->cache = stats;
}
