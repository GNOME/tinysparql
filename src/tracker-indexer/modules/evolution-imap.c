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

#include <gconf/gconf-client.h>

#include <libtracker-data/tracker-data-metadata.h>

#include <tracker-indexer/tracker-module-file.h>
#include <tracker-indexer/tracker-module-iteratable.h>

#include "evolution-imap.h"
#include "evolution-common.h"

#define METADATA_EMAIL_RECIPIENT     "Email:Recipient"
#define METADATA_EMAIL_DATE	     "Email:Date"
#define METADATA_EMAIL_SENDER	     "Email:Sender"
#define METADATA_EMAIL_SUBJECT	     "Email:Subject"
#define METADATA_EMAIL_SENT_TO	     "Email:SentTo"
#define METADATA_EMAIL_CC	     "Email:CC"

#define MODULE_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) \
{ \
    const GInterfaceInfo g_implement_interface_info = { \
      (GInterfaceInitFunc) iface_init, NULL, NULL \
  }; \
  g_type_module_add_interface (type_module, g_define_type_id, TYPE_IFACE, &g_implement_interface_info); \
}

typedef struct EvolutionAccountContext EvolutionAccountContext;


enum {
	SUMMARY_TYPE_INT32,
	SUMMARY_TYPE_UINT32,
	SUMMARY_TYPE_STRING,
	SUMMARY_TYPE_TOKEN,
	SUMMARY_TYPE_TIME_T
};

struct EvolutionAccountContext {
	gchar *account;
	gchar *uid;
};

static GHashTable *accounts = NULL;


static void          tracker_evolution_imap_file_finalize         (GObject *object);

static void          tracker_evolution_imap_file_initialize       (TrackerModuleFile *file);
static const gchar * tracker_evolution_imap_file_get_service_type (TrackerModuleFile *file);
static gchar *       tracker_evolution_imap_file_get_uri          (TrackerModuleFile *file);
static gchar *       tracker_evolution_imap_file_get_text         (TrackerModuleFile *file);
static TrackerModuleMetadata *
                     tracker_evolution_imap_file_get_metadata     (TrackerModuleFile *file);
static TrackerModuleFlags
                     tracker_evolution_imap_file_get_flags        (TrackerModuleFile *file);

static void          tracker_evolution_imap_file_iteratable_init  (TrackerModuleIteratableIface *iface);
static gboolean      tracker_evolution_imap_file_iter_contents    (TrackerModuleIteratable *iteratable);
static guint         tracker_evolution_imap_file_get_count        (TrackerModuleIteratable *iteratable);


G_DEFINE_DYNAMIC_TYPE_EXTENDED (TrackerEvolutionImapFile, tracker_evolution_imap_file, TRACKER_TYPE_MODULE_FILE, 0,
                                MODULE_IMPLEMENT_INTERFACE (TRACKER_TYPE_MODULE_ITERATABLE,
                                                            tracker_evolution_imap_file_iteratable_init))


static void
tracker_evolution_imap_file_class_init (TrackerEvolutionImapFileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerModuleFileClass *file_class = TRACKER_MODULE_FILE_CLASS (klass);

        object_class->finalize = tracker_evolution_imap_file_finalize;

        file_class->initialize = tracker_evolution_imap_file_initialize;
        file_class->get_service_type = tracker_evolution_imap_file_get_service_type;
        file_class->get_uri = tracker_evolution_imap_file_get_uri;
        file_class->get_text = tracker_evolution_imap_file_get_text;
        file_class->get_metadata = tracker_evolution_imap_file_get_metadata;
	file_class->get_flags = tracker_evolution_imap_file_get_flags;
}

static void
tracker_evolution_imap_file_class_finalize (TrackerEvolutionImapFileClass *klass)
{
}

static void
tracker_evolution_imap_file_init (TrackerEvolutionImapFile *file)
{
}

static void
tracker_evolution_imap_file_iteratable_init (TrackerModuleIteratableIface *iface)
{
        iface->iter_contents = tracker_evolution_imap_file_iter_contents;
        iface->get_count = tracker_evolution_imap_file_get_count;
}

static void
tracker_evolution_imap_file_finalize (GObject *object)
{
        TrackerEvolutionImapFile *file;

        file = TRACKER_EVOLUTION_IMAP_FILE (object);

        g_free (file->imap_dir);
        g_free (file->cur_message_uid);

        tracker_file_close (file->summary, FALSE);

        G_OBJECT_CLASS (tracker_evolution_imap_file_parent_class)->finalize (object);
}

static gboolean
read_summary (FILE *summary,
	      ...)
{
	va_list args;
	gint value_type;

	if (!summary) {
		return FALSE;
	}

	va_start (args, summary);

	while ((value_type = va_arg (args, gint)) != -1) {
		switch (value_type) {
		case SUMMARY_TYPE_TIME_T: {
                        time_t value = 0;
                        time_t *dest;
                        int size = sizeof (time_t) - 1;
                        int c = EOF;

                        while (size >= 0 && (c = fgetc (summary)) != EOF) {
                                value |= ((time_t) c) << (size * 8);
                                size--;
                        }

                        dest = va_arg (args, time_t*);

                        if (dest) {
                                *dest = value;
                        }

                        if (c == EOF) {
                                return FALSE;
                        }

			break;
		}
		case SUMMARY_TYPE_INT32: {
			gint32 value, *dest;

			if (fread (&value, sizeof (gint32), 1, summary) != 1) {
				return FALSE;
			}

			dest = va_arg (args, gint32*);

			if (dest) {
				*dest = g_ntohl (value);
			}
			break;
		}
		case SUMMARY_TYPE_UINT32: {
			guint32 *dest, value = 0;
			gint c;

			while (((c = fgetc (summary)) & 0x80) == 0 && c != EOF) {
				value |= c;
				value <<= 7;
			}

			if (c == EOF) {
				return FALSE;
			} else {
				value |= (c & 0x7f);
			}

			dest = va_arg (args, guint32*);

			if (dest) {
				*dest = value;
			}
			break;
		}
		case SUMMARY_TYPE_STRING:
		case SUMMARY_TYPE_TOKEN: {
			guint32 len;
			gchar *str, **dest;

			/* read string length */
			read_summary (summary, SUMMARY_TYPE_UINT32, &len, -1);
			dest = va_arg (args, gchar **);

			if (dest) {
				*dest = NULL;
			}

			if (value_type == SUMMARY_TYPE_TOKEN) {
				if (len < 32) {
					continue;
				} else {
					len -= 31;
				}
			}

			if (len <= 1) {
				continue;
			}

			str = g_try_malloc0 (len);
			if (!str) {
				return FALSE;
			}

			if (fread (str, len - 1, 1, summary) != 1) {
				g_free (str);
				return FALSE;
			}

			if (dest) {
				*dest = str;
			} else {
				g_free (str);
			}

			break;
		}
		default:
			break;
		}
	}

	va_end (args);

	return TRUE;
}

static gint
read_summary_header (FILE *summary)
{
	gint32 version, n_messages;

	read_summary (summary,
		      SUMMARY_TYPE_INT32, &version,
		      SUMMARY_TYPE_INT32, NULL,		/* flags */
		      SUMMARY_TYPE_INT32, NULL,		/* nextuid */
		      SUMMARY_TYPE_TIME_T, NULL,	/* time */
		      SUMMARY_TYPE_INT32, &n_messages,
		      -1);

	if ((version < 0x100 && version >= 13)) {
		read_summary (summary,
			      SUMMARY_TYPE_INT32, NULL, /* unread count*/
			      SUMMARY_TYPE_INT32, NULL, /* deleted count*/
			      SUMMARY_TYPE_INT32, NULL, /* junk count */
			      -1);
	}

	if (version != 0x30c) {
		read_summary (summary,
			      SUMMARY_TYPE_INT32, NULL,
			      SUMMARY_TYPE_INT32, NULL,
			      -1);
	}

	return n_messages;
}

static void
skip_content_info (FILE *summary)
{
	guint32 count, i;

	if (fgetc (summary)) {
		read_summary (summary,
			      SUMMARY_TYPE_TOKEN, NULL,
			      SUMMARY_TYPE_TOKEN, NULL,
			      SUMMARY_TYPE_UINT32, &count,
			      -1);

		if (count <= 500) {
			for (i = 0; i < count; i++) {
				read_summary (summary,
					      SUMMARY_TYPE_TOKEN, NULL,
					      SUMMARY_TYPE_TOKEN, NULL,
					      -1);
			}
		}

		read_summary (summary,
			      SUMMARY_TYPE_TOKEN, NULL,
			      SUMMARY_TYPE_TOKEN, NULL,
			      SUMMARY_TYPE_TOKEN, NULL,
			      SUMMARY_TYPE_UINT32, NULL,
			      -1);
	}

	read_summary (summary,
		      SUMMARY_TYPE_UINT32, &count,
		      -1);

	for (i = 0; i < count; i++) {
		skip_content_info (summary);
	}
}

static void
account_start_element_handler (GMarkupParseContext *context,
			       const gchar	   *element_name,
			       const gchar	   **attr_names,
			       const gchar	   **attr_values,
			       gpointer		   user_data,
			       GError		   **error)
{
	EvolutionAccountContext *account_context;
	gint i = 0;

	if (strcmp (element_name, "account") != 0) {
		return;
	}

	account_context = (EvolutionAccountContext *) user_data;

	while (attr_names[i]) {
		if (strcmp (attr_names[i], "uid") == 0) {
			account_context->uid = g_strdup (attr_values[i]);
			return;
		}

		i++;
	}
}

static gchar *
get_account_name_from_imap_uri (const gchar *imap_uri)
{
	const gchar *start, *at, *semic;
	gchar *user_name, *at_host_name, *account_name;

	/* Assume url schema is:
	 * imap://foo@imap.free.fr/;etc
	 * or
	 * imap://foo;auth=DIGEST-MD5@imap.bar.com/;etc
	 *
	 * We try to get "foo@imap.free.fr".
	 */

	if (!g_str_has_prefix (imap_uri, "imap://")) {
		return NULL;
	}

	user_name = at_host_name = account_name = NULL;

	/* check for embedded @ and then look for first colon after that */
	start = imap_uri + 7;
	at = strchr (start, '@');
	semic = strchr (start, ';');

	if ( strlen (imap_uri) < 7 || at == NULL ) {
		return g_strdup ("Unknown");
	}

	if (semic < at) {
		/* we have a ";auth=FOO@host" schema
		   Set semic to the next semicolon, which ends the hostname. */
		user_name = g_strndup (start, semic - start);
		/* look for ';' at the end of the domain name */
		semic = strchr (at, ';');
	} else {
		user_name = g_strndup (start, at - start);
	}

	at_host_name = g_strndup (at, (semic - 1) - at);

	account_name = g_strconcat (user_name, at_host_name, NULL);

	g_free (user_name);
	g_free (at_host_name);

	return account_name;
}

static void
account_text_handler (GMarkupParseContext  *context,
		      const gchar	   *text,
		      gsize		    text_len,
		      gpointer		    user_data,
		      GError		  **error)
{
	EvolutionAccountContext *account_context;
	const GSList *uri_element, *source_element;
	gchar *url;

	uri_element = g_markup_parse_context_get_element_stack (context);
	source_element = uri_element->next;

	if (strcmp ((gchar *) uri_element->data, "url") != 0 ||
	    !source_element ||
	    strcmp ((gchar *) source_element->data, "source") != 0) {
		return;
	}

	account_context = (EvolutionAccountContext *) user_data;

	url = g_strndup (text, text_len);
	account_context->account = get_account_name_from_imap_uri (url);
	g_free (url);
}

static void
ensure_imap_accounts (void)
{
	GConfClient *client;
	GMarkupParser parser = { 0 };
	GMarkupParseContext *parse_context;
	GSList *list, *l;
	EvolutionAccountContext account_context = { 0 };

        if (G_LIKELY (accounts)) {
                return;
        }

	client = gconf_client_get_default ();

	list = gconf_client_get_list (client,
				      "/apps/evolution/mail/accounts",
				      GCONF_VALUE_STRING,
				      NULL);

	parser.start_element = account_start_element_handler;
	parser.text = account_text_handler;
	parse_context = g_markup_parse_context_new (&parser, 0, &account_context, NULL);

	accounts = g_hash_table_new_full (g_str_hash, g_str_equal,
					  (GDestroyNotify) g_free,
					  (GDestroyNotify) g_free);

	for (l = list; l; l = l->next) {
		g_markup_parse_context_parse (parse_context, (const gchar *) l->data, -1, NULL);

		if (account_context.account &&
		    account_context.uid) {
			g_hash_table_insert (accounts,
					     account_context.account,
					     account_context.uid);
		} else {
			g_free (account_context.account);
			g_free (account_context.uid);
		}
	}

	g_markup_parse_context_free (parse_context);

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
}

static void
tracker_evolution_imap_file_initialize (TrackerModuleFile *file)
{
        TrackerEvolutionImapFile *self;
        gchar *path;

        self = TRACKER_EVOLUTION_IMAP_FILE (file);

        self->imap_dir = g_build_filename (g_get_home_dir (),
                                           ".evolution", 
					   "mail", 
					   "imap", 
					   G_DIR_SEPARATOR_S,
                                           NULL);

        path = g_file_get_path (tracker_module_file_get_file (file));
        self->summary = tracker_file_open (path, "r", TRUE);
        g_free (path);

        if (!self->summary) {
                return;
        }

        self->n_messages = read_summary_header (self->summary);
        self->cur_message = 1;

        if (self->n_messages > 0) {
                /* save current message uid */
                read_summary (self->summary,
                              SUMMARY_TYPE_STRING, &self->cur_message_uid, /* message uid */
                              -1);
        }

        ensure_imap_accounts ();
}

static const gchar *
tracker_evolution_imap_file_get_service_type (TrackerModuleFile *file)
{
        TrackerEvolutionImapFile *self;

        self = TRACKER_EVOLUTION_IMAP_FILE (file);

        if (self->current_mime_part) {
                return "EvolutionAttachments";
        }

        return "EvolutionEmails";
}

static gchar *
get_message_path (TrackerModuleFile *file,
                  const gchar       *uid)
{
	gchar *path, *prefix, *message_path;

        path = g_file_get_path (tracker_module_file_get_file (file));
	prefix = g_strndup (path, strlen (path) - strlen ("summary"));
        g_free (path);

        message_path = g_strconcat (prefix, uid, ".", NULL);
	g_free (prefix);

	return message_path;
}

static gboolean
get_attachment_info (const gchar            *mime_file,
                     gchar                 **name,
                     GMimePartEncodingType  *encoding)
{
	GMimeContentType *mime;
	gchar *tmp, *mime_content;
	const gchar *pos_content_type, *pos_encoding, *pos_end_encoding;

	if (name) {
		*name = NULL;
	}

	if (encoding) {
		*encoding = GMIME_PART_ENCODING_DEFAULT;
	}

	if (!g_file_get_contents (mime_file, &tmp, NULL, NULL)) {
		return FALSE;
	}

	/* all text content in lower case for comparisons */
	mime_content = g_ascii_strdown (tmp, -1);
	g_free (tmp);

	pos_content_type = strstr (mime_content, "content-type:");

	if (pos_content_type) {
		pos_encoding = strstr (pos_content_type, "content-transfer-encoding:");
	}

	if (!pos_content_type || !pos_encoding) {
		g_free (mime_content);
		return FALSE;
	}

	pos_content_type += strlen ("content-type:");
	pos_encoding += strlen ("content-transfer-encoding:");

	/* ignore spaces, tab or line returns */
	while (*pos_content_type != '\0' && (*pos_content_type == ' ' || *pos_content_type == '\t' || *pos_content_type == '\n')) {
		pos_content_type++;
	}

	while (*pos_encoding != '\0' && *pos_encoding == ' ') {
		pos_encoding++;
	}

	if (*pos_content_type == '\0' ||
	    *pos_encoding == '\0') {
		g_free (mime_content);
		return FALSE;
	}

	mime = g_mime_content_type_new_from_string (pos_content_type);

	if (mime) {
		if (name) {
			*name = g_strdup (g_mime_content_type_get_parameter (mime, "name"));
		}

		g_mime_content_type_destroy (mime);
	}

	if (name && !*name) {
		g_free (mime_content);
		return FALSE;
	}

	/* Find end of encoding */
	pos_end_encoding = pos_encoding;

	while (*pos_end_encoding != '\0' &&
	       *pos_end_encoding != ' ' &&
	       *pos_end_encoding != '\n' &&
	       *pos_end_encoding != '\t') {
		pos_end_encoding++;
	}

	if (encoding && pos_encoding != pos_end_encoding) {
		gchar *encoding_str = g_strndup (pos_encoding, pos_end_encoding - pos_encoding);

		if (strcmp (encoding_str, "7bit") == 0) {
			*encoding = GMIME_PART_ENCODING_7BIT;
		} else if (strcmp (encoding_str, "8bit") == 0) {
			*encoding = GMIME_PART_ENCODING_7BIT;
		} else if (strcmp (encoding_str, "binary") == 0) {
			*encoding = GMIME_PART_ENCODING_BINARY;
		} else if (strcmp (encoding_str, "base64") == 0) {
			*encoding = GMIME_PART_ENCODING_BASE64;
		} else if (strcmp (encoding_str, "quoted-printable") == 0) {
			*encoding = GMIME_PART_ENCODING_QUOTEDPRINTABLE;
		} else if (strcmp (encoding_str, "x-uuencode") == 0) {
			*encoding = GMIME_PART_ENCODING_UUENCODE;
		}

		g_free (encoding_str);
	}

	g_free (mime_content);

	return TRUE;
}

static gchar *
get_message_uri (TrackerModuleFile *file,
                 const gchar       *message_uid)
{
        TrackerEvolutionImapFile *self;
	gchar *path, *uri, *dir, *subdirs;
        GList *keys, *k;

        self = TRACKER_EVOLUTION_IMAP_FILE (file);

        path = g_file_get_path (tracker_module_file_get_file (file));
	keys = g_hash_table_get_keys (accounts);
        uri = NULL;

	for (k = keys; k; k = k->next) {
		if (!strstr (path, k->data)) {
                        continue;
                }

                dir = g_build_filename (self->imap_dir, k->data, NULL);

                /* now remove all relevant info to create the email:// basename */
                subdirs = path;
                subdirs = tracker_string_remove (subdirs, dir);
                subdirs = tracker_string_remove (subdirs, "/folders/");
                subdirs = tracker_string_remove (subdirs, "/subfolders");
                subdirs = tracker_string_remove (subdirs, "/summary");

                uri = g_strdup_printf ("email://%s/%s;uid=%s",
                                       (gchar *) g_hash_table_lookup (accounts, k->data),
                                       subdirs,
                                       message_uid);
                g_free (subdirs);
                g_free (dir);

                break;
	}

	g_list_free (keys);

        return uri;
}

static gchar *
tracker_evolution_imap_file_get_uri (TrackerModuleFile *file)
{
        TrackerEvolutionImapFile *self;
        gchar *message_uri, *part_filename;

        self = TRACKER_EVOLUTION_IMAP_FILE (file);

        if (!self->cur_message_uid) {
                return NULL;
        }

        message_uri = get_message_uri (file, self->cur_message_uid);

        if (!message_uri) {
                return NULL;
        }

        if (self->current_mime_part &&
            get_attachment_info (self->current_mime_part->data, &part_filename, NULL)) {
                gchar *attachment_uri;

                attachment_uri = g_strdup_printf ("%s/%s", message_uri, part_filename);
                g_free (message_uri);
                g_free (part_filename);

                return attachment_uri;
        }

        return message_uri;
}

static void
extract_message_text (GMimeObject *object,
                      gpointer     user_data)
{
        GString *body = (GString *) user_data;
        GMimePartEncodingType part_encoding;
        GMimePart *part;
        const gchar *content, *disposition, *filename;
        gchar *encoding, *part_body;
        gsize len;

        if (GMIME_IS_MESSAGE_PART (object)) {
		GMimeMessage *message;

		message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (object));

		if (message) {
			g_mime_message_foreach_part (message, extract_message_text, user_data);
			g_object_unref (message);
		}

		return;
        } else if (GMIME_IS_MULTIPART (object)) {
                g_mime_multipart_foreach (GMIME_MULTIPART (object), extract_message_text, user_data);
                return;
        }

	part = GMIME_PART (object);
        filename = g_mime_part_get_filename (part);
	disposition = g_mime_part_get_content_disposition (part);
        part_encoding = g_mime_part_get_encoding (part);

        if (part_encoding == GMIME_PART_ENCODING_BINARY ||
            part_encoding == GMIME_PART_ENCODING_BASE64 ||
            part_encoding == GMIME_PART_ENCODING_UUENCODE) {
                return;
        }

	if (disposition &&
	    strcmp (disposition, GMIME_DISPOSITION_ATTACHMENT) == 0) {
		return;
	}

        if (filename &&
            (strcmp (filename, "signature.asc") == 0 ||
             strcmp (filename, "signature.pgp") == 0)) {
                return;
        }

        content = g_mime_part_get_content (GMIME_PART (object), &len);

        if (!content) {
                return;
        }

        if (g_utf8_validate (content, len, NULL)) {
                g_string_append_len (body, content, (gssize) len);
                return;
        }

        encoding = evolution_common_get_object_encoding (object);

        if (!encoding) {
                /* FIXME: This will break for non-utf8 text without
                 * the proper content type set
                 */
                g_string_append_len (body, content, (gssize) len);
        } else {
                part_body = g_convert (content, (gssize) len, "utf8", encoding, NULL, NULL, NULL);
                g_string_append (body, part_body);

                g_free (part_body);
                g_free (encoding);
        }
}

static gchar *
tracker_evolution_imap_file_get_text (TrackerModuleFile *file)
{
        TrackerEvolutionImapFile *self;
	gchar *message_path;
        GMimeStream *stream;
        GMimeParser *parser;
        GMimeMessage *message;
        GString *body = NULL;

        self = TRACKER_EVOLUTION_IMAP_FILE (file);

	if (self->current_mime_part) {
		/* FIXME: Extract text from attachments */
		return NULL;
	}

	message_path = get_message_path (file, self->cur_message_uid);

#if defined(__linux__)
        stream = evolution_common_get_stream (message_path, O_RDONLY | O_NOATIME, 0);
#else
        stream = evolution_common_get_stream (message_path, O_RDONLY, 0);
#endif

	g_free (message_path);

        if (!stream) {
                return NULL;
        }

        parser = g_mime_parser_new_with_stream (stream);
        g_mime_parser_set_scan_from (parser, FALSE);
        message = g_mime_parser_construct_message (parser);

        if (message) {
                body = g_string_new (NULL);
                g_mime_message_foreach_part (message, extract_message_text, body);
                g_object_unref (message);
        }

        g_object_unref (stream);
        g_object_unref (parser);

	return (body) ? g_string_free (body, FALSE) : NULL;
}

static GList *
get_recipient_list (const gchar *str)
{
	GList *list = NULL;
	gchar **arr;
	gint i;

	if (!str) {
		return NULL;
	}

	arr = g_strsplit (str, ",", -1);

	for (i = 0; arr[i]; i++) {
		g_strstrip (arr[i]);
		list = g_list_prepend (list, g_strdup (arr[i]));
	}

	g_strfreev (arr);

	return g_list_reverse (list);
}

static TrackerModuleMetadata *
get_message_metadata (TrackerModuleFile *file)
{
        TrackerEvolutionImapFile *self;
	TrackerModuleMetadata *metadata = NULL;
	gchar *subject, *from, *to, *cc;
	gint32 i, count, flags;
	time_t t;
	GList *list, *l;
	gboolean deleted;

        self = TRACKER_EVOLUTION_IMAP_FILE (file);

	if (!read_summary (self->summary,
			   SUMMARY_TYPE_UINT32, &flags, /* flags */
			   -1)) {
		return NULL;
	}

	deleted = ((flags & EVOLUTION_MESSAGE_JUNK) != 0 ||
		   (flags & EVOLUTION_MESSAGE_DELETED) != 0);

	subject = NULL;
	from = NULL;
	to = NULL;
	cc = NULL;

	if (!read_summary (self->summary,
			   SUMMARY_TYPE_UINT32, NULL,	  /* size */
			   SUMMARY_TYPE_TIME_T, NULL,	  /* date sent */
			   SUMMARY_TYPE_TIME_T, &t,	  /* date received */
			   SUMMARY_TYPE_STRING, &subject, /* subject */
			   SUMMARY_TYPE_STRING, &from,	  /* from */
			   SUMMARY_TYPE_STRING, &to,	  /* to */
			   SUMMARY_TYPE_STRING, &cc,	  /* cc */
			   SUMMARY_TYPE_STRING, NULL,	  /* mlist */
			   -1)) {
		g_free (subject);
		g_free (from);
		g_free (to);
		g_free (cc);
		return NULL;
	}

	if (!deleted && subject && from) {
		metadata = tracker_module_metadata_new ();

		tracker_module_metadata_add_date (metadata, METADATA_EMAIL_DATE, t);
		tracker_module_metadata_add_string (metadata, METADATA_EMAIL_SENDER, from);
		tracker_module_metadata_add_string (metadata, METADATA_EMAIL_SUBJECT, subject);

		list = get_recipient_list (to);

		for (l = list; l; l = l->next) {
			tracker_module_metadata_add_string (metadata, METADATA_EMAIL_SENT_TO, l->data);
			g_free (l->data);
		}

		g_list_free (list);

		list = get_recipient_list (cc);

		for (l = list; l; l = l->next) {
			tracker_module_metadata_add_string (metadata, METADATA_EMAIL_CC, l->data);
			g_free (l->data);
		}

		g_list_free (list);
	}

	g_free (subject);
	g_free (from);
	g_free (to);
	g_free (cc);

	if (!read_summary (self->summary,
			   SUMMARY_TYPE_INT32, NULL,
			   SUMMARY_TYPE_INT32, NULL,
			   SUMMARY_TYPE_UINT32, &count,
			   -1)) {
		goto corruption;
	}

	/* references */
	for (i = 0; i < count; i++) {
		if (read_summary (self->summary,
				  SUMMARY_TYPE_INT32, NULL,
				  SUMMARY_TYPE_INT32, NULL,
				  -1)) {
			continue;
		}

		goto corruption;
	}

	if (!read_summary (self->summary, SUMMARY_TYPE_UINT32, &count, -1)) {
		goto corruption;
	}

	/* user flags */
	for (i = 0; i < count; i++) {
		if (read_summary (self->summary, SUMMARY_TYPE_STRING, NULL, -1)) {
			continue;
		}

		goto corruption;
	}

	if (!read_summary (self->summary, SUMMARY_TYPE_UINT32, &count, -1)) {
		goto corruption;
	}

	/* user tags */
	for (i = 0; i < count; i++) {
		if (read_summary (self->summary,
				  SUMMARY_TYPE_STRING, NULL,
				  SUMMARY_TYPE_STRING, NULL,
				  -1)) {
			continue;
		}

		goto corruption;
	}

	/* server flags */
	if (!read_summary (self->summary,
			   SUMMARY_TYPE_UINT32, NULL,
			   -1)) {
		goto corruption;
	}

	skip_content_info (self->summary);

	return metadata;

corruption:
	/* assume corruption */
	if (metadata) {
		g_object_unref (metadata);
	}

	return NULL;
}

static TrackerModuleMetadata *
get_attachment_metadata (TrackerModuleFile *file,
                         const gchar       *mime_file)
{
	TrackerModuleMetadata *metadata;
	GMimeStream *stream;
	GMimeDataWrapper *wrapper;
	GMimePartEncodingType encoding;
	gchar *path, *name;

	if (!get_attachment_info (mime_file, &name, &encoding)) {
		return NULL;
	}

	path = g_strdup (mime_file);
	path = tracker_string_remove (path, ".MIME");

#if defined(__linux__)
	stream = evolution_common_get_stream (path, O_RDONLY | O_NOATIME, 0);
#else
	stream = evolution_common_get_stream (path, O_RDONLY, 0);
#endif

	if (!stream) {
		g_free (name);
		g_free (path);
		return NULL;
	}

	wrapper = g_mime_data_wrapper_new_with_stream (stream, encoding);
	metadata = evolution_common_get_wrapper_metadata (wrapper);

	g_object_unref (wrapper);
	g_object_unref (stream);
	g_free (name);
	g_free (path);

	return metadata;
}

static TrackerModuleMetadata *
tracker_evolution_imap_file_get_metadata (TrackerModuleFile *file)
{
        TrackerEvolutionImapFile *self;

        self = TRACKER_EVOLUTION_IMAP_FILE (file);

	if (self->cur_message > self->n_messages) {
		return NULL;
	}

	if (self->current_mime_part) {
		return get_attachment_metadata (file, self->current_mime_part->data);
	} else {
                return get_message_metadata (file);
        }
}

static TrackerModuleFlags
tracker_evolution_imap_file_get_flags (TrackerModuleFile *file)
{
	return TRACKER_FILE_CONTENTS_STATIC;
}

static GList *
extract_mime_parts (TrackerEvolutionImapFile *self)
{
	gboolean has_attachment = TRUE;
	gint n_attachment = 0;
	gchar *message_path;
	GList *mime_parts = NULL;

	message_path = get_message_path (TRACKER_MODULE_FILE (self),
                                         self->cur_message_uid);

	while (has_attachment) {
		gchar *mime_file;

		n_attachment++;
		mime_file = g_strdup_printf ("%s%d.MIME", message_path, n_attachment);

		if (g_file_test (mime_file, G_FILE_TEST_EXISTS)) {
			mime_parts = g_list_prepend (mime_parts, mime_file);
		} else {
			g_free (mime_file);
			has_attachment = FALSE;
		}
	}

	g_free (message_path);

	return g_list_reverse (mime_parts);
}

static gboolean
tracker_evolution_imap_file_iter_contents (TrackerModuleIteratable *iteratable)
{
        TrackerEvolutionImapFile *self;

        self = TRACKER_EVOLUTION_IMAP_FILE (iteratable);

        /* Iterate through mime parts, if any */
        if (!self->mime_parts) {
                self->mime_parts = extract_mime_parts (self);
                self->current_mime_part = self->mime_parts;
        } else {
                self->current_mime_part = self->current_mime_part->next;
        }

        if (self->current_mime_part) {
                return TRUE;
        }

        g_list_foreach (self->mime_parts, (GFunc) g_free, NULL);
        g_list_free (self->mime_parts);
        self->mime_parts = NULL;

        g_free (self->cur_message_uid);
        self->cur_message_uid = NULL;

        /* save current message uid */
        read_summary (self->summary,
                      SUMMARY_TYPE_STRING, &self->cur_message_uid, /* message uid */
                      -1);

        self->cur_message++;

        return (self->cur_message < self->n_messages);
}

static guint
tracker_evolution_imap_file_get_count (TrackerModuleIteratable *iteratable)
{
        TrackerEvolutionImapFile *self;

        self = TRACKER_EVOLUTION_IMAP_FILE (iteratable);

        return self->n_messages;
}

void
tracker_evolution_imap_file_register (GTypeModule *module)
{
        tracker_evolution_imap_file_register_type (module);
}

TrackerModuleFile *
tracker_evolution_imap_file_new (GFile *file)
{
        return g_object_new (TRACKER_TYPE_EVOLUTION_IMAP_FILE,
                             "file", file,
                             NULL);
}
