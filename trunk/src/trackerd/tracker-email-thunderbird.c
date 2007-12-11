/* Tracker
 * routines for emails with Thunderbird
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
 * Copyright (C) 2007, Michal Pryc (Michal.Pryc@Sun.Com)
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

#include <string.h>
#include <glib/gstdio.h>
#include <stdlib.h>

#include "tracker-email-thunderbird.h"
#include "tracker-db-email.h"
#include "tracker-watch.h"


typedef enum
{
        TAG_NONE = 0,
        TAG_FOLDER_FILE,
        TAG_FEED_URL,
        TAG_AUTHOR,
        TAG_DATE,
        TAG_FOLDER,
        TAG_HAS_OFFLINE,
        TAG_MESSAGE_ID,
        TAG_MESSAGE_SIZE,
        TAG_MESSAGE_OFFSET,
        TAG_OFFLINE_SIZE,
        TAG_RECIPIENTS,
        TAG_SUBJECT,
        TAG_MESSAGE_KEY,
        TAG_URI,
} TmsParserTag;

typedef struct
{
        TmsParserTag tag;
        GString      *buffer;
        MailMessage  *mail_message;
} TmsParserData;

typedef struct {
	GSList	*mail_dirs;
} ThunderbirdPrefs;

#define THUNDERBIRD_MAIL_DIR_S ".xesam/ThunderbirdEmails/ToIndex"


extern Tracker *tracker;

static gchar            *thunderbird_mail_dir   = NULL;
static GMarkupParser    tms_file_parser;


static GSList *	add_persons_from_internet_address_list_string_parsing   (GSList *list, const gchar *s);
static gboolean email_parse_mail_tms_file_and_save_new_emails   (DBConnection *db_con, MailApplication mail_app,
                                                                 const gchar *path);
static MailMessage *  email_parse_mail_tms_file_by_path         (MailApplication mail_app, const gchar *path);
static void     text_handler                    (GMarkupParseContext *context, const gchar *text,
                                                 gsize text_len, gpointer user_data, GError **error);
static void     start_element_handler           (GMarkupParseContext *context, const gchar *element_name,
                                                 const gchar **attribute_names, const gchar **attribute_values,
                                                 gpointer user_data, GError **error);
static void     end_element_handler             (GMarkupParseContext *context, const gchar *element_name,
                                                 gpointer user_data,  GError **error);
static glong    get_date_from_string            (const gchar *date_string);
static gboolean get_boolean_from_string         (const gchar *boolean_string);
static void     free_parser_data                (gpointer user_data);




/********************************************************************************************
 Public functions
*********************************************************************************************/

gboolean
thunderbird_init_module (void)
{
	if (!thunderbird_mail_dir) {
		thunderbird_mail_dir = g_build_filename (g_get_home_dir (), THUNDERBIRD_MAIL_DIR_S, NULL);
	}

	return thunderbird_module_is_running();
}


gboolean
thunderbird_module_is_running (void)
{
	return thunderbird_mail_dir != NULL;
}


gboolean
thunderbird_finalize_module (void)
{
	if (thunderbird_mail_dir) {
		g_free (thunderbird_mail_dir);
		thunderbird_mail_dir = NULL;
	}

	return !thunderbird_module_is_running();
}


void
thunderbird_watch_emails (DBConnection *db_con)
{
        if( thunderbird_mail_dir != NULL ) {
            tracker_log("Thunderbird directory lookup: \"%s\"", thunderbird_mail_dir);
            email_watch_directory(thunderbird_mail_dir, "ThunderbirdEmails");
        }
}


gboolean
thunderbird_file_is_interesting (FileInfo *info, const gchar *service)
{
        //Filename should be objectX.tms (Thunderbird Message Summary)
        return g_str_has_suffix (info->uri, ".tms") ;
}


void
thunderbird_index_file (DBConnection *db_con, FileInfo *info)
{
	g_return_if_fail (db_con);
	g_return_if_fail (info);

        tracker_log ("Thunderbird file index: \"%s\"\n",info->uri);
        if (email_parse_mail_tms_file_and_save_new_emails (db_con, MAIL_APP_THUNDERBIRD, info->uri)) {
                unlink(info->uri);
        }
}




/********************************************************************************************
 Private functions
*********************************************************************************************/

static gboolean
email_parse_mail_tms_file_and_save_new_emails (DBConnection *db_con, MailApplication mail_app, const gchar *path)
{
	MailMessage *mail_msg;

        g_return_val_if_fail (db_con, FALSE);
        g_return_val_if_fail (path, FALSE);

	mail_msg = email_parse_mail_tms_file_by_path (mail_app, path);

	if (!mail_msg) {
                return FALSE;
        }
        //Check if it is valid mail message
            
        if (mail_msg->parent_mail_file->mail_app == MAIL_APP_THUNDERBIRD ) {
//           || mail_msg->parent_mail_file->mail_app == MAIL_APP_THUNDERBIRD_FEED) {
                tracker_db_email_save_email (db_con, mail_msg);
                email_free_mail_file(mail_msg->parent_mail_file);
                email_free_mail_message (mail_msg);
                return TRUE;
        }

        email_free_mail_file(mail_msg->parent_mail_file);
	email_free_mail_message (mail_msg);

	return FALSE;
}


static MailMessage *
email_parse_mail_tms_file_by_path (MailApplication mail_app, const gchar *path)
{
	MailMessage 		*mail_msg;
	gsize			length;
	gchar			*contents = NULL;
	GMarkupParseContext 	*context;
	GError 			*error = NULL;
	TmsParserData 		*parser_data;

        g_return_val_if_fail (path, NULL);

	parser_data                 = g_slice_new0 (TmsParserData);
        parser_data->mail_message   = email_allocate_mail_message ();
	mail_msg                    = parser_data->mail_message;
        mail_msg->parent_mail_file  = g_slice_new0 (MailFile);

        mail_msg->parent_mail_file->path = g_strdup (path);

	if (!g_file_get_contents(path, &contents, &length, &error)) {
                tracker_log ("Error reading Thunderbird message summary %s\n", error->message);
		g_error_free (error);
		return mail_msg;
	}

	context = g_markup_parse_context_new (&tms_file_parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
                                              parser_data, free_parser_data);

        if (!g_markup_parse_context_parse(context, contents, length, NULL)) {
		tracker_log ("Error parsing context with length \n");
                goto end;
	}

	if (!g_markup_parse_context_end_parse(context, NULL)) {
                tracker_log ("Error parsing context \n");
                goto end;
	}

 end:
	g_markup_parse_context_free (context);
	g_free (contents);
	return mail_msg;
}


static void
text_handler (GMarkupParseContext *context, const gchar *text,
              gsize text_len, gpointer user_data, GError **error)
{
	TmsParserData *data = user_data;

	if (data->buffer == NULL) {
		data->buffer = g_string_new_len (text, text_len);
	} else {
		g_string_append_len (data->buffer, text, text_len);
        }
}


static void
start_element_handler (GMarkupParseContext *context,
                       const gchar *element_name,
                       const gchar **attribute_names,
                       const gchar **attribute_values,
                       gpointer user_data, GError **error)
{
	TmsParserData *data = user_data;

	if (data->buffer != NULL) {
		g_string_free (data->buffer, TRUE);
		data->buffer = NULL;
	}

	if (!strcmp (element_name, "MailMessage")) {
                data->mail_message->parent_mail_file->mail_app = MAIL_APP_THUNDERBIRD;
	} else if (!strcmp (element_name, "DeleteFolder")) {
                //data->mail_message->tms_type = TYPE_DELETE_FOLDER;
        } else if (!strcmp (element_name, "FeedItem")) {
                data->mail_message->parent_mail_file->mail_app = MAIL_APP_THUNDERBIRD_FEED;
        } else if (!strcmp (element_name, "FolderFile")) {
                data->tag = TAG_FOLDER_FILE;
        } else if (!strcmp (element_name, "Author")) {
	 	data->tag =  TAG_AUTHOR;
	} else if (!strcmp (element_name, "Date")) {
		data->tag = TAG_DATE;
	} else if (!strcmp (element_name, "Folder")) {
                data->tag = TAG_FOLDER;
        } else if (!strcmp (element_name, "FolderFile")) {
                data->tag = TAG_FOLDER_FILE;
        } else if (!strcmp (element_name, "HasOffline")) {
                data->tag = TAG_HAS_OFFLINE;
        } else if (!strcmp (element_name, "MessageId")) {
                data->tag = TAG_MESSAGE_ID;
        } else if (!strcmp (element_name, "MessageSize")) {
                data->tag = TAG_MESSAGE_SIZE;
        } else if (!strcmp (element_name, "MessageOffset")) {
                data->tag = TAG_MESSAGE_OFFSET;
        } else if (!strcmp (element_name, "OfflineSize")) {
                data->tag = TAG_OFFLINE_SIZE;
        } else if (!strcmp (element_name, "Recipients")) {
                data->tag = TAG_RECIPIENTS;
        } else if (!strcmp (element_name, "Subject")) {
                data->tag = TAG_SUBJECT;
        } else if (!strcmp (element_name, "MessageKey")) {
                data->tag = TAG_MESSAGE_KEY;
        } else if (!strcmp (element_name, "Uri")) {
                data->tag = TAG_URI;
        } else if (!strcmp (element_name, "FeedURL")) {
                data->tag = TAG_FEED_URL;
        }
}


static void
end_element_handler (GMarkupParseContext *context, const gchar *element_name,
                     gpointer user_data, GError **error)
{
	TmsParserData *data = user_data;
	MailMessage   *msg = data->mail_message;
        gchar         *buffer;

	if (data->buffer == NULL) {
		return;
        }

	buffer = g_string_free (data->buffer, FALSE);
	data->buffer = NULL;
	
	if (*buffer != '\0') {
		switch (data->tag) {
			case TAG_AUTHOR:
                                msg->from = g_strdup (buffer);
				break;
			case TAG_DATE:
                                 msg->date = get_date_from_string (g_strdup (buffer));
				break;
			case TAG_FOLDER:
                                //msg->folder = g_strdup (buffer);
				break;
			case TAG_FOLDER_FILE:
                                //msg->folder_file = g_strdup (buffer);
				break;
			case TAG_HAS_OFFLINE:
                                //msg->has_offline = get_boolean_from_string (g_strdup (buffer));
				break;
			case TAG_MESSAGE_ID:
                                msg->message_id = g_strdup (buffer);
                                break;
                        case TAG_MESSAGE_SIZE:
                                //Value should be uint
                                //msg->message_size = atoi (g_strdup (buffer));
                                break;
                        case TAG_MESSAGE_OFFSET:
                                msg->offset = atoi (g_strdup (buffer));
                                break;
                        case TAG_OFFLINE_SIZE:
                                //msg->offline_size = atoi (g_strdup (buffer));
                                break;
                        case TAG_RECIPIENTS:
                                //Should be function to parse recipients
                                //
                                //g_slist_prepend (msg->to,g_strdup (buffer));
                                msg->to = add_persons_from_internet_address_list_string_parsing (NULL, g_strdup (buffer));
                                break;
                        case TAG_SUBJECT:
                                    msg->subject = g_strdup (buffer);
                                break;
                        case TAG_MESSAGE_KEY:
                                //msg->message_key = g_strdup (buffer);
                                break;
                        case TAG_URI:
                                msg->uri = g_strdup (buffer);
                                break;
                        case TAG_FEED_URL:
                                //msg->url = g_strdup (buffer);
                                break;
                        default:
                                break;
		}
	}

	data->tag = TAG_NONE;

	g_free (buffer);
}


static GMarkupParser tms_file_parser =
{
        start_element_handler,
        end_element_handler,
        text_handler,
        NULL,
        NULL
};


static glong
get_date_from_string (const gchar *date_string)
{
        gchar *char_date = NULL;
        glong date;
   
        date = strtol (date_string, &char_date, 10);

        return *char_date ? 0 : date;
}


static gboolean
get_boolean_from_string (const gchar *boolean_string)        
{   
        return g_str_equal (boolean_string, "true") ;
}


static void 
free_parser_data (gpointer user_data)
{
	TmsParserData *data = user_data;

	if (data->buffer != NULL) {
		g_string_free (data->buffer, TRUE);
        }

	g_slice_free (TmsParserData, data);
}


static GSList *
add_persons_from_internet_address_list_string_parsing (GSList *list, const gchar *s)
{
	InternetAddressList *addrs_list, *tmp;

	g_return_val_if_fail (s, NULL);

	addrs_list = internet_address_parse_string (s);

	for (tmp = addrs_list; tmp; tmp = tmp->next) {
		MailPerson *mp = email_allocate_mail_person ();

		mp->addr = g_strdup (tmp->address->value.addr);
                if(tmp->address->name) {
                        mp->name = g_strdup (tmp->address->name);
                } else {
                        mp->name = g_strdup (tmp->address->value.addr);
                }

		list = g_slist_prepend (list, mp);
	}

	internet_address_list_destroy (addrs_list);

	return list;
}
