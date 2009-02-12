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

#define METADATA_EMAIL_RECIPIENT     "Email:Recipient"
#define METADATA_EMAIL_DATE	     "Email:Date"
#define METADATA_EMAIL_SENDER	     "Email:Sender"
#define METADATA_EMAIL_SUBJECT	     "Email:Subject"
#define METADATA_EMAIL_SENT_TO	     "Email:SentTo"
#define METADATA_EMAIL_CC	     "Email:CC"
#define METADATA_EMAIL_TEXT	     "Email:Body"
#define METADATA_EMAIL_TAG	     "User:Keywords"

G_DEFINE_TYPE (TrackerEvolutionIndexer, tracker_evolution_indexer, G_TYPE_OBJECT)

/* This runs in-process of tracker-indexer */

static GObject *idx_indexer = NULL;

enum {
	PROP_0,
};

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
	const gchar *message_subject = user_data;
	gchar *subject = NULL;
	const gchar *disposition, *filename;
	GMimePart *part;

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

		subject = g_strdup_printf ("%s/%s", message_subject, 
					   filename);

		metadata = tracker_module_metadata_new ();

		tracker_module_metadata_add_string (metadata, 
						    "File:Path", 
						    subject);

		tracker_module_metadata_add_string (metadata, 
						    "File:Name", 
						    filename);

		data = tracker_module_metadata_get_hash_table (metadata);

		tracker_data_update_replace_service (subject, "EvolutionEmails", data);

		g_hash_table_destroy (data);
		g_object_unref (metadata);
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

	start_encoding += strlen ("charset=");

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
perform_set (TrackerEvolutionIndexer *object, 
	     const gchar *subject, 
	     const GStrv predicates, 
	     const GStrv values)
{
	guint i = 0;
	TrackerModuleMetadata *metadata;
	GHashTable *data;

	metadata = tracker_module_metadata_new ();

	while (predicates [i] != NULL && values[i] != NULL) {

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
			gchar *text, *orig_text, *ptr, *encoding;
			gchar *path = g_strdup (values[i]);
			off_t offset = 0;
			gboolean is_html;

			ptr = strstr (path, "/!");
			if (ptr) {
				offset = (off_t) atol (ptr+2);
				*ptr = '\0';
			}

			fd = tracker_file_open (path, FALSE);

			g_free (path);

			if (fd == -1) 
				goto cont;

			stream = g_mime_stream_fs_new_with_bounds (fd, offset, -1);

			if (!stream) {
				close (fd);
				goto cont;
			}

			parser = g_mime_parser_new_with_stream (stream);

			if (!parser) {
				g_object_unref (stream);
				goto cont;
			}

			g_mime_parser_set_scan_from (parser, FALSE);

			message = g_mime_parser_construct_message (parser);

			if (!message) {
				g_object_unref (parser);
				g_object_unref (stream);
				goto cont;
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
				} else
					text = orig_text;

				tracker_module_metadata_add_string (metadata, 
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

			if (!values[i] || strlen (values[i]) < 1)
				goto cont;

			key = g_strdup (values[i]);

			value = strchr (key, '=');

			if (value) {
				*value = '\0';
				value++;
			}

			/* TODO: what about value? The format of Evolution is
			 * key=value, so we can store a value too here. Is this 
			 * something Nepomuk can someday save us with? */

			tracker_module_metadata_add_string (metadata, 
							    METADATA_EMAIL_TAG, 
							    key);

			g_free (key);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_SUBJECT) == 0) {
			tracker_module_metadata_add_string (metadata, 
							    METADATA_EMAIL_SUBJECT, 
							    values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_SENT) == 0) {
			tracker_module_metadata_add_string (metadata, 
							    METADATA_EMAIL_DATE, 
							    values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_FROM) == 0) {
			tracker_module_metadata_add_string (metadata, 
							    METADATA_EMAIL_SENDER, 
							    values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_TO) == 0) {
			tracker_module_metadata_add_string (metadata, 
							    METADATA_EMAIL_SENT_TO, 
							    values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_CC) == 0) {
			tracker_module_metadata_add_string (metadata, 
							    METADATA_EMAIL_CC, 
							    values[i]);
		}

		cont:

		i++;
	}

	data = tracker_module_metadata_get_hash_table (metadata);

	tracker_data_update_replace_service (subject, "EvolutionEmails", data);

	g_hash_table_destroy (data);
	g_object_unref (metadata);
}

static void 
perform_unset (TrackerEvolutionIndexer *object, 
	       const gchar *subject)
{
	tracker_data_update_delete_service_by_path (subject, "EvolutionEmails"); 
}

static void
perform_cleanup (TrackerEvolutionIndexer *object)
{
	tracker_data_update_delete_service_all ("EvolutionEmails");
}

static void
set_stored_last_modseq (guint last_modseq)
{
	tracker_data_manager_set_db_option_int ("EvolutionLastModseq", (gint) last_modseq);
}

void
tracker_evolution_indexer_set (TrackerEvolutionIndexer *object, 
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
tracker_evolution_indexer_set_many (TrackerEvolutionIndexer *object, 
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
tracker_evolution_indexer_unset_many (TrackerEvolutionIndexer *object, 
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
tracker_evolution_indexer_unset (TrackerEvolutionIndexer *object, 
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
tracker_evolution_indexer_cleanup (TrackerEvolutionIndexer *object, 
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
	if (idx_indexer)
		g_object_unref (idx_indexer);
}
