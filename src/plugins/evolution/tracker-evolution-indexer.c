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

#define TRANSACTION_MAX 200

#include "tracker-evolution-indexer.h"

/* These defines/renames are necessary for -glue.h */
#define tracker_evolution_registrar_set tracker_evolution_indexer_set
#define tracker_evolution_registrar_set_many tracker_evolution_indexer_set_many
#define tracker_evolution_registrar_unset_many tracker_evolution_indexer_unset_many
#define tracker_evolution_registrar_unset tracker_evolution_indexer_unset
#define tracker_evolution_registrar_cleanup tracker_evolution_indexer_cleanup
#define dbus_glib_tracker_evolution_indexer_object_info dbus_glib_tracker_evolution_registrar_object_info

#include "tracker-evolution-registrar-glue.h"

/* Based on data/services/email.metadata */

#define NIE_DATASOURCE 			       TRACKER_NIE_PREFIX "DataSource"
#define NIE_DATASOURCE_P 		       TRACKER_NIE_PREFIX "dataSource"

#define RDF_PREFIX	TRACKER_RDF_PREFIX
#define NMO_PREFIX	TRACKER_NMO_PREFIX
#define NCO_PREFIX	TRACKER_NCO_PREFIX
#define NAO_PREFIX	TRACKER_NAO_PREFIX

#define DATASOURCE_URN			       "urn:nepomuk:datasource:1cb1eb90-1241-11de-8c30-0800200c9a66"

void tracker_push_module_init (TrackerConfig *config);
void tracker_push_module_shutdown (void);

G_DEFINE_TYPE (TrackerEvolutionIndexer, tracker_evolution_indexer, G_TYPE_OBJECT)

/* This runs in-process of tracker-indexer */

static GObject *idx_indexer = NULL;

enum {
	PROP_0,
};

void tracker_push_module_init (TrackerConfig *config);
void tracker_push_module_shutdown (void);

static void
tracker_evolution_indexer_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_evolution_indexer_parent_class)->finalize (object);
}

static void
tracker_evolution_indexer_set_property (GObject      *object,
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
tracker_evolution_indexer_get_property (GObject    *object,
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
tracker_evolution_indexer_class_init (TrackerEvolutionIndexerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_evolution_indexer_finalize;
	object_class->set_property = tracker_evolution_indexer_set_property;
	object_class->get_property = tracker_evolution_indexer_get_property;
}

static void
tracker_evolution_indexer_init (TrackerEvolutionIndexer *object)
{
}

#if 0
static void
extract_mime_parts (GMimeObject *object,
		    gpointer     user_data)
{
	GMimePart *part;
	const gchar *disposition, *filename;

	if (GMIME_IS_MESSAGE_PART (object)) {
		GMimeMessage *message;

		message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (object));

		if (message) {
			g_mime_message_foreach_part (message, extract_mime_parts, user_data);
			g_object_unref (message);
		}

		return;
	} else if (GMIME_IS_MULTIPART (object)) {
		g_mime_multipart_foreach (GMIME_MULTIPART (object), extract_mime_parts, user_data);
		return;
	}

	part = GMIME_PART (object);
	disposition = g_mime_part_get_content_disposition (part);

	if (!disposition ||
	    (g_strcmp0 (disposition, GMIME_DISPOSITION_ATTACHMENT) != 0 &&
	     g_strcmp0 (disposition, GMIME_DISPOSITION_INLINE) != 0)) {
		return;
	}

	filename = g_mime_part_get_filename (GMIME_PART (object));

	if (!filename ||
	    g_strcmp0 (filename, "signature.asc") == 0 ||
	    g_strcmp0 (filename, "signature.pgp") == 0) {
		return;
	}

	if (filename) {
		GHashTable *data;
		TrackerModuleMetadata *metadata;
		gchar *subject;

		/* This is not a path but a URI: don't use the OS's dir separator
		 * here, use the '/'. Another option is to use '#' instead of '/'
		 * here. This depends on how we want to format the URI and what
		 * Evolution can cope with as URI for an attachment (I don't 
		 * think it can cope with any attachment URI, btw). */

		subject = g_strdup_printf ("%s/%s", user_data, filename);

		tracker_data_insert_statement (subject, 
					       "File:Path", 
					       filename);

		tracker_data_insert_statement (subject, 
					       "File:Name", 
					       filename);

		g_free (subject);
	}
}

static gchar *
get_object_encoding (GMimeObject *object)
{
	const gchar *start_encoding, *end_encoding;
	const gchar *content_type = NULL;

	if (GMIME_IS_MESSAGE (object)) {
		content_type = g_mime_message_get_header (GMIME_MESSAGE (object), "Content-Type");
	} else if (GMIME_IS_PART (object)) {
		content_type = g_mime_part_get_content_header (GMIME_PART (object), "Content-Type");
	}

	if (!content_type) {
		return NULL;
	}

	start_encoding = strstr (content_type, "charset=");

	if (!start_encoding) {
		return NULL;
	}

	/* Length of "charset=" */
	start_encoding += 8;

	if (start_encoding[0] == '"') {
		/* encoding is quoted */
		start_encoding++;
		end_encoding = strstr (start_encoding, "\"");
	} else {
		end_encoding = strstr (start_encoding, ";");
	}

	if (end_encoding) {
		return g_strndup (start_encoding, end_encoding - start_encoding);
	} else {
		return g_strdup (start_encoding);
	}
}
#endif


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
perform_set (TrackerEvolutionIndexer *object, 
	     const gchar             *subject, 
	     const GStrv              predicates, 
	     const GStrv              values)
{
	guint i;

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

	for (i = 0; predicates[i] && values[i]; i++) {
		/* TODO: TRACKER_EVOLUTION_PREDICATE_JUNK (!)
		 *       TRACKER_EVOLUTION_PREDICATE_ANSWERED
		 *       TRACKER_EVOLUTION_PREDICATE_FLAGGED
		 *       TRACKER_EVOLUTION_PREDICATE_FORWARDED
		 *       TRACKER_EVOLUTION_PREDICATE_DELETED (!)
		 *       TRACKER_EVOLUTION_PREDICATE_SIZE (!) :
		 *
		 * I don't have predicates in Tracker's ontology for these. In
		 * Jürg's vstore branch we are working with Nepomuk as ontology-
		 * set. Perhaps when we merge this to that branch that we can 
		 * improve this situation. */

#if 0
		/* Disabling this as I can't find any version of GMime-2.0 that
		 * wont crash on any of my test E-mails. Going to ask Garnacho
		 * to migrate to GMime-2.4 with his old Evolution support. */

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_FILE) == 0) {
			GMimeStream *stream;
			GMimeParser *parser;
			GMimeMessage *message;
			gint fd;
			gchar *text, *orig_text, *p, *encoding;
			gchar *path;
			off_t offset;
			gboolean is_html;

			path = g_strdup (values[i]);

			p = strstr (path, "/!");
			if (p) {
				offset = (off_t) atol (p + 2);
				*p = '\0';
			} else {
				offset = 0;
			}

			fd = tracker_file_open (path, FALSE);
			g_free (path);

			if (fd == -1) {
				continue;
			}

			stream = g_mime_stream_fs_new_with_bounds (fd, offset, -1);

			if (!stream) {
				tracker_file_close (fd);
				continue;
			}

			parser = g_mime_parser_new_with_stream (stream);

			if (!parser) {
				g_object_unref (stream);
				continue;
			}

			g_mime_parser_set_scan_from (parser, FALSE);

			message = g_mime_parser_construct_message (parser);

			if (!message) {
				g_object_unref (parser);
				g_object_unref (stream);
				continue;
			}

			g_mime_message_foreach_part (message,
						     extract_mime_parts,
						     subject);

			orig_text = g_mime_message_get_body (message, TRUE, &is_html);

			if (orig_text) {
				encoding = get_object_encoding (GMIME_OBJECT (message));

				if (encoding) {
					text = g_convert (text, -1, "utf8", encoding, NULL, NULL, NULL);
					g_free (orig_text);
				} else {
					text = orig_text;
				}

				tracker_data_insert_statement (subject, 
							       METADATA_EMAIL_TEXT, 
							       text);

				g_free (text);
				g_free (encoding);
			}

			g_object_unref (message);
			g_object_unref (parser);
			g_object_unref (stream);
		}
#endif

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_TAG) == 0) {
			gchar *key, *value;

			if (tracker_is_empty_string (values[i])) {
				continue;
			}

			key = g_strdup (values[i]);
			value = g_utf8_strchr (key, -1, '=');

			if (value) {
				*value = '\0';
				value++;
			}

			tracker_data_insert_statement (":1", RDF_PREFIX "type",
			                               NAO_PREFIX "Property");

			tracker_data_insert_statement (":1", 
			                               NAO_PREFIX "propertyName",
			                               key);

			tracker_data_insert_statement (":1", 
			                               NAO_PREFIX "propertyValue",
			                               value);

			tracker_data_insert_statement (subject, 
			                               NAO_PREFIX "hasProperty", 
			                               ":1");

			g_free (key);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_SUBJECT) == 0) {
			tracker_data_insert_statement (subject,
						       TRACKER_NMO_PREFIX "messageSubject", 
						       values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_SENT) == 0) {
			tracker_data_insert_statement (subject,
						       TRACKER_NMO_PREFIX "receivedDate", 
						       values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_FROM) == 0) {
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

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_TO) == 0) {
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

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_CC) == 0) {
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
	}

}

static void 
perform_unset (TrackerEvolutionIndexer *object, 
	       const gchar             *subject)
{
	g_message ("Evolution Plugin is deleting subject:'%s'", subject);

	tracker_data_delete_resource (subject); 
}

static void
perform_cleanup (TrackerEvolutionIndexer *object)
{
	GError *error = NULL;

	g_message ("Evolution Plugin is deleting all items");
	tracker_data_update_sparql ("DELETE { ?s ?p ?o } WHERE { ?s nie:dataSource <" DATASOURCE_URN "> }", &error);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
set_stored_last_modseq (guint last_modseq)
{
	g_message ("Evolution Plugin is setting last modseq to:%d", last_modseq);
	tracker_data_manager_set_db_option_int ("EvolutionLastModseq", (gint) last_modseq);
}

void
tracker_evolution_indexer_set (TrackerEvolutionIndexer *object, 
			       const gchar             *subject, 
			       const GStrv              predicates,
			       const GStrv              values,
			       const guint              modseq,
			       DBusGMethodInvocation   *context,
			       GError                  *derror)
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
tracker_evolution_indexer_set_many (TrackerEvolutionIndexer *object, 
				    const GStrv              subjects, 
				    const GPtrArray         *predicates,
				    const GPtrArray         *values,
				    const guint              modseq,
				    DBusGMethodInvocation   *context,
				    GError                  *derror)
{
	guint len;
	guint i, amount = 0;

	dbus_async_return_if_fail (subjects != NULL, context);
	dbus_async_return_if_fail (predicates != NULL, context);
	dbus_async_return_if_fail (values != NULL, context);

	len = g_strv_length (subjects);

	dbus_async_return_if_fail (len == predicates->len, context);
	dbus_async_return_if_fail (len == values->len, context);

	tracker_data_begin_transaction ();

	for (i = 0; subjects[i] != NULL; i++) {
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
	}

	set_stored_last_modseq (modseq);

	tracker_data_commit_transaction ();

	dbus_g_method_return (context);
}

void
tracker_evolution_indexer_unset_many (TrackerEvolutionIndexer *object, 
				      const GStrv              subjects, 
				      const guint              modseq,
				      DBusGMethodInvocation   *context,
				      GError                  *derror)
{
	guint i, amount = 0;

	dbus_async_return_if_fail (subjects != NULL, context);

	tracker_data_begin_transaction ();

	for (i = 0; subjects[i]; i++) {
		perform_unset (object, subjects[i]);

		amount++;
		if (amount > TRANSACTION_MAX) {
			tracker_data_commit_transaction ();
			g_main_context_iteration (NULL, FALSE);
			tracker_data_begin_transaction ();
			amount = 0;
		}
	}

	set_stored_last_modseq (modseq);

	tracker_data_commit_transaction ();

	dbus_g_method_return (context);
}

void
tracker_evolution_indexer_unset (TrackerEvolutionIndexer *object, 
				 const gchar             *subject, 
				 const guint              modseq,
				 DBusGMethodInvocation   *context,
				 GError                  *derror)
{
	dbus_async_return_if_fail (subject != NULL, context);

	perform_unset (object, subject);

	dbus_g_method_return (context);
}

void
tracker_evolution_indexer_cleanup (TrackerEvolutionIndexer *object, 
				   const guint              modseq,
				   DBusGMethodInvocation   *context,
				   GError                  *derror)
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

	g_message ("Evolution Plugin is initializing");

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!error) {
		idx_indexer = g_object_new (TRACKER_TYPE_EVOLUTION_INDEXER, NULL);

		dbus_g_object_type_install_info (G_OBJECT_TYPE (idx_indexer), 
						 &dbus_glib_tracker_evolution_indexer_object_info);

		dbus_g_connection_register_g_object (connection, 
						     TRACKER_EVOLUTION_INDEXER_PATH, 
						     idx_indexer);
	}

	if (error) {
		g_critical ("Can't init DBus for Evolution support: %s", error->message);
		g_error_free (error);
	}
}

void
tracker_push_module_shutdown (void)
{
	g_message ("Evolution Plugin is shutting down");

	if (idx_indexer) {
		g_object_unref (idx_indexer);
	}
}
