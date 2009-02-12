/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <stdio.h>

#include <gmime/gmime.h>

#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data-manager.h>

/* This is okay, we run in-process of the indexer: we can access its symbols */
#include <tracker-indexer/tracker-module.h>
#include <tracker-indexer/tracker-push.h>
#include <tracker-indexer/tracker-module-metadata-private.h>

#include "tracker-rss-indexer.h"

/* These defines/renames are necessary for -glue.h */
#define tracker_rss_registrar_set tracker_rss_indexer_set
#define tracker_rss_registrar_set_many tracker_rss_indexer_set_many
#define tracker_rss_registrar_unset_many tracker_rss_indexer_unset_many
#define tracker_rss_registrar_unset tracker_rss_indexer_unset
#define tracker_rss_registrar_cleanup tracker_rss_indexer_cleanup
#define dbus_glib_tracker_rss_indexer_object_info dbus_glib_tracker_rss_registrar_object_info

#include "tracker-rss-registrar-glue.h"

/* Based on data/services/email.metadata */

G_DEFINE_TYPE (TrackerRssIndexer, tracker_rss_indexer, G_TYPE_OBJECT)

/* This runs in-process of tracker-indexer */

static GObject *idx_indexer = NULL;

enum {
	PROP_0,
};

static void
tracker_rss_indexer_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_rss_indexer_parent_class)->finalize (object);
}

static void
tracker_rss_indexer_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_rss_indexer_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_rss_indexer_class_init (TrackerRssIndexerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_rss_indexer_finalize;
	object_class->set_property = tracker_rss_indexer_set_property;
	object_class->get_property = tracker_rss_indexer_get_property;
}

static void
tracker_rss_indexer_init (TrackerRssIndexer *object)
{
}


static void
perform_set (TrackerRssIndexer *object, 
	     const gchar *subject, 
	     const GStrv predicates, 
	     const GStrv values)
{
	guint i = 0;
	TrackerModuleMetadata *metadata;
	GHashTable *data;

	metadata = tracker_module_metadata_new ();

	while (predicates [i] != NULL && values[i] != NULL) {

		if (g_strcmp0 (predicates[i], TRACKER_RSS_PREDICATE_THING) == 0) {
			tracker_module_metadata_add_string (metadata, 
							    "Rss:Something", 
							    values[i]);
		}

		i++;
	}

	data = tracker_module_metadata_get_hash_table (metadata);

	// TODO: the service for RSS
	tracker_data_update_replace_service (subject, "Rss", data);

	g_hash_table_destroy (data);
	g_object_unref (metadata);
}

static void 
perform_unset (TrackerRssIndexer *object, 
	       const gchar *subject)
{
	// TODO: the service for RSS
	tracker_data_update_delete_service_by_path (subject, "Rss"); 
}

static void
perform_cleanup (TrackerRssIndexer *object)
{
	// TODO: the service for RSS
	tracker_data_update_delete_service_all ("Rss");
}

static void
set_stored_last_modseq (guint last_modseq)
{
	tracker_data_manager_set_db_option_int ("RssLastModseq", (gint) last_modseq);
}

void
tracker_rss_indexer_set (TrackerRssIndexer *object, 
			 const gchar *subject, 
			 const GStrv predicates,
			 const GStrv values,
			 const guint modseq,
			 DBusGMethodInvocation *context,
			 GError *derror)
{
	dbus_async_return_if_fail (subject != NULL, context);

	if (predicates && values) {

		dbus_async_return_if_fail (g_strv_length (predicates) == 
					   g_strv_length (values), context);

		perform_set (object, subject, predicates, values);
	}

	set_stored_last_modseq (modseq);

	dbus_g_method_return (context);
}

void
tracker_erssindexer_set_many (TrackerRssIndexer *object, 
			      const GStrv subjects, 
			      const GPtrArray *predicates,
			      const GPtrArray *values,
			      const guint modseq,
			      DBusGMethodInvocation *context,
			      GError *derror)
{
	guint len;
	guint i = 0;

	dbus_async_return_if_fail (subjects != NULL, context);
	dbus_async_return_if_fail (predicates != NULL, context);
	dbus_async_return_if_fail (values != NULL, context);

	len = g_strv_length (subjects);

	dbus_async_return_if_fail (len == predicates->len, context);
	dbus_async_return_if_fail (len == values->len, context);

	while (subjects[i] != NULL) {
		GStrv preds = g_ptr_array_index (predicates, i);
		GStrv vals = g_ptr_array_index (values, i);

		perform_set (object, subjects[i], preds, vals);

		i++;
	}

	set_stored_last_modseq (modseq);

	dbus_g_method_return (context);
}

void
tracker_rss_indexer_unset_many (TrackerRssIndexer *object, 
				const GStrv subjects, 
				const guint modseq,
				DBusGMethodInvocation *context,
				GError *derror)
{
	guint i = 0;

	dbus_async_return_if_fail (subjects != NULL, context);

	while (subjects[i] != NULL) {

		perform_unset (object, subjects[i]);

		i++;
	}

	set_stored_last_modseq (modseq);

	dbus_g_method_return (context);
}

void
tracker_rss_indexer_unset (TrackerRssIndexer *object, 
			   const gchar *subject, 
			   const guint modseq,
			   DBusGMethodInvocation *context,
			   GError *derror)
{
	dbus_async_return_if_fail (subject != NULL, context);

	perform_unset (object, subject);

	dbus_g_method_return (context);
}

void
tracker_rss_indexer_cleanup (TrackerRssIndexer *object, 
			     const guint modseq,
			     DBusGMethodInvocation *context,
			     GError *derror)
{
	perform_cleanup (object);

	set_stored_last_modseq (modseq);

	dbus_g_method_return (context);
}

void
tracker_push_module_init (TrackerConfig *config)
{
	GError *error = NULL;
	DBusGConnection *connection;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!error) {
		idx_indexer = g_object_new (TRACKER_TYPE_RSS_INDEXER, NULL);

		dbus_g_object_type_install_info (G_OBJECT_TYPE (idx_indexer), 
						 &dbus_glib_tracker_rss_indexer_object_info);

		dbus_g_connection_register_g_object (connection, 
						     TRACKER_RSS_INDEXER_PATH, 
						     idx_indexer);
	}

	if (error) {
		g_critical ("Can't init DBus for Rss support: %s", error->message);
		g_error_free (error);
	}
}

void
tracker_push_module_shutdown (void)
{
	if (idx_indexer)
		g_object_unref (idx_indexer);
}
