/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <libtracker-data/tracker-data-metadata.h>

#include <tracker-indexer/tracker-module-file.h>
#include <tracker-indexer/tracker-module-iteratable.h>

#include "evolution-pop.h"
#include "evolution-common.h"

#define MODULE_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) \
{ \
    const GInterfaceInfo g_implement_interface_info = { \
      (GInterfaceInitFunc) iface_init, NULL, NULL \
  }; \
  g_type_module_add_interface (type_module, g_define_type_id, TYPE_IFACE, &g_implement_interface_info); \
}

static void          tracker_evolution_pop_file_finalize         (GObject *object);

static void          tracker_evolution_pop_file_initialize       (TrackerModuleFile *file);
static const gchar * tracker_evolution_pop_file_get_service_type (TrackerModuleFile *file);
static gchar *       tracker_evolution_pop_file_get_uri          (TrackerModuleFile *file);
static gchar *       tracker_evolution_pop_file_get_text         (TrackerModuleFile *file);
static TrackerModuleMetadata *
                     tracker_evolution_pop_file_get_metadata     (TrackerModuleFile *file);
static TrackerModuleFlags
                     tracker_evolution_pop_file_get_flags        (TrackerModuleFile *file);

static void          tracker_evolution_pop_file_iteratable_init  (TrackerModuleIteratableIface *iface);
static gboolean      tracker_evolution_pop_file_iter_contents    (TrackerModuleIteratable *iteratable);


G_DEFINE_DYNAMIC_TYPE_EXTENDED (TrackerEvolutionPopFile, tracker_evolution_pop_file, TRACKER_TYPE_MODULE_FILE, 0,
                                MODULE_IMPLEMENT_INTERFACE (TRACKER_TYPE_MODULE_ITERATABLE,
                                                            tracker_evolution_pop_file_iteratable_init))


static void
tracker_evolution_pop_file_class_init (TrackerEvolutionPopFileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerModuleFileClass *file_class = TRACKER_MODULE_FILE_CLASS (klass);

        object_class->finalize = tracker_evolution_pop_file_finalize;

        file_class->initialize = tracker_evolution_pop_file_initialize;
        file_class->get_service_type = tracker_evolution_pop_file_get_service_type;
        file_class->get_uri = tracker_evolution_pop_file_get_uri;
        file_class->get_text = tracker_evolution_pop_file_get_text;
        file_class->get_metadata = tracker_evolution_pop_file_get_metadata;
	file_class->get_flags = tracker_evolution_pop_file_get_flags;
}

static void
tracker_evolution_pop_file_class_finalize (TrackerEvolutionPopFileClass *klass)
{
}

static void
tracker_evolution_pop_file_init (TrackerEvolutionPopFile *file)
{
}

static void
tracker_evolution_pop_file_iteratable_init (TrackerModuleIteratableIface *iface)
{
        iface->iter_contents = tracker_evolution_pop_file_iter_contents;
}

static void
tracker_evolution_pop_file_finalize (GObject *object)
{
        TrackerEvolutionPopFile *file;

        file = TRACKER_EVOLUTION_POP_FILE (object);

        if (file->mime_parts) {
		g_list_foreach (file->mime_parts, (GFunc) g_object_unref, NULL);
		g_list_free (file->mime_parts);
	}

	if (file->message) {
		g_object_unref (file->message);
	}

	if (file->parser) {
		g_object_unref (file->parser);
	}

	if (file->stream) {
		g_mime_stream_close (file->stream);
		g_object_unref (file->stream);
	}

        g_free (file->local_folder);

        G_OBJECT_CLASS (tracker_evolution_pop_file_parent_class)->finalize (object);
}

static void
tracker_evolution_pop_file_initialize (TrackerModuleFile *file)
{
        TrackerEvolutionPopFile *self;
        gchar *path;

        self = TRACKER_EVOLUTION_POP_FILE (file);
        path = g_file_get_path (tracker_module_file_get_file (file));

        self->local_folder = g_build_filename (g_get_home_dir (),
                                               ".evolution", "mail", "local", G_DIR_SEPARATOR_S,
                                               NULL);

#if defined(__linux__)
        self->stream = evolution_common_get_stream (path, O_RDONLY | O_NOATIME, 0);
#else
        self->stream = evolution_common_get_stream (path, O_RDONLY, 0);
#endif

        if (self->stream) {
                self->parser = g_mime_parser_new_with_stream (self->stream);
                g_mime_parser_set_scan_from (self->parser, TRUE);

                /* Initialize to the first message */
                self->message = g_mime_parser_construct_message (self->parser);
        }

        g_free (path);
}

static const gchar *
tracker_evolution_pop_file_get_service_type (TrackerModuleFile *file)
{
        TrackerEvolutionPopFile *self;

        self = TRACKER_EVOLUTION_POP_FILE (file);

        if (self->current_mime_part) {
                return "EvolutionAttachments";
        }

        return "EvolutionEmails";
}

static gint
get_message_id (GMimeMessage *message)
{
	const gchar *header, *pos;
	gchar *number;
	gint id;

	header = g_mime_object_get_header (GMIME_OBJECT (message), "X-Evolution");

        if (!header) {
                return -1;
        }

        pos = strchr (header, '-');

	number = g_strndup (header, pos - header);
	id = strtoul (number, NULL, 16);

	g_free (number);

	return id;
}

static gchar *
get_message_uri (TrackerModuleFile *file,
                 GMimeMessage      *message)
{
        TrackerEvolutionPopFile *self;
	gchar *path, *uri;
        gint message_id;

        self = TRACKER_EVOLUTION_POP_FILE (file);
        message_id = get_message_id (message);

        if (message_id < 0) {
                return NULL;
        }

        path = g_file_get_path (tracker_module_file_get_file (file));
        path = tracker_string_remove (path, self->local_folder);
	path = tracker_string_remove (path, ".sbd");

        uri = g_strdup_printf ("email://local@local/%s;uid=%d", path, message_id);

        g_free (path);

        return uri;
}

static gchar *
tracker_evolution_pop_file_get_uri (TrackerModuleFile *file)
{
        TrackerEvolutionPopFile *self;
        gchar *message_uri;

        self = TRACKER_EVOLUTION_POP_FILE (file);

        if (!self->message) {
                return NULL;
        }

        message_uri = get_message_uri (file, self->message);

        if (!message_uri) {
                return NULL;
        }

        if (self->current_mime_part) {
                gchar *attachment_uri;
                const gchar *part_filename;

                part_filename = g_mime_part_get_filename (self->current_mime_part->data);
                attachment_uri = g_strdup_printf ("%s/%s", message_uri, part_filename);
                g_free (message_uri);

                return attachment_uri;
        }

        return message_uri;
}

static gchar *
tracker_evolution_pop_file_get_text (TrackerModuleFile *file)
{
        TrackerEvolutionPopFile *self;
	const gchar *encoding;
	gchar buffer[1024];
	guint len;
	GString *body;
	GMimeStream *stream;
	GMimeDataWrapper *data;

        self = TRACKER_EVOLUTION_POP_FILE (file);

	if (self->current_mime_part) {
		/* FIXME: Extract text from attachments */
		return NULL;
	}

	data = g_mime_part_get_content_object (GMIME_PART (self->message));

	if (!data)
		return NULL;

	stream = g_mime_data_wrapper_get_stream (data);

	if (!stream) {
		g_object_unref (data);
		return NULL;
	}

	body = g_string_new ("");

	encoding = g_mime_object_get_content_disposition_parameter (GMIME_OBJECT (self->message), "charset");

	while (!g_mime_stream_eos (stream)) {
		len = g_mime_stream_read (stream, buffer, 1024);
		if (len > 0 && g_utf8_validate (buffer, len, NULL)) {
			if (!encoding)
				g_string_append_len (body, buffer, (gssize) len);
			else {
 				gchar *part_body = g_convert (buffer, (gssize) len, "utf8", encoding, NULL, NULL, NULL);
				g_string_append (body, part_body);
				g_free (part_body);
			}
		}
	}

	g_object_unref (stream);

	return g_string_free (body, FALSE);
}

static guint
get_message_flags (GMimeMessage *message)
{
	const gchar *header, *pos;

	header = g_mime_object_get_header (GMIME_OBJECT (message), "X-Evolution");

	if (!header) {
		return 0;
	}

	pos = strchr (header, '-');

	return (guint) strtoul (pos + 1, NULL, 16);
}

static GList *
get_message_recipients (GMimeMessage *message,
                        GMimeRecipientType type)
{
	GList *list = NULL;
	InternetAddressList *addresses;
	guint len, i;

	addresses = g_mime_message_get_recipients (message, type);

	len = internet_address_list_length (addresses);
	
	while (i < len) {
		InternetAddress *address;

		address = internet_address_list_get_address (addresses, i);

		list = g_list_prepend (list, internet_address_to_string (address, TRUE));

		i++;
	}

	return g_list_reverse (list);
}

static TrackerModuleMetadata *
get_message_metadata (GMimeMessage *message)
{
	TrackerModuleMetadata *metadata;
	time_t t;
	GList *list, *l;

	metadata = tracker_module_metadata_new ();

	g_mime_message_get_date (message, &t, NULL);

	tracker_module_metadata_add_date (metadata, METADATA_EMAIL_DATE, t);
	tracker_module_metadata_add_string (metadata, METADATA_EMAIL_SENDER, g_mime_message_get_sender (message));
	tracker_module_metadata_add_string (metadata, METADATA_EMAIL_SUBJECT, g_mime_message_get_subject (message));

	list = get_message_recipients (message, GMIME_RECIPIENT_TYPE_TO);

	for (l = list; l; l = l->next) {
		tracker_module_metadata_add_string (metadata, METADATA_EMAIL_SENT_TO, l->data);
		g_free (l->data);
	}

	g_list_free (list);

	list = get_message_recipients (message, GMIME_RECIPIENT_TYPE_CC);

	for (l = list; l; l = l->next) {
		tracker_module_metadata_add_string (metadata, METADATA_EMAIL_CC, l->data);
		g_free (l->data);
	}

	g_list_free (list);

	return metadata;
}

static TrackerModuleMetadata *
get_attachment_metadata (GMimePart *part)
{
	TrackerModuleMetadata *metadata;
	GMimeDataWrapper *content;

	content = g_mime_part_get_content_object (part);

	if (!content) {
		return NULL;
	}

	metadata = evolution_common_get_wrapper_metadata (content);
	g_object_unref (content);

	return metadata;
}

static TrackerModuleMetadata *
tracker_evolution_pop_file_get_metadata (TrackerModuleFile *file)
{
        TrackerEvolutionPopFile *self;
        TrackerModuleMetadata *metadata;
        guint flags;

        self = TRACKER_EVOLUTION_POP_FILE (file);

        if (!self->message) {
                return NULL;
        }

        flags = get_message_flags (self->message);

	if (flags & EVOLUTION_MESSAGE_JUNK ||
	    flags & EVOLUTION_MESSAGE_DELETED) {
		return NULL;
	}

        if (self->current_mime_part) {
                metadata = get_attachment_metadata (self->current_mime_part->data);
        } else {
                metadata = get_message_metadata (self->message);
        }

        return metadata;
}

static TrackerModuleFlags
tracker_evolution_pop_file_get_flags (TrackerModuleFile *file)
{
	return TRACKER_FILE_CONTENTS_STATIC;
}

static void
extract_mime_parts (GMimeObject *parent, 
                    GMimeObject *object,
                    gpointer     user_data)
{
	GList **list = (GList **) user_data;
	const gchar *disposition, *filename;
	GMimePart *part;

	if (GMIME_IS_MESSAGE_PART (object)) {
		GMimeMessage *message;

		message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (object));

		if (message) {
			g_mime_message_foreach (message, extract_mime_parts, user_data);
			g_object_unref (message);
		}

		return;
	} else if (GMIME_IS_MULTIPART (object)) {
		g_mime_multipart_foreach (GMIME_MULTIPART (object), extract_mime_parts, user_data);
		return;
	}

	part = GMIME_PART (object);
	disposition = g_mime_object_get_disposition (GMIME_OBJECT (part));

	if (!disposition ||
	    (strcmp (disposition, GMIME_DISPOSITION_ATTACHMENT) != 0 &&
	     strcmp (disposition, GMIME_DISPOSITION_INLINE) != 0)) {
		return;
	}

	filename = g_mime_part_get_filename (GMIME_PART (object));

	if (!filename ||
	    strcmp (filename, "signature.asc") == 0 ||
	    strcmp (filename, "signature.pgp") == 0) {
		return;
	}

	*list = g_list_prepend (*list, g_object_ref (object));
}

static gboolean
tracker_evolution_pop_file_iter_contents (TrackerModuleIteratable *iteratable)
{
        TrackerEvolutionPopFile *self;

        self = TRACKER_EVOLUTION_POP_FILE (iteratable);

        if (!self->parser) {
                return FALSE;
        }

        if (self->message) {
                /* Iterate through mime parts, if any */
                if (!self->mime_parts) {
                        g_mime_message_foreach (self->message,
                                                extract_mime_parts,
                                                &self->mime_parts);
                        self->current_mime_part = self->mime_parts;
                } else {
                        self->current_mime_part = self->current_mime_part->next;
                }

                if (self->current_mime_part) {
                        return TRUE;
                }

                /* all possible mime parts have been already iterated, move on */
                g_object_unref (self->message);

                g_list_foreach (self->mime_parts, (GFunc) g_object_unref, NULL);
                g_list_free (self->mime_parts);
                self->mime_parts = NULL;
        }

        self->message = g_mime_parser_construct_message (self->parser);

        return (self->message != NULL);
}

void
tracker_evolution_pop_file_register (GTypeModule *module)
{
        tracker_evolution_pop_file_register_type (module);
}

TrackerModuleFile *
tracker_evolution_pop_file_new (GFile *file)
{
        return g_object_new (TRACKER_TYPE_EVOLUTION_POP_FILE,
                             "file", file,
                             NULL);
}
