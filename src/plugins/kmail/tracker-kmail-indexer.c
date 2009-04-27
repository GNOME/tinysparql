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

#include <libtracker-common/tracker-ontology.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>

/* This is okay, we run in-process of the indexer: we can access its symbols */
#include <tracker-indexer/tracker-module.h>
#include <tracker-indexer/tracker-push.h>
#include <tracker-indexer/tracker-module-metadata-private.h>

#define TRANSACTION_MAX 200

#include "tracker-kmail-indexer.h"

void tracker_push_module_init (TrackerConfig *config);
void tracker_push_module_shutdown (void);

/* These defines/renames are necessary for -glue.h */
#define tracker_kmail_registrar_set tracker_kmail_indexer_set
#define tracker_kmail_registrar_set_many tracker_kmail_indexer_set_many
#define tracker_kmail_registrar_unset_many tracker_kmail_indexer_unset_many
#define tracker_kmail_registrar_unset tracker_kmail_indexer_unset
#define tracker_kmail_registrar_cleanup tracker_kmail_indexer_cleanup
#define dbus_glib_tracker_kmail_indexer_object_info dbus_glib_tracker_kmail_registrar_object_info

#include "tracker-kmail-registrar-glue.h"

/* Based on data/services/email.metadata */


#define RDF_PREFIX	TRACKER_RDF_PREFIX
#define NMO_PREFIX	TRACKER_NMO_PREFIX
#define NCO_PREFIX	TRACKER_NCO_PREFIX
#define NAO_PREFIX	TRACKER_NAO_PREFIX

#define NIE_DATASOURCE 			       TRACKER_NIE_PREFIX "DataSource"
#define NIE_DATASOURCE_P 		       TRACKER_NIE_PREFIX "dataSource"

#define DATASOURCE_URN			       "urn:nepomuk:datasource:4a157cf0-1241-11de-8c30-0800200c9a66"

G_DEFINE_TYPE (TrackerKMailIndexer, tracker_kmail_indexer, G_TYPE_OBJECT)

/* This runs in-process of tracker-indexer */

static GObject *idx_indexer = NULL;


enum {
	PROP_0,
};

void tracker_push_module_init (TrackerConfig *config);
void tracker_push_module_shutdown (void);

static void
tracker_kmail_indexer_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_kmail_indexer_parent_class)->finalize (object);
}

static void
tracker_kmail_indexer_set_property (GObject      *object,
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
tracker_kmail_indexer_get_property (GObject    *object,
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
tracker_kmail_indexer_class_init (TrackerKMailIndexerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_kmail_indexer_finalize;
	object_class->set_property = tracker_kmail_indexer_set_property;
	object_class->get_property = tracker_kmail_indexer_get_property;
}

static void
tracker_kmail_indexer_init (TrackerKMailIndexer *object)
{
}



static void
get_email_and_fullname (const gchar *line, gchar **email, gchar **fullname)
{
	gchar *ptr = g_utf8_strchr (line, -1, '<');

	if (ptr) {
		gchar *holder;

		holder = g_strdup (line);
		ptr = g_utf8_strchr (holder, -1, '<');
		*ptr = '\0';
		ptr++;
		*fullname = holder;
		holder = ptr;
		ptr = g_utf8_strchr (ptr, -1, '>');
		if (ptr) {
			*ptr = '\0';
		}
		*email = g_strdup (holder);

	} else {
		*email = g_strdup (line);
		*fullname = NULL;
	}
}


static void
perform_set (TrackerKMailIndexer *object, 
	     const gchar *subject, 
	     const GStrv predicates, 
	     const GStrv values)
{
	guint i = 0;

	if (!tracker_data_query_resource_exists (DATASOURCE_URN, NULL)) {
		tracker_data_insert_statement (DATASOURCE_URN, RDF_PREFIX "type",
					       NIE_DATASOURCE);
	}


	tracker_data_insert_statement (subject, RDF_PREFIX "type",
		                       NMO_PREFIX "Email");

	tracker_data_insert_statement (subject, RDF_PREFIX "type",
		                       NMO_PREFIX "MailboxDataObject");

	tracker_data_insert_statement (subject, NIE_DATASOURCE_P,
		                       DATASOURCE_URN);

	while (predicates [i] != NULL && values[i] != NULL) {

		/* TODO: TRACKER_KMAIL_PREDICATE_IDMD5
		 *       TRACKER_KMAIL_PREDICATE_UID
		 *       TRACKER_KMAIL_PREDICATE_SERNUM
		 *       TRACKER_KMAIL_PREDICATE_SPAM
		 *       TRACKER_KMAIL_PREDICATE_HAM
		 *
		 * I don't have predicates in Tracker's ontology for these. In
		 * Jürg's vstore branch we are working with Nepomuk as ontology-
		 * set. Perhaps when we merge this to that branch that we can 
		 * improve this situation. */

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_TAG) == 0) {

			tracker_data_insert_statement (":1", RDF_PREFIX "type",
			                               NAO_PREFIX "Tag");

			tracker_data_insert_statement (":1", 
			                               NAO_PREFIX "prefLabel",
			                               values[i]);

			tracker_data_insert_statement (subject, 
			                               NAO_PREFIX "hasTag", 
			                                ":1");
		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_SUBJECT) == 0) {
			tracker_data_insert_statement (subject,
						       TRACKER_NMO_PREFIX "messageSubject", 
						       values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_SENT) == 0) {
			tracker_data_insert_statement (subject,
						       TRACKER_NMO_PREFIX "receivedDate", 
						       values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_FROM) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;
			tracker_data_insert_statement (":1", RDF_PREFIX "type", NCO_PREFIX "Contact");
			get_email_and_fullname (values[i], &email, &fullname);
			if (fullname) {
				tracker_data_insert_statement (":1", NCO_PREFIX "fullname", fullname);
				g_free (fullname);
			}
			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 
			tracker_data_insert_statement (email_uri, RDF_PREFIX "type", NCO_PREFIX "EmailAddress");
			tracker_data_insert_statement (email_uri, NCO_PREFIX "emailAddress", email);
			tracker_data_insert_statement (":1", NCO_PREFIX "hasEmailAddress", email_uri);
			tracker_data_insert_statement (subject, NMO_PREFIX "from", ":1");
			g_free (email_uri);
			g_free (email);
		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_TO) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;
			tracker_data_insert_statement (":1", RDF_PREFIX "type", NCO_PREFIX "Contact");
			get_email_and_fullname (values[i], &email, &fullname);
			if (fullname) {
				tracker_data_insert_statement (":1", NCO_PREFIX "fullname", fullname);
				g_free (fullname);
			}
			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 
			tracker_data_insert_statement (email_uri, RDF_PREFIX "type", NCO_PREFIX "EmailAddress");
			tracker_data_insert_statement (email_uri, NCO_PREFIX "emailAddress", email);
			tracker_data_insert_statement (":1", NCO_PREFIX "hasEmailAddress", email_uri);
			tracker_data_insert_statement (subject, NMO_PREFIX "to", ":1");
			g_free (email_uri);
			g_free (email);
		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_CC) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;
			tracker_data_insert_statement (":1", RDF_PREFIX "type", NCO_PREFIX "Contact");
			get_email_and_fullname (values[i], &email, &fullname);
			if (fullname) {
				tracker_data_insert_statement (":1", NCO_PREFIX "fullname", fullname);
				g_free (fullname);
			}
			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 
			tracker_data_insert_statement (email_uri, RDF_PREFIX "type", NCO_PREFIX "EmailAddress");
			tracker_data_insert_statement (email_uri, NCO_PREFIX "emailAddress", email);
			tracker_data_insert_statement (":1", NCO_PREFIX "hasEmailAddress", email_uri);
			tracker_data_insert_statement (subject, NMO_PREFIX "cc", ":1");
			g_free (email_uri);
			g_free (email);
		}


		i++;
	}

}

static void 
perform_unset (TrackerKMailIndexer *object, 
	       const gchar *subject)
{
	tracker_data_delete_resource (subject); 
}

static void
perform_cleanup (TrackerKMailIndexer *object)
{
	GError *error = NULL;

	tracker_data_update_sparql ("DELETE { ?s ?p ?o } WHERE { ?s nie:dataSource <" DATASOURCE_URN "> }", &error);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
set_stored_last_modseq (guint last_modseq)
{
	tracker_data_manager_set_db_option_int ("KMailLastModseq", (gint) last_modseq);
}

void
tracker_kmail_indexer_set (TrackerKMailIndexer *object, 
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
tracker_kmail_indexer_set_many (TrackerKMailIndexer *object, 
				const GStrv subjects, 
				const GPtrArray *predicates,
				const GPtrArray *values,
				const guint modseq,
				DBusGMethodInvocation *context,
				GError *derror)
{
	guint len;
	guint i = 0, amount = 0;

	dbus_async_return_if_fail (subjects != NULL, context);
	dbus_async_return_if_fail (predicates != NULL, context);
	dbus_async_return_if_fail (values != NULL, context);

	len = g_strv_length (subjects);

	dbus_async_return_if_fail (len == predicates->len, context);
	dbus_async_return_if_fail (len == values->len, context);

	tracker_data_begin_transaction ();

	while (subjects[i] != NULL) {
		GStrv preds = g_ptr_array_index (predicates, i);
		GStrv vals = g_ptr_array_index (values, i);

		perform_set (object, subjects[i], preds, vals);

		amount++;
		if (amount > TRANSACTION_MAX) {
			tracker_data_commit_transaction ();
			g_main_context_iteration (NULL, FALSE);
			tracker_data_begin_transaction ();
			amount = 0;
		}

		i++;
	}

	set_stored_last_modseq (modseq);

	tracker_data_commit_transaction ();

	dbus_g_method_return (context);
}

void
tracker_kmail_indexer_unset_many (TrackerKMailIndexer *object, 
				  const GStrv subjects, 
				  const guint modseq,
				  DBusGMethodInvocation *context,
				  GError *derror)
{
	guint i = 0, amount = 0;

	dbus_async_return_if_fail (subjects != NULL, context);

	tracker_data_begin_transaction ();

	while (subjects[i] != NULL) {

		perform_unset (object, subjects[i]);

		amount++;
		if (amount > TRANSACTION_MAX) {
			tracker_data_commit_transaction ();
			g_main_context_iteration (NULL, FALSE);
			tracker_data_begin_transaction ();
			amount = 0;
		}

		i++;
	}

	set_stored_last_modseq (modseq);

	tracker_data_commit_transaction ();

	dbus_g_method_return (context);
}

void
tracker_kmail_indexer_unset (TrackerKMailIndexer *object, 
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
tracker_kmail_indexer_cleanup (TrackerKMailIndexer *object, 
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
		idx_indexer = g_object_new (TRACKER_TYPE_KMAIL_INDEXER, NULL);

		dbus_g_object_type_install_info (G_OBJECT_TYPE (idx_indexer), 
						 &dbus_glib_tracker_kmail_indexer_object_info);

		dbus_g_connection_register_g_object (connection, 
						     TRACKER_KMAIL_INDEXER_PATH, 
						     idx_indexer);
	}

	if (error) {
		g_critical ("Can't init DBus for KMail support: %s", error->message);
		g_error_free (error);
	}
}

void
tracker_push_module_shutdown (void)
{
	if (idx_indexer)
		g_object_unref (idx_indexer);
}
