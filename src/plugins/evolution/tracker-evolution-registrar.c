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

#include <glib-object.h>
#include <dbus/dbus-glib-bindings.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-sparql-builder.h>

#include <tracker-store/tracker-push-registrar.h>
#include <tracker-store/tracker-store.h>

#define __TRACKER_EVOLUTION_REGISTRAR_C__

#include "tracker-evolution-registrar.h"
#include "tracker-evolution-registrar-glue.h"

#define TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_EVOLUTION_REGISTRAR, TrackerEvolutionRegistrarPrivate))

#define TRACKER_TYPE_EVOLUTION_PUSH_REGISTRAR    (tracker_evolution_push_registrar_get_type ())
#define TRACKER_EVOLUTION_PUSH_REGISTRAR(module) (G_TYPE_CHECK_INSTANCE_CAST ((module), TRACKER_TYPE_EVOLUTION_PUSH_REGISTRAR, TrackerEvolutionPushRegistrar))

#define NIE_DATASOURCE 			       TRACKER_NIE_PREFIX "DataSource"
#define NIE_DATASOURCE_P 		       TRACKER_NIE_PREFIX "dataSource"

#define RDF_PREFIX	TRACKER_RDF_PREFIX
#define NMO_PREFIX	TRACKER_NMO_PREFIX
#define NCO_PREFIX	TRACKER_NCO_PREFIX
#define NAO_PREFIX	TRACKER_NAO_PREFIX

#define DATASOURCE_URN			       "urn:nepomuk:datasource:1cb1eb90-1241-11de-8c30-0800200c9a66"

typedef struct TrackerEvolutionPushRegistrar TrackerEvolutionPushRegistrar;
typedef struct TrackerEvolutionPushRegistrarClass TrackerEvolutionPushRegistrarClass;

struct TrackerEvolutionPushRegistrar {
	TrackerPushRegistrar parent_instance;
};

struct TrackerEvolutionPushRegistrarClass {
	TrackerPushRegistrarClass parent_class;
};


typedef struct {
	gpointer dummy;
} TrackerEvolutionRegistrarPrivate;

enum {
	PROP_0,
};

static GType tracker_evolution_push_registrar_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (TrackerEvolutionRegistrar, tracker_evolution_registrar, G_TYPE_OBJECT)
G_DEFINE_TYPE (TrackerEvolutionPushRegistrar, tracker_evolution_push_registrar, TRACKER_TYPE_PUSH_REGISTRAR);

static void
tracker_evolution_registrar_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_evolution_registrar_parent_class)->finalize (object);
}


static void
tracker_evolution_registrar_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_evolution_registrar_get_property (GObject    *object,
					  guint       prop_id,
					  GValue     *value,
					  GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_evolution_registrar_class_init (TrackerEvolutionRegistrarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_evolution_registrar_finalize;
	object_class->set_property = tracker_evolution_registrar_set_property;
	object_class->get_property = tracker_evolution_registrar_get_property;

	g_type_class_add_private (object_class, sizeof (TrackerEvolutionRegistrarPrivate));
}

static void
tracker_evolution_registrar_init (TrackerEvolutionRegistrar *object)
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

		data_insert_statement (subject, 
		                      "File:Path", 
				       filename);

		data_insert_statement (subject, 
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
perform_set (TrackerEvolutionRegistrar *object, 
	     const gchar *subject, 
	     const GStrv predicates, 
	     const GStrv values)
{
	guint i = 0;
	TrackerSparqlBuilder *sparql;

	sparql = tracker_sparql_builder_new_update ();

	tracker_sparql_builder_insert_open (sparql);

	tracker_sparql_builder_subject_iri (sparql, DATASOURCE_URN);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object_iri (sparql, NIE_DATASOURCE);

	tracker_sparql_builder_subject_iri (sparql, subject);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nmo:Email");

	tracker_sparql_builder_subject_iri (sparql, subject);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nmo:MailboxDataObject");

	tracker_sparql_builder_subject_iri (sparql, subject);
	tracker_sparql_builder_predicate_iri (sparql, NIE_DATASOURCE_P);
	tracker_sparql_builder_object_iri (sparql, DATASOURCE_URN);

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

				data_insert_statement (object, subject, 
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

			tracker_sparql_builder_subject_iri (sparql, subject);

			if (value) {
				*value = '\0';
				value++;

				tracker_sparql_builder_predicate (sparql, "nao:hasProperty");

				tracker_sparql_builder_object_blank_open (sparql);

				tracker_sparql_builder_predicate (sparql, "rdf:type");
				tracker_sparql_builder_object (sparql, "nao:Property");

				tracker_sparql_builder_predicate (sparql, "nao:propertyName");
				tracker_sparql_builder_object (sparql, key);
				
				tracker_sparql_builder_predicate (sparql, "nao:propertyValue");
				tracker_sparql_builder_object_string (sparql, value);

				tracker_sparql_builder_object_blank_close (sparql);

			} else {
				tracker_sparql_builder_predicate (sparql, "nao:hasTag");

				tracker_sparql_builder_object_blank_open (sparql);

				tracker_sparql_builder_predicate (sparql, "rdf:type");
				tracker_sparql_builder_object (sparql, "nao:Tag");

				tracker_sparql_builder_predicate (sparql, "nao:prefLabel");
				tracker_sparql_builder_object_string (sparql, key);

				tracker_sparql_builder_object_blank_close (sparql);

			}

			g_free (key);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_SUBJECT) == 0) {
			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:messageSubject");
			tracker_sparql_builder_object_string (sparql, values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_SENT) == 0) {
			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:receivedDate");
			tracker_sparql_builder_object_string (sparql, values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_FROM) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;

			get_email_and_fullname (values[i], &email, &fullname);

			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:EmailAddress");

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "nco:emailAddress");
			tracker_sparql_builder_object_string (sparql, email);

			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:from");

			tracker_sparql_builder_object_blank_open (sparql);

			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:Contact");

			if (fullname) {
				tracker_sparql_builder_predicate (sparql, "nco:fullname");
				tracker_sparql_builder_object_string (sparql, fullname);
				g_free (fullname);
			}

			tracker_sparql_builder_predicate (sparql, "nco:hasEmailAddress");
			tracker_sparql_builder_object_iri (sparql, email_uri);

			tracker_sparql_builder_object_blank_close (sparql);

			g_free (email_uri);
			g_free (email);
		}


		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_TO) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;

			get_email_and_fullname (values[i], &email, &fullname);

			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:EmailAddress");

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "nco:emailAddress");
			tracker_sparql_builder_object_string (sparql, email);

			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:to");

			tracker_sparql_builder_object_blank_open (sparql);

			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:Contact");

			if (fullname) {
				tracker_sparql_builder_predicate (sparql, "nco:fullname");
				tracker_sparql_builder_object_string (sparql, fullname);
				g_free (fullname);
			}

			tracker_sparql_builder_predicate (sparql, "nco:hasEmailAddress");
			tracker_sparql_builder_object_iri (sparql, email_uri);

			tracker_sparql_builder_object_blank_close (sparql);

			g_free (email_uri);
			g_free (email);
		}

		if (g_strcmp0 (predicates[i], TRACKER_EVOLUTION_PREDICATE_CC) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;

			get_email_and_fullname (values[i], &email, &fullname);

			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:EmailAddress");

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "nco:emailAddress");
			tracker_sparql_builder_object_string (sparql, email);

			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:cc");

			tracker_sparql_builder_object_blank_open (sparql);

			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:Contact");

			if (fullname) {
				tracker_sparql_builder_predicate (sparql, "nco:fullname");
				tracker_sparql_builder_object_string (sparql, fullname);
				g_free (fullname);
			}

			tracker_sparql_builder_predicate (sparql, "nco:hasEmailAddress");
			tracker_sparql_builder_object_iri (sparql, email_uri);

			tracker_sparql_builder_object_blank_close (sparql);

			g_free (email_uri);
			g_free (email);
		}

		cont:

		i++;
	}

	tracker_store_queue_sparql_update (tracker_sparql_builder_get_result (sparql),
	                                   NULL, NULL, NULL);

	g_object_unref (sparql);
}

static void 
perform_unset (TrackerEvolutionRegistrar *object, 
	       const gchar *subject, gboolean batch)
{
	gchar *sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", subject);

	if (batch) {
		tracker_store_sparql_update (sparql, NULL);
	} else {
		tracker_store_queue_sparql_update (sparql, NULL, NULL, NULL);
	}

	g_free (sparql);
}

static void
perform_cleanup (TrackerEvolutionRegistrar *object)
{
	tracker_store_sparql_update ("DELETE { ?s ?p ?o } WHERE { ?s nie:dataSource <" DATASOURCE_URN "> }", NULL);
}

static void
set_stored_last_modseq (guint last_modseq)
{
	tracker_data_manager_set_db_option_int ("EvolutionLastModseq", (gint) last_modseq);
}


void
tracker_evolution_registrar_set (TrackerEvolutionRegistrar *object, 
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

	tracker_store_queue_commit (NULL, NULL, NULL);

	dbus_g_method_return (context);
}



typedef struct {
	TrackerEvolutionRegistrar *object;
	GStrv subjects;
	GPtrArray *predicates;
	GPtrArray *values;
	guint modseq;
	DBusGMethodInvocation *context;
} SetManyInfo;

static gboolean
set_many_idle (gpointer user_data)
{
	guint i = 0;
	SetManyInfo *info = user_data;
	gboolean cont = FALSE;

	while (info->subjects[i] != NULL) {
		GStrv preds = g_ptr_array_index (info->predicates, i);
		GStrv vals = g_ptr_array_index (info->values, i);

		perform_set (info->object, info->subjects[i], preds, vals);

		if (i > 100) {
			cont = TRUE;
			break;
		}

		i++;
	}

	return cont;
}

static void
strv_ptrarray_free (GPtrArray *array)
{
	guint i;

	for (i = 0; i < array->len; i++) {
		g_strfreev (g_ptr_array_index (array, i));
	}

	g_ptr_array_free (array, TRUE);
}

static void 
set_many_destroy (gpointer user_data)
{
	SetManyInfo *info = user_data;

	strv_ptrarray_free (info->predicates);
	strv_ptrarray_free (info->values);
	g_strfreev (info->subjects);

	set_stored_last_modseq (info->modseq);

	tracker_store_queue_commit (NULL, NULL, NULL);

	dbus_g_method_return (info->context);

	g_object_unref (info->object);
	g_free (info);
}

static GPtrArray* 
strv_ptrarray_dup (const GPtrArray *array)
{
	GPtrArray *new_array = g_ptr_array_sized_new (array->len);
	guint i;

	for (i = 0; i < array->len; i++) {
		g_ptr_array_add	(new_array, g_strdupv (
		                 g_ptr_array_index (array, i)));
	}

	return new_array;
}

void
tracker_evolution_registrar_set_many (TrackerEvolutionRegistrar *object, 
				      const GStrv subjects, 
				      const GPtrArray *predicates,
				      const GPtrArray *values,
				      const guint modseq,
				      DBusGMethodInvocation *context,
				      GError *derror)
{
	guint len;
	SetManyInfo *info;

	dbus_async_return_if_fail (subjects != NULL, context);
	dbus_async_return_if_fail (predicates != NULL, context);
	dbus_async_return_if_fail (values != NULL, context);

	len = g_strv_length (subjects);

	dbus_async_return_if_fail (len == predicates->len, context);
	dbus_async_return_if_fail (len == values->len, context);

	info = g_new0 (SetManyInfo, 1);

	info->object = g_object_ref (object);
	info->context = context;
	info->modseq = modseq;
	info->subjects = g_strdupv (subjects);
	info->predicates = strv_ptrarray_dup (predicates);
	info->values = strv_ptrarray_dup (values);

	g_idle_add_full (G_PRIORITY_DEFAULT,
	                 set_many_idle,
	                 info,
	                 set_many_destroy);
}


typedef struct {
	GStrv subjects;
	guint modseq;
	DBusGMethodInvocation *context;
	TrackerEvolutionRegistrar *object;
} UnsetManyInfo;

static gboolean
unset_many_idle (gpointer user_data)
{
	guint i = 0;
	gboolean cont = FALSE;
	UnsetManyInfo *info = user_data;

	while (info->subjects[i] != NULL) {

		perform_unset (info->object, info->subjects[i], TRUE);

		if (i > 100) {
			cont = TRUE;
			break;
		}

		i++;
	}

	return cont;
}

static void
unset_many_destroy (gpointer user_data)
{
	UnsetManyInfo *info = user_data;

	set_stored_last_modseq (info->modseq);

	tracker_store_queue_commit (NULL, NULL, NULL);

	dbus_g_method_return (info->context);

	g_object_unref (info->object);
	g_free (info);
}

void
tracker_evolution_registrar_unset_many (TrackerEvolutionRegistrar *object, 
					const GStrv subjects, 
					const guint modseq,
					DBusGMethodInvocation *context,
					GError *derror)
{
	UnsetManyInfo *info;

	dbus_async_return_if_fail (subjects != NULL, context);

	info = g_new0 (UnsetManyInfo, 1);

	info->object = g_object_ref (object);
	info->context = context;
	info->modseq = modseq;
	info->subjects = g_strdupv (subjects);

	g_idle_add_full (G_PRIORITY_DEFAULT,
	                 unset_many_idle,
	                 info,
	                 unset_many_destroy);
}

void
tracker_evolution_registrar_unset (TrackerEvolutionRegistrar *object, 
				   const gchar *subject, 
				   const guint modseq,
				   DBusGMethodInvocation *context,
				   GError *derror)
{
	dbus_async_return_if_fail (subject != NULL, context);

	perform_unset (object, subject, FALSE);

	dbus_g_method_return (context);
}

void
tracker_evolution_registrar_cleanup (TrackerEvolutionRegistrar *object, 
				     const guint modseq,
				     DBusGMethodInvocation *context,
				     GError *derror)
{
	perform_cleanup (object);

	set_stored_last_modseq (modseq);

	dbus_g_method_return (context);
}


static void
on_manager_destroy (DBusGProxy *proxy, gpointer user_data)
{
	return;
}

static void
tracker_evolution_push_registrar_enable (TrackerPushRegistrar *registrar, 
					 DBusGConnection      *connection,
					 DBusGProxy           *dbus_proxy, 
					 GError              **error)
{
	GError *nerror = NULL;
	guint result;
	DBusGProxy *manager_proxy;
	GObject *object;

	tracker_push_registrar_set_object (registrar, NULL);
	tracker_push_registrar_set_manager (registrar, NULL);

	manager_proxy = dbus_g_proxy_new_for_name (connection,
						   TRACKER_EVOLUTION_MANAGER_SERVICE,
						   TRACKER_EVOLUTION_MANAGER_PATH,
						   TRACKER_EVOLUTION_MANAGER_INTERFACE);

	/* Creation of the registrar */
	if (!org_freedesktop_DBus_request_name (dbus_proxy, 
						TRACKER_EVOLUTION_REGISTRAR_SERVICE,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&result, &nerror)) {

		g_critical ("Could not setup DBus, %s in use\n", 
			    TRACKER_EVOLUTION_REGISTRAR_SERVICE);

		if (nerror) {
			g_propagate_error (error, nerror);
			return;
		}
	}

	if (nerror) {
		g_propagate_error (error, nerror);
		return;
	}

	object = g_object_new (TRACKER_TYPE_EVOLUTION_REGISTRAR, NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_tracker_evolution_registrar_object_info);

	dbus_g_connection_register_g_object (connection, 
					     TRACKER_EVOLUTION_REGISTRAR_PATH, 
					     object);

	/* Registration of the registrar to the manager */
	dbus_g_proxy_call_no_reply (manager_proxy, "Register",
				    G_TYPE_OBJECT, object, 
				    G_TYPE_UINT, (guint) tracker_data_manager_get_db_option_int ("EvolutionLastModseq"),
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);

	/* If while we had a proxy for the manager the manager shut itself down,
	 * then we'll get rid of our registrar too, in on_manager_destroy */

	g_signal_connect (manager_proxy, "destroy",
			  G_CALLBACK (on_manager_destroy), registrar);

	tracker_push_registrar_set_object (registrar, object);
	tracker_push_registrar_set_manager (registrar, manager_proxy);

	g_object_unref (object); /* sink own */
	g_object_unref (manager_proxy);  /* sink own */
}

static void
tracker_evolution_push_registrar_disable (TrackerPushRegistrar *registrar)
{
	tracker_push_registrar_set_object (registrar, NULL);
	tracker_push_registrar_set_manager (registrar, NULL);
}

static void
tracker_evolution_push_registrar_class_init (TrackerEvolutionPushRegistrarClass *klass)
{
	TrackerPushRegistrarClass *p_class = TRACKER_PUSH_REGISTRAR_CLASS (klass);

	p_class->enable = tracker_evolution_push_registrar_enable;
	p_class->disable = tracker_evolution_push_registrar_disable;
}

static void
tracker_evolution_push_registrar_init (TrackerEvolutionPushRegistrar *registrar)
{
	return;
}

TrackerPushRegistrar *
tracker_push_module_init (void)
{
	GObject *object;

	object = g_object_new (TRACKER_TYPE_EVOLUTION_PUSH_REGISTRAR, NULL);

	tracker_push_registrar_set_service (TRACKER_PUSH_REGISTRAR (object),
					    TRACKER_EVOLUTION_MANAGER_SERVICE);

	return TRACKER_PUSH_REGISTRAR (object);
}

void
tracker_push_module_shutdown (TrackerPushRegistrar *registrar)
{
	tracker_evolution_push_registrar_disable (registrar);
}
