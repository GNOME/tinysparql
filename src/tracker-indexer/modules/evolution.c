/* Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gmime/gmime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <gconf/gconf-client.h>
#include <tracker-indexer/tracker-module.h>
#include <tracker-indexer/tracker-metadata.h>
#include <tracker-indexer/tracker-metadata-utils.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-type-utils.h>

#define METADATA_FILE_PATH	     "File:Path"
#define METADATA_FILE_NAME	     "File:Name"
#define METADATA_EMAIL_RECIPIENT     "Email:Recipient"
#define METADATA_EMAIL_DATE	     "Email:Date"
#define METADATA_EMAIL_SENDER	     "Email:Sender"
#define METADATA_EMAIL_SUBJECT	     "Email:Subject"
#define METADATA_EMAIL_SENT_TO	     "Email:SentTo"
#define METADATA_EMAIL_CC	     "Email:CC"

typedef union EvolutionFileData EvolutionFileData;
typedef struct EvolutionLocalData EvolutionLocalData;
typedef struct EvolutionImapData EvolutionImapData;
typedef struct EvolutionAccountContext EvolutionAccountContext;
typedef enum MailStorageType MailStorageType;

enum MailStorageType {
	MAIL_STORAGE_NONE,
	MAIL_STORAGE_LOCAL,
	MAIL_STORAGE_IMAP
};

struct EvolutionLocalData {
	MailStorageType type;
	GMimeStream *stream;
	GMimeParser *parser;
	GMimeMessage *message;

	GList *mime_parts;
	GList *current_mime_part;
};

struct EvolutionImapData {
	MailStorageType type;
	gint fd;
	FILE *summary;
	guint n_messages;
	guint cur_message;
	gchar *cur_message_uid;

	GList *mime_parts;
	GList *current_mime_part;
};

union EvolutionFileData {
	MailStorageType type;
	EvolutionLocalData mbox;
	EvolutionImapData imap;
};

enum SummaryDataType {
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

enum EvolutionFlags {
	EVOLUTION_MESSAGE_ANSWERED     = 1 << 0,
	EVOLUTION_MESSAGE_DELETED      = 1 << 1,
	EVOLUTION_MESSAGE_DRAFT        = 1 << 2,
	EVOLUTION_MESSAGE_FLAGGED      = 1 << 3,
	EVOLUTION_MESSAGE_SEEN	       = 1 << 4,
	EVOLUTION_MESSAGE_ATTACHMENTS  = 1 << 5,
	EVOLUTION_MESSAGE_ANSWERED_ALL = 1 << 6,
	EVOLUTION_MESSAGE_JUNK	       = 1 << 7,
	EVOLUTION_MESSAGE_SECURE       = 1 << 8
};


static gchar *local_dir = NULL;
static gchar *imap_dir = NULL;
static GHashTable *accounts = NULL;


void   get_imap_accounts (void);


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
			time_t value, *dest;

			if (fread (&value, sizeof (time_t), 1, summary) != 1) {
				return FALSE;
			}

			dest = va_arg (args, time_t*);

			if (dest) {
				*dest = g_ntohl (value);
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

void
tracker_module_init (void)
{
	g_mime_init (0);
	get_imap_accounts ();

	local_dir = g_build_filename (g_get_home_dir (), ".evolution", "mail", "local", G_DIR_SEPARATOR_S, NULL);
	imap_dir = g_build_filename (g_get_home_dir (), ".evolution", "mail", "imap", G_DIR_SEPARATOR_S, NULL);
}

void
tracker_module_shutdown (void)
{
	g_mime_shutdown ();

	g_hash_table_destroy (accounts);
	g_free (local_dir);
	g_free (imap_dir);
}

G_CONST_RETURN gchar *
tracker_module_get_name (void)
{
	/* Return module name here */
	return "EvolutionEmails";
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

void
get_imap_accounts (void)
{
	GConfClient *client;
	GMarkupParser parser = { 0 };
	GMarkupParseContext *parse_context;
	GSList *list, *l;
	EvolutionAccountContext account_context = { 0 };

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

static MailStorageType
get_mail_storage_type_from_path (const gchar *path)
{
	MailStorageType type = MAIL_STORAGE_NONE;
	gchar *basename;

	basename = g_path_get_basename (path);

	if (g_str_has_prefix (path, local_dir) &&
	    strchr (basename, '.') == NULL) {
		type = MAIL_STORAGE_LOCAL;
	} else if (g_str_has_prefix (path, imap_dir) &&
		   strcmp (basename, "summary") == 0) {
		type = MAIL_STORAGE_IMAP;
	}

	/* Exclude non wanted folders */
	if (strcasestr (path, "junk") ||
	    strcasestr (path, "spam") ||
	    strcasestr (path, "trash") ||
	    strcasestr (path, "drafts") ||
	    strcasestr (path, "sent") ||
	    strcasestr (path, "outbox")) {
		type = MAIL_STORAGE_NONE;
	}

	g_free (basename);

	return type;
}

static GMimeStream *
email_get_stream (const gchar *path,
		  gint	       flags,
		  off_t        start)
{
	GMimeStream *stream;
	gint fd;

	fd = g_open (path, flags, S_IRUSR | S_IWUSR);

	if (fd == -1) {
		return NULL;
	}

	stream = g_mime_stream_fs_new_with_bounds (fd, start, -1);

	if (!stream) {
		close (fd);
	}

	return stream;
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

gpointer
tracker_module_file_get_data (const gchar *path)
{
	EvolutionFileData *data = NULL;
	MailStorageType type;

	type = get_mail_storage_type_from_path (path);

	if (type == MAIL_STORAGE_NONE) {
		return NULL;
	}

	data = g_slice_new0 (EvolutionFileData);
	data->type = type;

	if (type == MAIL_STORAGE_IMAP) {
		EvolutionImapData *imap_data;

		imap_data = (EvolutionImapData *) data;

		imap_data->fd = tracker_file_open (path, TRUE);

		if (imap_data->fd == -1) {
			return NULL;
		}

		imap_data->summary = fdopen (imap_data->fd, "r");
		imap_data->n_messages = read_summary_header (imap_data->summary);
		imap_data->cur_message = 1;

                if (imap_data->n_messages > 0) {
                        /* save current message uid */
                        read_summary (imap_data->summary,
                                      SUMMARY_TYPE_STRING, &imap_data->cur_message_uid,	/* message uid */
                                      -1);
                }
        } else {
		EvolutionLocalData *local_data;

		local_data = (EvolutionLocalData *) data;

#if defined(__linux__)
		local_data->stream = email_get_stream (path, O_RDONLY | O_NOATIME, 0);
#else
		local_data->stream = email_get_stream (path, O_RDONLY, 0);
#endif

		if (local_data->stream) {
			local_data->parser = g_mime_parser_new_with_stream (local_data->stream);
			g_mime_parser_set_scan_from (local_data->parser, TRUE);

			/* Initialize to the first message */
			local_data->message = g_mime_parser_construct_message (local_data->parser);
		}
	}

	return data;
}

static void
free_imap_data (EvolutionImapData *data)
{
	fclose (data->summary);
	close (data->fd);
}

static void
free_local_data (EvolutionLocalData *data)
{
	if (data->mime_parts) {
		g_list_foreach (data->mime_parts, (GFunc) g_object_unref, NULL);
		g_list_free (data->mime_parts);
	}

	if (data->message) {
		g_object_unref (data->message);
	}

	if (data->parser) {
		g_object_unref (data->parser);
	}

	if (data->stream) {
		g_mime_stream_close (data->stream);
		g_object_unref (data->stream);
	}
}

void
tracker_module_file_free_data (gpointer file_data)
{
	EvolutionFileData *data;

	data = (EvolutionFileData *) file_data;

	if (data->type == MAIL_STORAGE_LOCAL) {
		free_local_data ((EvolutionLocalData *) data);
	} else if (data->type == MAIL_STORAGE_IMAP) {
		free_imap_data ((EvolutionImapData *) data);
	}

	g_slice_free (EvolutionFileData, data);
}

static gint
get_mbox_message_id (GMimeMessage *message)
{
	const gchar *header, *pos;
	gchar *number;
	gint id;

	header = g_mime_message_get_header (message, "X-Evolution");
	pos = strchr (header, '-');

	number = g_strndup (header, pos - header);
	id = strtoul (number, NULL, 16);

	g_free (number);

	return id;
}

static guint
get_mbox_message_flags (GMimeMessage *message)
{
	const gchar *header, *pos;

	header = g_mime_message_get_header (message, "X-Evolution");
	pos = strchr (header, '-');

	return (guint) strtoul (pos + 1, NULL, 16);
}

static void
get_mbox_uri (TrackerFile   *file,
	      GMimeMessage  *message,
	      gchar	   **dirname,
	      gchar	   **basename)
{
	gchar *dir, *name;

	dir = tracker_string_replace (file->path, local_dir, NULL);
	dir = tracker_string_remove (dir, ".sbd");

	name = g_strdup_printf ("%s;uid=%d", dir, get_mbox_message_id (message));

	*dirname = g_strdup ("email://local@local");
	*basename = name;

	g_free (dir);
}

static void
get_mbox_attachment_uri (TrackerFile   *file,
			 GMimeMessage  *message,
			 GMimePart     *part,
			 gchar	      **dirname,
			 gchar	      **basename)
{
	gchar *dir;

	dir = tracker_string_replace (file->path, local_dir, NULL);
	dir = tracker_string_remove (dir, ".sbd");

	*dirname = g_strdup_printf ("email://local@local/%s;uid=%d",
				    dir, get_mbox_message_id (message));
	*basename = g_strdup (g_mime_part_get_filename (part));

	g_free (dir);
}

static GList *
get_mbox_recipient_list (GMimeMessage *message,
			 const gchar  *type)
{
	GList *list = NULL;
	const InternetAddressList *addresses;

	addresses = g_mime_message_get_recipients (message, type);

	while (addresses) {
		InternetAddress *address;
		gchar *str;

		address = addresses->address;

		if (address->name && address->value.addr) {
			str = g_strdup_printf ("%s %s", address->name, address->value.addr);
		} else if (address->value.addr) {
			str = g_strdup (address->value.addr);
		} else if (address->name) {
			str = g_strdup (address->name);
		} else {
			str = NULL;
		}

		if (str) {
			list = g_list_prepend (list, str);
		}

		addresses = addresses->next;
	}

	return g_list_reverse (list);
}

static TrackerMetadata *
get_metadata_for_data_wrapper (GMimeDataWrapper *wrapper)
{
	TrackerMetadata *metadata;
	GMimeStream *stream;
	gchar *path;
	gint fd;

	path = g_build_filename (g_get_tmp_dir (), "tracker-evolution-module-XXXXXX", NULL);
	fd = g_mkstemp (path);
	metadata = NULL;

	stream = g_mime_stream_fs_new (fd);

	if (g_mime_data_wrapper_write_to_stream (wrapper, stream) != -1) {
		g_mime_stream_flush (stream);

		metadata = tracker_metadata_utils_get_data (path);
		g_unlink (path);
	}

	g_mime_stream_close (stream);
	g_object_unref (stream);
	g_free (path);

	return metadata;
}

static TrackerMetadata *
get_metadata_for_mbox_attachment (TrackerFile  *file,
				  GMimeMessage *message,
				  GMimePart    *part)
{
	TrackerMetadata *metadata;
	GMimeDataWrapper *content;

	content = g_mime_part_get_content_object (part);

	if (!content) {
		return NULL;
	}

	metadata = get_metadata_for_data_wrapper (content);

	if (metadata) {
		gchar *dirname, *basename;

		get_mbox_attachment_uri (file, message, part,
					 &dirname, &basename);

		tracker_metadata_insert (metadata, METADATA_FILE_PATH, dirname);
		tracker_metadata_insert (metadata, METADATA_FILE_NAME, basename);
	}

	g_object_unref (content);

	return metadata;
}

static TrackerMetadata *
get_metadata_for_mbox (TrackerFile *file)
{
	EvolutionLocalData *data;
	GMimeMessage *message;
	TrackerMetadata *metadata;
	gchar *dirname, *basename;
	time_t date;
	GList *list;
	guint flags;

	data = file->data;
	message = data->message;

	if (!message) {
		return NULL;
	}

	flags = get_mbox_message_flags (message);

	if (flags & EVOLUTION_MESSAGE_JUNK ||
	    flags & EVOLUTION_MESSAGE_DELETED) {
		return NULL;
	}

	if (data->current_mime_part) {
		/* We're processing an attachment */
		return get_metadata_for_mbox_attachment (file, message, data->current_mime_part->data);
	}

	metadata = tracker_metadata_new ();

	get_mbox_uri (file, message, &dirname, &basename);
	tracker_metadata_insert (metadata, METADATA_FILE_PATH, dirname);
	tracker_metadata_insert (metadata, METADATA_FILE_NAME, basename);

	g_mime_message_get_date (message, &date, NULL);
	tracker_metadata_insert (metadata, METADATA_EMAIL_DATE,
				 tracker_guint_to_string (date));

	tracker_metadata_insert (metadata, METADATA_EMAIL_SENDER,
				 g_strdup (g_mime_message_get_sender (message)));
	tracker_metadata_insert (metadata, METADATA_EMAIL_SUBJECT,
				 g_strdup (g_mime_message_get_subject (message)));

	list = get_mbox_recipient_list (message, GMIME_RECIPIENT_TYPE_TO);
	tracker_metadata_insert_multiple_values (metadata, METADATA_EMAIL_SENT_TO, list);

	list = get_mbox_recipient_list (message, GMIME_RECIPIENT_TYPE_CC);
	tracker_metadata_insert_multiple_values (metadata, METADATA_EMAIL_CC, list);

	return metadata;
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

static gboolean
get_imap_attachment_info (const gchar		 *mime_file,
			  gchar			**name,
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

static void
get_imap_uri (TrackerFile  *file,
	      const gchar  *uid,
	      gchar	  **uri_base,
	      gchar	  **basename)
{
	GList *keys, *k;
	gchar *path, *dir, *subdirs;

	path = file->path;
	keys = g_hash_table_get_keys (accounts);
	*uri_base = *basename = NULL;

	for (k = keys; k; k = k->next) {
		if (strstr (path, k->data)) {
			*uri_base = g_strdup_printf ("email://%s", (gchar *) g_hash_table_lookup (accounts, k->data));

			dir = g_build_filename (imap_dir, k->data, NULL);

			/* now remove all relevant info to create the email:// basename */
			subdirs = g_strdup (path);
			subdirs = tracker_string_remove (subdirs, dir);
			subdirs = tracker_string_remove (subdirs, "/folders");
			subdirs = tracker_string_remove (subdirs, "/subfolders");
			subdirs = tracker_string_remove (subdirs, "/summary");

			*basename = g_strdup_printf ("%s;uid=%s", subdirs, uid);

			g_free (subdirs);
			g_free (dir);

			break;
		}
	}

	g_list_free (keys);

	return;
}

static void
get_imap_attachment_uri (TrackerFile  *file,
			 gchar	     **dirname,
			 gchar	     **basename)
{
	EvolutionImapData *data;
	gchar *message_dirname, *message_basename, *name;

	data = file->data;

	if (!get_imap_attachment_info (data->current_mime_part->data, &name, NULL)) {
		return;
	}

	get_imap_uri (file, data->cur_message_uid, &message_dirname, &message_basename);
	*dirname = g_strdup_printf ("%s/%s", message_dirname, message_basename);
	*basename = name;

	g_free (message_dirname);
	g_free (message_basename);
}

static GList *
get_imap_recipient_list (const gchar *str)
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

static gchar *
get_imap_message_path (TrackerFile *file,
		       const gchar *uid)
{
	gchar *prefix, *message_path;

	prefix = g_strndup (file->path, strlen (file->path) - strlen ("summary"));
	message_path = g_strconcat (prefix, uid, ".", NULL);
	g_free (prefix);

	return message_path;
}

static TrackerMetadata *
get_metadata_for_imap_attachment (TrackerFile *file,
				  const gchar *mime_file)
{
	TrackerMetadata *metadata;
	GMimeStream *stream;
	GMimeDataWrapper *wrapper;
	GMimePartEncodingType encoding;
	gchar *path, *name;

	if (!get_imap_attachment_info (mime_file, &name, &encoding)) {
		return NULL;
	}

	path = g_strdup (mime_file);
	path = tracker_string_remove (path, ".MIME");

#if defined(__linux__)
	stream = email_get_stream (path, O_RDONLY | O_NOATIME, 0);
#else
	stream = email_get_stream (path, O_RDONLY, 0);
#endif

	if (!stream) {
		g_free (name);
		g_free (path);
		return NULL;
	}

	wrapper = g_mime_data_wrapper_new_with_stream (stream, encoding);
	metadata = get_metadata_for_data_wrapper (wrapper);

	if (metadata) {
		EvolutionImapData *data;
		gchar *dirname, *basename;

		data = file->data;

		get_imap_uri (file,
			      data->cur_message_uid,
			      &dirname, &basename);

		tracker_metadata_insert (metadata, METADATA_FILE_NAME, g_strdup (name));
		tracker_metadata_insert (metadata, METADATA_FILE_PATH,
					 g_strdup_printf ("%s/%s", dirname, basename));

		g_free (dirname);
		g_free (basename);
	}

	g_object_unref (wrapper);
	g_object_unref (stream);
	g_free (name);
	g_free (path);

	return metadata;
}

static TrackerMetadata *
get_metadata_for_imap (TrackerFile *file)
{
	EvolutionImapData *data;
	TrackerMetadata *metadata = NULL;
	gchar *dirname, *basename;
	gchar *subject, *from, *to, *cc;
	gint32 i, count, flags;
	time_t date;
	GList *list;
	gboolean deleted;

	data = file->data;

	if (data->cur_message > data->n_messages) {
		return NULL;
	}

	if (data->current_mime_part) {
		return get_metadata_for_imap_attachment (file, data->current_mime_part->data);
	}

	if (!read_summary (data->summary,
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

	if (!read_summary (data->summary,
			   SUMMARY_TYPE_UINT32, NULL,	  /* size */
			   SUMMARY_TYPE_TIME_T, NULL,	  /* date sent */
			   SUMMARY_TYPE_TIME_T, &date,	  /* date received */
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

	if (!deleted) {
		metadata = tracker_metadata_new ();
		get_imap_uri (file, data->cur_message_uid, &dirname, &basename);

		tracker_metadata_insert (metadata, METADATA_FILE_PATH, dirname);
		tracker_metadata_insert (metadata, METADATA_FILE_NAME, basename);

		tracker_metadata_insert (metadata, METADATA_EMAIL_DATE,
					 tracker_guint_to_string (date));

		tracker_metadata_insert (metadata, METADATA_EMAIL_SENDER, from);
		tracker_metadata_insert (metadata, METADATA_EMAIL_SUBJECT, subject);

		list = get_imap_recipient_list (to);
		tracker_metadata_insert_multiple_values (metadata, METADATA_EMAIL_SENT_TO, list);

		list = get_imap_recipient_list (cc);
		tracker_metadata_insert_multiple_values (metadata, METADATA_EMAIL_CC, list);
	}

	g_free (to);
	g_free (cc);

	if (!read_summary (data->summary,
			   SUMMARY_TYPE_INT32, NULL,
			   SUMMARY_TYPE_INT32, NULL,
			   SUMMARY_TYPE_UINT32, &count,
			   -1)) {
		goto corruption;
	}

	/* references */
	for (i = 0; i < count; i++) {
		if (read_summary (data->summary,
				  SUMMARY_TYPE_INT32, NULL,
				  SUMMARY_TYPE_INT32, NULL,
				  -1)) {
			continue;
		}

		goto corruption;
	}

	if (!read_summary (data->summary, SUMMARY_TYPE_UINT32, &count, -1)) {
		goto corruption;
	}

	/* user flags */
	for (i = 0; i < count; i++) {
		if (read_summary (data->summary, SUMMARY_TYPE_STRING, NULL, -1)) {
			continue;
		}

		goto corruption;
	}

	if (!read_summary (data->summary, SUMMARY_TYPE_UINT32, &count, -1)) {
		goto corruption;
	}

	/* user tags */
	for (i = 0; i < count; i++) {
		if (read_summary (data->summary,
				  SUMMARY_TYPE_STRING, NULL,
				  SUMMARY_TYPE_STRING, NULL,
				  -1)) {
			continue;
		}

		goto corruption;
	}

	/* server flags */
	if (!read_summary (data->summary,
			   SUMMARY_TYPE_UINT32, NULL,
			   -1)) {
		goto corruption;
	}

	skip_content_info (data->summary);

	return metadata;

corruption:
	/* assume corruption */
	if (metadata) {
		tracker_metadata_free (metadata);
	}

	return NULL;
}

void
tracker_module_file_get_uri (TrackerFile  *file,
			     gchar	 **dirname,
			     gchar	 **basename)
{
	EvolutionFileData *file_data;

	file_data = file->data;

	if (!file_data) {
		/* It isn't any of the files the
		 * module is interested for */
		return;
	}

	switch (file_data->type) {
	case MAIL_STORAGE_LOCAL: {
		EvolutionLocalData *data = file->data;

		if (!data->message) {
			return;
		}

		if (data->current_mime_part) {
			/* We're currently on an attachment */
			get_mbox_attachment_uri (file, data->message,
						 data->current_mime_part->data,
						 dirname, basename);
		} else {
			get_mbox_uri (file, data->message, dirname, basename);
		}

		break;
	}
	case MAIL_STORAGE_IMAP: {
		EvolutionImapData *data = file->data;

		if (data->current_mime_part) {
			/* We're currently on an attachment */
			get_imap_attachment_uri (file, dirname, basename);
		} else {
			get_imap_uri (file, data->cur_message_uid, dirname, basename);
		}

		break;
	}
	default:
		break;
	}
}

gchar *
tracker_module_file_get_service_type (TrackerFile *file)
{
	EvolutionFileData *data;

	data = file->data;

	if (!data) {
		/* It isn't any of the files the module handles */
		return NULL;
	}

	if (data->type == MAIL_STORAGE_LOCAL) {
		EvolutionLocalData *local_data = file->data;

		if (local_data->current_mime_part) {
			return g_strdup ("EvolutionAttachments");
		}
	} else if (data->type == MAIL_STORAGE_IMAP) {
		EvolutionImapData *imap_data = file->data;

		if (imap_data->current_mime_part) {
			return g_strdup ("EvolutionAttachments");
		}
	}

	return g_strdup ("EvolutionEmails");
}

TrackerMetadata *
tracker_module_file_get_metadata (TrackerFile *file)
{
	EvolutionFileData *data;

	data = file->data;

	if (!data) {
		/* It isn't any of the files the
		 * module is interested for */
		return NULL;
	}

	switch (data->type) {
	case MAIL_STORAGE_LOCAL:
		return get_metadata_for_mbox (file);
	case MAIL_STORAGE_IMAP:
		return get_metadata_for_imap (file);
	default:
		break;
	}

	return NULL;
}

static gchar *
get_message_encoding (GMimeMessage *message)
{
        const gchar *content_type, *start_encoding, *end_encoding;

        content_type = g_mime_message_get_header (message, "Content-Type");

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

static gchar *
get_text_for_mbox (TrackerFile *file)
{
        EvolutionLocalData *data;
        gboolean is_html;
        gchar *text, *encoding, *utf8_text;

	data = file->data;

	if (data->current_mime_part) {
		/* FIXME: Extract text from attachments */
		return NULL;
	}

        text = g_mime_message_get_body (data->message, TRUE, &is_html);

        if (!text) {
                return NULL;
        }

        encoding = get_message_encoding (data->message);

        if (!encoding) {
                /* FIXME: could still puke on non-utf8
                 * messages without proper content type
                 */
                return text;
        }

        utf8_text = g_convert (text, -1, "utf8", encoding, NULL, NULL, NULL);

        g_free (encoding);
        g_free (text);

        return utf8_text;
}

static gchar *
get_text_for_imap (TrackerFile *file)
{
	EvolutionImapData *data;
	gchar *message_path;
	gchar *body = NULL;

	data = file->data;

	if (data->current_mime_part) {
		/* FIXME: Extract text from attachments */
		return NULL;
	}

	message_path = get_imap_message_path (file, data->cur_message_uid);
	g_file_get_contents (message_path, &body, NULL, NULL);
	g_free (message_path);

	return body;
}

gchar *
tracker_module_file_get_text (TrackerFile *file)
{
	EvolutionFileData *data;
	gchar *text = NULL;

	data = file->data;

	if (!data) {
		/* It isn't any of the files the
		 * module is interested in */
		return NULL;
	}

	if (data->type == MAIL_STORAGE_LOCAL) {
		text = get_text_for_mbox (file);
	} else if (data->type == MAIL_STORAGE_IMAP) {
		text = get_text_for_imap (file);
	}

	return text;
}

static GList *
extract_imap_mime_parts (TrackerFile *file)
{
	EvolutionImapData *data;
	gboolean has_attachment = TRUE;
	gint n_attachment = 0;
	gchar *message_path;
	GList *mime_parts = NULL;

	data = file->data;
	message_path = get_imap_message_path (file, data->cur_message_uid);

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

	return g_list_reverse (mime_parts);;
}

static void
extract_mbox_mime_parts (GMimeObject *object,
			 gpointer     user_data)
{
	GList **list = (GList **) user_data;
	GMimePart *part;
	const gchar *disposition, *filename;

	if (GMIME_IS_MESSAGE_PART (object)) {
		GMimeMessage *message;

		message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (object));

		if (message) {
			g_mime_message_foreach_part (message, extract_mbox_mime_parts, user_data);
			g_object_unref (message);
		}

		return;
	} else if (GMIME_IS_MULTIPART (object)) {
		g_mime_multipart_foreach (GMIME_MULTIPART (object), extract_mbox_mime_parts, user_data);
		return;
	}

	part = GMIME_PART (object);
	disposition = g_mime_part_get_content_disposition (part);

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

gboolean
tracker_module_file_iter_contents (TrackerFile *file)
{
	EvolutionFileData *data;

	data = file->data;

	if (data->type == MAIL_STORAGE_IMAP) {
		EvolutionImapData *imap_data = file->data;

		/* Iterate through mime parts, if any */
		if (!imap_data->mime_parts) {
			imap_data->mime_parts = extract_imap_mime_parts (file);
			imap_data->current_mime_part = imap_data->mime_parts;
		} else {
			imap_data->current_mime_part = imap_data->current_mime_part->next;
		}

		if (imap_data->current_mime_part) {
			return TRUE;
		}

		g_list_foreach (imap_data->mime_parts, (GFunc) g_free, NULL);
		g_list_free (imap_data->mime_parts);
		imap_data->mime_parts = NULL;

		g_free (imap_data->cur_message_uid);
		imap_data->cur_message_uid = NULL;

                /* save current message uid */
                read_summary (imap_data->summary,
                              SUMMARY_TYPE_STRING, &imap_data->cur_message_uid,	/* message uid */
                              -1);

		imap_data->cur_message++;

		return (imap_data->cur_message < imap_data->n_messages);
	} else if (data->type == MAIL_STORAGE_LOCAL) {
		EvolutionLocalData *local_data = file->data;

		if (!local_data->parser) {
			return FALSE;
		}

		if (local_data->message) {
			/* Iterate through mime parts, if any */
			if (!local_data->mime_parts) {
				g_mime_message_foreach_part (local_data->message,
							     extract_mbox_mime_parts,
							     &local_data->mime_parts);
				local_data->current_mime_part = local_data->mime_parts;
			} else {
				local_data->current_mime_part = local_data->current_mime_part->next;
			}

			if (local_data->current_mime_part) {
				return TRUE;
			}

			/* all possible mime parts have been already iterated, move on */
			g_object_unref (local_data->message);

			g_list_foreach (local_data->mime_parts, (GFunc) g_object_unref, NULL);
			g_list_free (local_data->mime_parts);
			local_data->mime_parts = NULL;
		}

		local_data->message = g_mime_parser_construct_message (local_data->parser);

		return (local_data->message != NULL);
	}

	return FALSE;
}
