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

#ifdef HAVE_INOTIFY
#   include "tracker-inotify.h"
#else
#   ifdef HAVE_FAM
#      include "tracker-fam.h"
#   endif
#endif

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
    GString *buffer;
    MailMessage *mail_message;
 } TmsParserData;

extern Tracker *tracker;


#define THUNDERBIRD_MAIL_DIR_S ".xesam/ThunderbirdEmails/ToIndex"


typedef struct {
	GSList	*mail_dirs;
} ThunderbirdPrefs;


static char	*thunderbird_mail_dir = NULL;


static ThunderbirdPrefs *	prefjs_parser			(const char *filename);
static void			free_thunderbirdprefs_struct	(ThunderbirdPrefs *prefs);
static GSList *			find_thunderbird_mboxes		(GSList *list_of_mboxes, const char *profile_dir);
static GSList *			add_inbox_mbox_of_dir		(GSList *list_of_mboxes, const char *dir);
static GSList *	add_persons_from_internet_address_list_string_parsing	(GSList *list, const gchar *s);
static gboolean email_parse_mail_tms_file_and_save_new_emails   (DBConnection *db_con, MailApplication mail_app,
                                                                    const char *path);
static MailMessage *  email_parse_mail_tms_file_by_path         (MailApplication mail_app, const char *path);
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
static          GMarkupParser                   tms_file_parser;



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
thunderbird_file_is_interesting (FileInfo *info, const char *service)
{
        //Filename should be objectX.tms (Thunderbird Message Summary)
        if (g_str_has_suffix(info->uri, ".tms")) {
            return TRUE;
        } else {
            return FALSE;
        }
}


void
thunderbird_index_file (DBConnection *db_con, FileInfo *info)
{
	g_return_if_fail (db_con && info);

        tracker_log ("Thunderbird file index: \"%s\"\n",info->uri);
        if(email_parse_mail_tms_file_and_save_new_emails (db_con, MAIL_APP_THUNDERBIRD, info->uri))
            unlink(info->uri);
}




/********************************************************************************************
 Private functions
*********************************************************************************************/

static gboolean
email_parse_mail_tms_file_and_save_new_emails (DBConnection *db_con, MailApplication mail_app, const char *path)
{
	MailMessage *mail_msg;

        g_return_val_if_fail (db_con, FALSE);
        g_return_val_if_fail (path, FALSE);
	mail_msg = email_parse_mail_tms_file_by_path (mail_app, path);

	if (!mail_msg) {
                return FALSE;
        }
        //Check if it is valid mail message
            
        if(mail_msg->parent_mail_file->mail_app == MAIL_APP_THUNDERBIRD ) {
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
email_parse_mail_tms_file_by_path (MailApplication mail_app, const char *path)
{
	MailMessage 		*mail_msg;
	gsize			length;
	gchar			*contents = NULL;
	GMarkupParseContext 	*context;
	GError 			*error = NULL;
	TmsParserData 		*parser_data;
        
        g_return_val_if_fail (path, NULL);
        
	parser_data                 = g_new0(TmsParserData,1);
        parser_data->mail_message   = email_allocate_mail_message();
	mail_msg                    = parser_data->mail_message;
        mail_msg->parent_mail_file  = g_slice_new0 (MailFile);
        
        mail_msg->parent_mail_file->path = g_strdup(path);

	if (!g_file_get_contents(path, &contents, &length, &error)) {
                tracker_log ("Error reading Thunderbird message summary %s\n", error->message);
		g_error_free(error);
		return mail_msg;
	}

	context = g_markup_parse_context_new(&tms_file_parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
					parser_data, free_parser_data );

        if (!g_markup_parse_context_parse(context, contents, length, NULL)) {
		tracker_log ("Error parsing context with length \n");
		g_markup_parse_context_free(context);
		g_free(contents);
		return mail_msg;
	}

	if (!g_markup_parse_context_end_parse(context, NULL)) {
                tracker_log ("Error parsing context \n");;
		g_markup_parse_context_free(context);
		g_free(contents);
		return mail_msg;
	}

	g_markup_parse_context_free(context);
	g_free(contents);
	return mail_msg;
}

static void
text_handler(GMarkupParseContext *context, const gchar *text,
			 gsize text_len, gpointer user_data, GError **error)
{
	TmsParserData	*data = user_data;

	if (data->buffer == NULL)
		data->buffer = g_string_new_len(text, text_len);
	else
		g_string_append_len(data->buffer, text, text_len);
}


static void
start_element_handler(GMarkupParseContext *context,
					  const gchar *element_name,
					  const gchar **attribute_names,
					  const gchar **attribute_values,
					  gpointer user_data, GError **error)
{
	TmsParserData	*data = user_data;

	if (data->buffer != NULL) {
		g_string_free(data->buffer, TRUE);
		data->buffer = NULL;
	}
        
	if (!strcmp(element_name, "MailMessage")) {
                 data->mail_message->parent_mail_file->mail_app = MAIL_APP_THUNDERBIRD;
	} else if (!strcmp(element_name, "DeleteFolder")) {
                 //data->mail_message->tms_type = TYPE_DELETE_FOLDER;
        } else if (!strcmp(element_name, "FeedItem")) {
                 data->mail_message->parent_mail_file->mail_app = MAIL_APP_THUNDERBIRD_FEED;
        } else if (!strcmp(element_name, "FolderFile")) {
                 data->tag =  TAG_FOLDER_FILE;
        } else if (!strcmp(element_name, "Author")) {
	 	 data->tag =  TAG_AUTHOR;
	} else if (!strcmp(element_name, "Date")) {
		 data->tag =  TAG_DATE;
	} else if (!strcmp(element_name, "Folder")) {
                 data->tag =  TAG_FOLDER;
        } else if (!strcmp(element_name, "FolderFile")) {
                 data->tag =  TAG_FOLDER_FILE;
        } else if (!strcmp(element_name, "HasOffline")) {
                 data->tag =  TAG_HAS_OFFLINE;
        } else if (!strcmp(element_name, "MessageId")) {
                 data->tag =  TAG_MESSAGE_ID;
        } else if (!strcmp(element_name, "MessageSize")) {
                 data->tag =  TAG_MESSAGE_SIZE;
        } else if (!strcmp(element_name, "MessageOffset")) {
                 data->tag =  TAG_MESSAGE_OFFSET;
        } else if (!strcmp(element_name, "OfflineSize")) {
                 data->tag =  TAG_OFFLINE_SIZE;
        } else if (!strcmp(element_name, "Recipients")) {
                 data->tag =  TAG_RECIPIENTS;
        } else if (!strcmp(element_name, "Subject")) {
                 data->tag =  TAG_SUBJECT;
        } else if (!strcmp(element_name, "MessageKey")) {
                 data->tag =  TAG_MESSAGE_KEY;
        } else if (!strcmp(element_name, "Uri")) {
                 data->tag =  TAG_URI;
        } else if (!strcmp(element_name, "FeedURL")) {
                 data->tag =  TAG_FEED_URL;
        } 
}


static void
end_element_handler(GMarkupParseContext *context, const gchar *element_name,
                                        gpointer user_data,  GError **error)
{
	TmsParserData	*data = user_data;
	gchar *buffer;
	MailMessage *msg;
        msg = data->mail_message;
        
	if (data->buffer == NULL)
		return;
	buffer = g_string_free(data->buffer, FALSE);
	data->buffer = NULL;
	
	if (*buffer != '\0') {
		switch(data->tag) {
			case TAG_AUTHOR:
                                msg->from = g_strdup(buffer);
				break;
			case TAG_DATE:
                                 msg->date = get_date_from_string(g_strdup(buffer));
				break;
			case TAG_FOLDER:
                                //msg->folder = g_strdup(buffer);
				break;
			case TAG_FOLDER_FILE:
                                //msg->folder_file = g_strdup(buffer);
				break;
			case TAG_HAS_OFFLINE:
                                //msg->has_offline = get_boolean_from_string(g_strdup(buffer));
				break;
			case TAG_MESSAGE_ID:
                                msg->message_id = g_strdup(buffer);
                                break;
                        case TAG_MESSAGE_SIZE:
                                //Value should be uint
                                //msg->message_size = atoi(g_strdup(buffer));
                                break;
                        case TAG_MESSAGE_OFFSET:
                                msg->offset = atoi(g_strdup(buffer));
                                break;
                        case TAG_OFFLINE_SIZE:
                                //msg->offline_size = atoi(g_strdup(buffer));
                                break;
                        case TAG_RECIPIENTS:
                                //Should be function to parse recipients
                                //
                                //g_slist_prepend(msg->to,g_strdup(buffer));
                                msg->to = add_persons_from_internet_address_list_string_parsing (NULL, g_strdup(buffer));
                                break;
                        case TAG_SUBJECT:
                                    msg->subject = g_strdup(buffer);
                                break;
                        case TAG_MESSAGE_KEY:
                                //msg->message_key = g_strdup(buffer);
                                break;
                        case TAG_URI:
                                msg->uri = g_strdup(buffer);
                                break;
                        case TAG_FEED_URL:
                                //msg->url = g_strdup(buffer);
                                break;                                
		}	
	}	

	data->tag = TAG_NONE;	
	
	g_free(buffer);	

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
    glong date;
    gchar *char_date=NULL;
    
    date = strtol(date_string,&char_date,10);
    if (*char_date)
        return 0;

    return date;
}

static gboolean
get_boolean_from_string(const gchar *boolean_string)        
{   
    if(g_str_equal(boolean_string,"true"))
        return TRUE;
    return FALSE;
    
}

static void 
free_parser_data(gpointer user_data)
{
	TmsParserData *data = user_data;
	if(data->buffer != NULL)
		g_string_free(data->buffer,TRUE);
	g_free(data);
}

static GSList *
add_persons_from_internet_address_list_string_parsing (GSList *list, const gchar *s)
{
	InternetAddressList *addrs_list, *tmp;

	g_return_val_if_fail (s, NULL);

	addrs_list = internet_address_parse_string (s);

	for (tmp = addrs_list; tmp; tmp = tmp->next) {
		MailPerson *mp;

		mp = email_allocate_mail_person ();

		mp->addr = g_strdup (tmp->address->value.addr);
                if(tmp->address->name)
                    mp->name = g_strdup (tmp->address->name);
                else
                mp->name = g_strdup (tmp->address->value.addr);
                
		list = g_slist_prepend (list, mp);
	}

	internet_address_list_destroy (addrs_list);

	return list;
}

/*
 * Currently parser is only looking for server hostnames. Here it means that we
 * only want to know folders containing emails currently used by user.
 */
static ThunderbirdPrefs *
prefjs_parser (const char *filename)
{
	ThunderbirdPrefs *prefs;
	FILE		 *f;
	const char	 *prefix;
	size_t		 len_prefix;

	g_return_val_if_fail (filename, NULL);

	prefs = g_new0 (ThunderbirdPrefs, 1);

	prefs->mail_dirs = NULL;

	prefix = "user_pref(\"mail.server.server";

	len_prefix = strlen (prefix);

	f = g_fopen (filename, "r");

	if (f) {
		char buf[512];

		while (fgets (buf, 512, f)) {

			if (g_str_has_prefix (buf, prefix)) {
				size_t	 i;
				char	 *end, *folder;

				/* at this point, we find an ID for the server:
				 *   mail.server.server.1    with 1 as ID.
				 * Complete string is something like:
				 *   "mail.server.server1.hostname", "foo.bar");
				 * so we are looking for "hostname\", \"" now...
				 */

				i = len_prefix + 1;

				/* ignore ID */
				while (buf[i] != '.') {
					if (buf[i] == ' ' || i >= 511) {
						goto error_continue;
					}
					i++;
				}

				/* we must have ".hostname\"" now */
				if (strncmp (buf + i, ".hostname\"", 10)) {
					goto error_continue;
				}

				i += 10;

				/* ignore characters until '"' */
				while (buf[i] != '"') {
					if (i >= 511) {
						goto error_continue;
					}
					i++;
				}

				/* find termination of line */
				end = strstr (buf + i + 1, "\");");

				if (!end) {
					goto error_continue;
				}

				/* and now we can read the hostname! */
				folder = g_strndup (buf + i + 1, end - (buf + i + 1));

				prefs->mail_dirs = g_slist_prepend (prefs->mail_dirs, folder);

			error_continue:
				continue;
			}
		}

		fclose (f);
	}

	return prefs;
}


static void
free_thunderbirdprefs_struct (ThunderbirdPrefs *prefs)
{
	if (!prefs) {
		return;
	}

	if (prefs->mail_dirs) {
		g_slist_foreach (prefs->mail_dirs, (GFunc) g_free, NULL);
		g_slist_free (prefs->mail_dirs);
	}

	g_free (prefs);
}


static GSList *
find_thunderbird_mboxes (GSList *list_of_mboxes, const char *profile_dir)
{
	char		 *profile;
	ThunderbirdPrefs *prefs;
	const GSList	 *mail_dir;

	g_return_val_if_fail (profile_dir, list_of_mboxes);

	profile = g_build_filename (profile_dir, "prefs.js", NULL);

	prefs = prefjs_parser (profile);

	g_free (profile);

	if (!prefs) {
		return NULL;
	}


	for (mail_dir = prefs->mail_dirs; mail_dir; mail_dir = mail_dir->next) {
		const char *tmp_dir;
		char	   *dir, *inbox, *sent, *inbox_dir;

		tmp_dir = mail_dir->data;

		dir = g_build_filename (profile_dir, "Mail", tmp_dir, NULL);

		if (tracker_file_is_no_watched (dir)) {
			g_free (dir);
			continue;
		}

		/* on first level, we should have Inbox and Sent as mboxes, so we add them if they are present */

		/* Inbox */
		inbox = g_build_filename (dir, "Inbox", NULL);

		if (!g_access (inbox, F_OK) && !g_access (inbox, R_OK)) {
			list_of_mboxes = g_slist_prepend (list_of_mboxes, inbox);
		} else {
			g_free (inbox);
		}

		/* Sent */
		sent = g_build_filename (dir, "Sent", NULL);

		if (!g_access (sent, F_OK) && !g_access (sent, R_OK)) {
			list_of_mboxes = g_slist_prepend (list_of_mboxes, sent);
		} else {
			g_free (sent);
		}

		/* now we recursively find other "Inbox" mboxes */
		inbox_dir = g_build_filename (dir, "Inbox.sbd", NULL);

		list_of_mboxes = add_inbox_mbox_of_dir (list_of_mboxes, inbox_dir);

		g_free (inbox_dir);

		g_free (dir);
	}

	free_thunderbirdprefs_struct (prefs);

	return list_of_mboxes;
}


static GSList *
add_inbox_mbox_of_dir (GSList *list_of_mboxes, const char *dir)
{
	GQueue *queue;

	g_return_val_if_fail (dir, list_of_mboxes);

	queue = g_queue_new ();

	g_queue_push_tail (queue, g_strdup (dir));

	while (!g_queue_is_empty (queue)) {
		char *m_dir;
		GDir *dirp;

		m_dir = g_queue_pop_head (queue);

		if (tracker_file_is_no_watched (m_dir)) {
			g_free (m_dir);
			continue;
		}

		if ((dirp = g_dir_open (m_dir, 0, NULL))) {
			const char *file;

			while ((file = g_dir_read_name (dirp))) {
				char *tmp_inbox, *inbox;

				if (!g_str_has_suffix (file, ".msf")) {
					continue;
				}

				/* we've found a mork file so it has a corresponding Inbox file and there
				   is perhaps a new directory to explore */

				tmp_inbox = g_strndup (file, strstr (file, ".msf") - file);

				inbox = g_build_filename (m_dir, tmp_inbox, NULL);

				g_free (tmp_inbox);

				if (inbox) {
					char *dir_to_explore;

					if (!g_access (inbox, F_OK) && !g_access (inbox, R_OK)) {
						list_of_mboxes = g_slist_prepend (list_of_mboxes, inbox);
					}

					dir_to_explore = g_strdup_printf ("%s.sbd", inbox);

					g_queue_push_tail (queue, dir_to_explore);
				}
			}

			g_dir_close (dirp);
		}

		g_free (m_dir);
	}

	g_queue_free (queue);

	return list_of_mboxes;
}
