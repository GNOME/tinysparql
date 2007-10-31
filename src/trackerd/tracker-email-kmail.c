/* Tracker
 * routines for emails with KMail
 * Copyright (C) 2006, 2007, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glib/gstdio.h>

#include "tracker-email-kmail.h"
#include "tracker-email-utils.h"
#include "tracker-db-email.h"
#include "tracker-watch.h"


typedef struct {
        gchar            *imap_path;
        gchar            *file;
} FileAndImapPaths;

typedef enum {
        KMAIL_MAIL_PROTOCOL_UNKNOWN,
        KMAIL_MAIL_PROTOCOL_MBOX,
        KMAIL_MAIL_PROTOCOL_IMAP,
        KMAIL_MAIL_PROTOCOL_CACHED_IMAP
} KMailMailProtocol;

typedef struct {
        gchar            *name;
        gchar            *login;
        gchar            *host;
        gchar            *type;
        gchar            *location;              /* Currently only useful for maildir */
        FileAndImapPaths **file_and_imap_paths;	 /* only for IMAP
                                                    each entry has been sort by its imap_path length */
} KMailAccount;

typedef struct {
        gchar            *kmailrc_path;
        gchar            *local_dir;
        gchar            *cache_dir;
        gchar            *imap_cache;
        gchar            *dimap_cache;
        GSList           *accounts;
} KMailConfig;


extern Tracker          *tracker;

static KMailConfig      *kmail_config = NULL;


static gboolean         load_kmail_config               (KMailConfig **conf);
static void             free_kmail_config               (KMailConfig *conf);
static FileAndImapPaths ** find_dir_and_imap_path_pairs (GKeyFile *key_file, gchar **groups, const gchar *imap_id);
static void             free_file_and_imap_path_pair    (FileAndImapPaths *pair);
static void             free_kmail_account              (KMailAccount *account);
static GSList *         get_dirs_to_watch               (DBConnection *db_con, const gchar *dir_path, gboolean in_imap_dir);
static gboolean         ignore_email                    (const gchar *uri);
static KMailMailProtocol find_mail_protocol             (const gchar *mail_path);
static gboolean         index_mail_file_in_maildir_dir  (DBConnection *db_con, const gchar *dir, FileInfo *info, MailType mail_type);
//static void           watch_imap_cache                (DBConnection *db_con, const gchar *imap_dir_path);
//static void           watch_local_maildir_dir         (DBConnection *db_con, const gchar *dir_path);

static void             load_uri_of_mbox_mail_message   (GMimeMessage *g_m_message, MailMessage *mail_msg, gpointer user_data);
static gchar *          forward_uri_for_mbox_email      (MailMessage *mail_msg, gpointer user_data);
static void             fill_uri_with_uid_for_imap      (GMimeMessage *g_m_message, MailMessage *mail_msg, gpointer user_data);
static gchar *          make_imap_email_uri             (MailMessage *mail_msg, gpointer user_data);

static gchar *          expand_string                   (const gchar *s);




/********************************************************************************************
 Public functions
*********************************************************************************************/

gboolean
kmail_init_module (void)
{
        KMailConfig *conf;

        if (kmail_config) {
                return TRUE;
        }

        conf = NULL;

        if (load_kmail_config (&conf)) {
                kmail_config = conf;
        }

        return kmail_module_is_running ();
}


gboolean
kmail_module_is_running (void)
{
        return kmail_config != NULL;
}


gboolean
kmail_finalize_module (void)
{
        if (!kmail_config) {
                return TRUE;
        }

        free_kmail_config (kmail_config);
        kmail_config = NULL;

        return !kmail_module_is_running ();
}


void
kmail_watch_emails (DBConnection *db_con)
{
        g_return_if_fail (kmail_config);

        #define WATCH_DIRS(root_dir, in_imap_dir)                                       \
        {                                                                               \
                GSList *dirs = get_dirs_to_watch (db_con, root_dir, in_imap_dir);       \
                email_watch_directories (dirs, "KMailEmails");                          \
                g_slist_foreach (dirs, (GFunc) g_free, NULL);                           \
                g_slist_free (dirs);                                                    \
        }

        WATCH_DIRS (kmail_config->local_dir, FALSE);
        WATCH_DIRS (kmail_config->imap_cache, TRUE);
        WATCH_DIRS (kmail_config->dimap_cache, TRUE);

        #undef WATCH_DIRS
}


gboolean
kmail_file_is_interesting (FileInfo *info, const gchar *service)
{
        const GSList *account;

        g_return_val_if_fail (info, FALSE);
        g_return_val_if_fail (kmail_config, FALSE);

        /* We want to take any file that KMail may handle. */
        if (g_str_has_prefix (info->uri, kmail_config->cache_dir) ||
            g_str_has_prefix (info->uri, kmail_config->local_dir)) {
                return TRUE;
        }

        for (account = kmail_config->accounts; account; account = account->next) {
                const KMailAccount *acc = account->data;

                if (acc->location && g_str_has_prefix (info->uri, acc->location)) {
                        return TRUE;
                }
        }

        return FALSE;
}


void
kmail_index_file (DBConnection *db_con, FileInfo *info)
{
        KMailMailProtocol mail_protocol;

        g_return_if_fail (db_con);
        g_return_if_fail (info);

        mail_protocol = find_mail_protocol (info->uri);

        switch (mail_protocol) {
                case KMAIL_MAIL_PROTOCOL_MBOX:
                case KMAIL_MAIL_PROTOCOL_IMAP: {
                        if (email_is_in_a_maildir_dir (info->uri)) {
                                /* check directory is registered */
                                gchar *dir = g_path_get_dirname (info->uri);
                                index_mail_file_in_maildir_dir (db_con, dir, info,
                                                                (mail_protocol == KMAIL_MAIL_PROTOCOL_MBOX ? MAIL_TYPE_MBOX : MAIL_TYPE_IMAP));
                                g_free (dir);

                        } else {
                                if (ignore_email (info->uri)) {
                                        break;
                                }

                                /* Directory can contain a file named something like %2BAH4 which acts as an index for emails.
                                   We ignore this type of file. */
                                if (g_strrstr (info->uri, "%")) {
                                        break;
                                }

                                /* check mbox is registered */
                                if (tracker_db_email_get_mbox_id (db_con, info->uri) == -1) {
                                        gchar *mbox_name = g_path_get_basename (info->uri);
                                        gchar *uri_prefix = g_path_get_dirname (info->uri);

                                        tracker_db_email_register_mbox (db_con, MAIL_APP_KMAIL,
                                                                        (mail_protocol == KMAIL_MAIL_PROTOCOL_MBOX ? MAIL_TYPE_MBOX : MAIL_TYPE_IMAP),
                                                                        info->uri, mbox_name, uri_prefix);

                                        g_free (mbox_name);
                                        g_free (uri_prefix);
                                }

                                MailStore *store = tracker_db_email_get_mbox_details (db_con, info->uri);
                                if (!store) {
                                        tracker_error ("ERROR: could not retrieve store for file %s", info->uri);
                                        return;
                                }

                                if (mail_protocol == KMAIL_MAIL_PROTOCOL_MBOX) {
                                        email_parse_mail_file_and_save_new_emails (db_con, MAIL_APP_KMAIL, info->uri,
                                                                                   load_uri_of_mbox_mail_message, NULL,
                                                                                   forward_uri_for_mbox_email, NULL,
                                                                                   store);
                                } else {
                                        email_parse_mail_file_and_save_new_emails (db_con, MAIL_APP_KMAIL, info->uri,
                                                                                   fill_uri_with_uid_for_imap, NULL,
                                                                                   make_imap_email_uri, kmail_config->accounts,
                                                                                   store);

                                }
                                tracker_db_email_free_mail_store (store);
                        }
                        break;
                }
                case KMAIL_MAIL_PROTOCOL_CACHED_IMAP: {
                        gchar *dir = g_path_get_dirname (info->uri) ;
                        //tracker_db_index_file (db_con, info, NULL, NULL);
                        index_mail_file_in_maildir_dir (db_con, dir, info, MAIL_TYPE_IMAP);
                        g_free (dir);
                        break;
                }
                case KMAIL_MAIL_PROTOCOL_UNKNOWN: {
                        break;
                }
        }
}



/********************************************************************************************
 Private functions
*********************************************************************************************/

static gboolean
load_kmail_config (KMailConfig **conf)
{
        KMailConfig *m_conf;

        if (*conf) {
                free_kmail_config (*conf);
        }

        *conf = g_slice_new0 (KMailConfig);
        m_conf = *conf;

        m_conf->kmailrc_path = g_build_filename (g_get_home_dir (), ".kde/share/config/kmailrc", NULL);
        m_conf->cache_dir = g_build_filename (g_get_home_dir (), ".kde/share/apps/kmail", NULL);
        m_conf->imap_cache = g_build_filename (m_conf->cache_dir, "imap", NULL);
        m_conf->dimap_cache = g_build_filename (m_conf->cache_dir, "dimap", NULL);

        if (g_file_test (m_conf->kmailrc_path, G_FILE_TEST_EXISTS)) {
                GKeyFile *key_file;
                gchar    *tmp_local_dir, *str_nb_accounts;
                guint    i, nb_accounts;
                gchar    **groups;

                key_file = g_key_file_new ();

                if (!g_key_file_load_from_file (key_file, m_conf->kmailrc_path, G_KEY_FILE_NONE, NULL)) {
                        goto error;
                }

                if (!g_key_file_has_group (key_file, "General")) {
                        g_key_file_free (key_file);
                        goto error;
                }

                tmp_local_dir = g_key_file_get_string (key_file, "General", "folders", NULL);

                if (tmp_local_dir) {
                        m_conf->local_dir = expand_string (tmp_local_dir);
                        g_free (tmp_local_dir);

                } else {
                        g_key_file_free (key_file);
                        goto error;
                }

                str_nb_accounts = g_key_file_get_string (key_file, "General", "accounts", NULL);

                if (!str_nb_accounts) {
                        g_key_file_free (key_file);
                        goto error;
                }

                if (!tracker_str_to_uint (str_nb_accounts, &nb_accounts)) {
                        g_free (str_nb_accounts);
                        g_key_file_free (key_file);
                        goto error;
                }

                g_free (str_nb_accounts);

                groups = g_key_file_get_groups (key_file, NULL);

                for (i = 1; i <= nb_accounts; i++) {
                        gchar        *account_group;
                        KMailAccount *account;

                        account_group = g_strdup_printf ("Account %d", i);

                        if (!g_key_file_has_group (key_file, account_group)) {
                                g_free (account_group);
                                continue;
                        }

                        account = g_slice_new0 (KMailAccount);
                        account->name = g_key_file_get_string (key_file, account_group, "Name", NULL);
                        account->login = g_key_file_get_string (key_file, account_group, "login", NULL);
                        account->host = g_key_file_get_string (key_file, account_group, "host", NULL);
                        account->type = g_key_file_get_string (key_file, account_group, "Type", NULL);

                        if (strcmp (account->type, "imap") == 0) {
                                gchar *account_id = g_key_file_get_string (key_file, account_group, "Id", NULL);
                                if (account_id) {
                                        account->file_and_imap_paths = find_dir_and_imap_path_pairs (key_file, groups, account_id);
                                        g_free (account_id);
                                }

                        } else if (strcmp (account->type, "cachedimap") == 0) {
                                gchar *account_id = g_key_file_get_string (key_file, account_group, "Id", NULL);
                                if (account_id) {
                                        account->file_and_imap_paths = find_dir_and_imap_path_pairs (key_file, groups, account_id);
                                        g_free (account_id);
                                }

                        } else if (strcmp (account->type, "maildir") == 0) {
                                gchar *location = g_key_file_get_string (key_file, account_group, "Location", NULL);
                                if (location) {
                                        account->location = expand_string (location);
                                        g_free (location);
                                }
                        }

                        g_free (account_group);

                        m_conf->accounts = g_slist_prepend (m_conf->accounts, account);
                }

                g_strfreev (groups);
                g_key_file_free (key_file);
        }

        return TRUE;

 error:
        tracker_error ("ERROR: file \"%s\" cannot be loaded", m_conf->kmailrc_path);
        free_kmail_config (*conf);
        *conf = NULL;
        return FALSE;
}


static void
free_kmail_config (KMailConfig *conf)
{
        if (!conf) {
                return;
        }

        if (conf->kmailrc_path) {
                g_free (conf->kmailrc_path);
        }

        if (conf->local_dir) {
                g_free (conf->local_dir);
        }

        if (conf->cache_dir) {
                g_free (conf->cache_dir);
        }

        if (conf->imap_cache) {
                g_free (conf->imap_cache);
        }

        if (conf->dimap_cache) {
                g_free (conf->dimap_cache);
        }

        g_slist_foreach (conf->accounts, (GFunc) free_kmail_account, NULL);

        g_slice_free (KMailConfig, conf);
}


static gint
compare_pairs (gconstpointer a, gconstpointer b)
{
        const FileAndImapPaths *pa, *pb;

        pa = a;
        pb = b;

        /* we want longest names first */
        return - (strlen (pa->imap_path) - strlen (pb->imap_path));
}


static FileAndImapPaths **
find_dir_and_imap_path_pairs (GKeyFile *key_file, gchar **groups, const gchar *imap_id)
{
        gchar            **group;
        gchar            *str_group;
        GArray           *pairs;
        FileAndImapPaths **ret;
        guint            i;

        if (!groups) {
                return NULL;
        }

        pairs = g_array_new (TRUE, FALSE, sizeof (FileAndImapPaths *));

        str_group = g_strconcat ("Folder-.", imap_id, ".directory", NULL);

        for (group = groups; *group; group++) {
                if (g_str_has_prefix (*group, str_group)) {
                        FileAndImapPaths *pair = g_slice_new0 (FileAndImapPaths);

                        pair->imap_path = g_key_file_get_string (key_file, *group, "ImapPath", NULL);
                        pair->file = g_strdup (strstr (*group, "-.") + 2);

                        if (!pair->imap_path || !pair->file) {
                                free_file_and_imap_path_pair (pair);

                        } else {
                                g_array_append_val (pairs, pair);
                        }
                }
        }

        g_array_sort (pairs, compare_pairs);

        ret = g_new0 (FileAndImapPaths *, pairs->len + 1);
        for (i = 0; i < pairs->len; i++) {
                ret[i] = g_array_index (pairs, FileAndImapPaths *, i);
        }

        g_array_free (pairs, TRUE);

        return ret;
}


static void
free_file_and_imap_path_pair (FileAndImapPaths *pair)
{
        if (!pair) {
                return;
        }

        if (pair->imap_path) {
                g_free (pair->imap_path);
        }

        if (pair->file) {
                g_free (pair->file);
        }

        g_slice_free (FileAndImapPaths, pair);
}


static void
free_kmail_account (KMailAccount *account)
{
        if (!account) {
                return;
        }

        if (account->name) {
                g_free (account->name);
        }

        if (account->login) {
                g_free (account->login);
        }

        if (account->host) {
                g_free (account->host);
        }

        if (account->type) {
                g_free (account->type);
        }

        if (account->location) {
                g_free (account->location);
        }

        if (account->file_and_imap_paths) {
                FileAndImapPaths **pair;

                for (pair = account->file_and_imap_paths; *pair; pair++) {
                        free_file_and_imap_path_pair (*pair);
                        *pair = NULL;
                }

                g_free (account->file_and_imap_paths);
        }

        g_slice_free (KMailAccount, account);
}


#define HEX_ESCAPE '%'

static gint
hex_to_int (gchar c)
{
        return  c >= '0' && c <= '9' ? c - '0'
            : c >= 'A' && c <= 'F' ? c - 'A' + 10
            : c >= 'a' && c <= 'f' ? c - 'a' + 10
            : -1;
}


static gint
unescape_character (const gchar *scanner)
{
        gint first_digit;
        gint second_digit;

        first_digit = hex_to_int (*scanner++);

        if (first_digit < 0) {
                return -1;
        }

        second_digit = hex_to_int (*scanner++);
        if (second_digit < 0) {
                return -1;
        }

        return (first_digit << 4) | second_digit;
}


static gchar *
g_unescape_uri_string (const gchar *escaped, const gchar *illegal_characters)
{
        const gchar *in;
        gchar            *out, *result;
        gint            character;

        if (escaped == NULL) {
                return NULL;
        }

        result = g_malloc (strlen (escaped) + 1);

        out = result;
        for (in = escaped; *in != '\0'; in++) {
                character = *in;
                if (character == HEX_ESCAPE) {

                        character = unescape_character (in + 1);

                        /* Check for an illegal character. We consider '\0' illegal here. */
                        if (character == 0 || (illegal_characters != NULL && strchr (illegal_characters, (char)character) != NULL)) {
                                g_free (result);
                                return NULL;
                        }
                        in += 2;
                }
                *out++ = character;
        }

        *out = '\0';

        //g_assert (out - result <= strlen (escaped));

        if (!g_utf8_validate (result, -1, NULL)) {
                g_free (result);
                return NULL;
        }

        return result;
}


static gboolean
ignore_email (const gchar *uri)
{
        return (g_strrstr (uri, "trash")  ||
                g_strrstr (uri, "outbox") ||
                g_strrstr (uri, "drafts"));
}


static GSList *
get_dirs_to_watch (DBConnection *db_con, const gchar *dir_path, gboolean in_imap_dir)
{
        GSList       *tmp_dirs, *dirs;
        const GSList *dir;

        g_return_val_if_fail (db_con, NULL);
        g_return_val_if_fail (dir_path, NULL);

        if (tracker_file_is_no_watched (dir_path)) {
                return NULL;
        }

        dirs = NULL;
        tmp_dirs = NULL;

        tracker_get_all_dirs (dir_path, &tmp_dirs);

        for (dir = tmp_dirs; dir; dir = dir->next) {
                gchar *dir_path = g_unescape_uri_string (dir->data, "");
                gchar *dir_name = g_path_get_basename (dir_path);

                if (!tracker_file_is_no_watched (dir_path) && !ignore_email (dir_path) &&
                    (( in_imap_dir && dir_name[0] == '.' && g_str_has_suffix (dir_name, ".directory")) ||
                      !in_imap_dir )
                    ) {
                        if (!tracker_is_directory_watched (dir_path, db_con)) {
                                /* if we are in a maildir directory, we will only index emails in directory "cur" */
                                gchar *dir_cur = g_build_filename (dir_path, "cur", NULL);

                                if (tracker_is_directory (dir_cur)) {
                                        dirs = g_slist_prepend (dirs, g_strdup (dir_cur));
                                }
                                g_free (dir_cur);
                        }
                }

                g_free (dir_path);
                g_free (dir_name);
        }

        g_slist_foreach (tmp_dirs, (GFunc) g_free, NULL);
        g_slist_free (tmp_dirs);

        return dirs;
}


static KMailMailProtocol
find_mail_protocol (const gchar *mail_path)
{
        if (g_str_has_prefix (mail_path, kmail_config->dimap_cache)) {
                return KMAIL_MAIL_PROTOCOL_CACHED_IMAP;

        } else if (g_str_has_prefix (mail_path, kmail_config->imap_cache)) {
                return KMAIL_MAIL_PROTOCOL_IMAP;

        } else if (g_str_has_prefix (mail_path, kmail_config->local_dir)) {
                return KMAIL_MAIL_PROTOCOL_MBOX;

        } else {
                return KMAIL_MAIL_PROTOCOL_UNKNOWN;
        }
}


static gboolean
index_mail_file_in_maildir_dir (DBConnection *db_con, const gchar *dir, FileInfo *info, MailType mail_type)
{
        MailStore *store;

        if (tracker_db_email_get_mbox_id (db_con, dir) == -1) {
                gchar *filename = g_path_get_basename (info->uri);
                gchar *uri_prefix = g_path_get_dirname (info->uri);

                tracker_db_email_register_mbox (db_con, MAIL_APP_KMAIL, mail_type, dir, filename, uri_prefix);

                g_free (filename);
                g_free (uri_prefix);
        }

        store = tracker_db_email_get_mbox_details (db_con, dir);
        if (!store) {
                tracker_error ("ERROR: could not retrieve store for directory %s", dir);
                return FALSE;
        }

        tracker_log ("Looking for email file \"%s\"", info->uri);

        if (!tracker_db_email_is_saved_email_file (db_con, info->uri)) {
                MailMessage *mail_msg = email_parse_mail_message_by_path (MAIL_APP_KMAIL, info->uri, NULL, NULL);

                if (!mail_msg) {
                        tracker_log ("WARNING: email %s not found", info->uri);
                        tracker_db_email_free_mail_store (store);
                        return FALSE;
                }

                if (mail_msg->uri) {
                        g_free (mail_msg->uri);
                }

                mail_msg->uri = g_strdup (mail_msg->path);
                mail_msg->store = store;
                mail_msg->mtime = tracker_get_file_mtime (mail_msg->path);
                tracker_db_email_save_email (db_con, mail_msg);

                email_free_mail_file (mail_msg->parent_mail_file);
                email_free_mail_message (mail_msg);

        } else {
                /* A file is never touched at all (ever when a user read it) so its containt is never updated.
                   When a email file is put in trash it is moved to the trash directory and it is given the
                   suffix ":2,S"... For us it just means that trackerd will see a file being deleted and it
                   will remove it from its database.
                */
                tracker_log ("Nothing need to be done, email file \"%s\" is already up to date", info->uri) ;
        }

        tracker_db_email_free_mail_store (store);

        return TRUE;
}


/*

static void
watch_local_maildir_dir (DBConnection *db_con, const gchar *dir_path)
{
        gchar *dir_names[] = {"inbox", "sent-mail", "drafts", NULL};
        gchar **dir_name;

        g_return_if_fail (db_con);
        g_return_if_fail (dir_path);

        for (dir_name = dir_names; *dir_name; dir_name++) {
                gchar *dir_path = g_build_filename (dir_path, *dir_name, NULL);

                if (tracker_is_directory (dir_path)) {
                        email_maildir_watch_mail_messages (db_con, dir_path);
                }

                g_free (dir_path);
        }
}
*/

static void
load_uri_of_mbox_mail_message (GMimeMessage *g_m_message, MailMessage *mail_msg, gpointer user_data)
{
        const gchar *field;
        gchar       *tmp_from, *from;

        g_return_if_fail (g_m_message);
        g_return_if_fail (mail_msg);

        /* We need that line:
           From laurent.aguerreche@free.fr Fri Sep 22 00:58:37 2006
           and we need ALL the line (i.e. "From " + email address + date).

           This line is at the beginning of each header but GMIME does not give access to it so we reproduce it.
        */

        field = g_mime_message_get_header (g_m_message, "From");

        tmp_from = g_strdup (field);

        from = NULL;

        if (tmp_from) {
                gchar *email_addr_pos = strchr (tmp_from, '<');

                if (email_addr_pos) {
                        size_t len;

                        email_addr_pos++;

                        len = strlen (email_addr_pos);

                        if (len > 1) {
                                from = g_strndup (email_addr_pos, len - 1);
                        }
                } else {
                        from = g_strdup (tmp_from);
                }

                g_free (tmp_from);
        }

        if (mail_msg->uri) {
                g_free (mail_msg->uri);
        }

        mail_msg->uri = NULL;

        if (from) {
                time_t date;
                gint   gmt_offset;

                g_mime_message_get_date (g_m_message, &date, &gmt_offset);
                mail_msg->uri = g_strdup_printf ("mbox:%s/From %s %s", mail_msg->parent_mail_file->path, from, ctime (&date));

                /* remove '\n' introduced by ctime() */
                if (!tracker_is_empty_string (mail_msg->uri)) {
                        size_t len = strlen (mail_msg->uri);
                        if (mail_msg->uri[len - 1] == '\n') {
                                mail_msg->uri[len - 1] = '\0';
                        }
                }
                g_free (from);
        }
}


static gchar *
forward_uri_for_mbox_email (MailMessage *mail_msg, gpointer user_data)
{
        /* URI previously made in load_uri_of_mbox_mail_message() so we just
           need to forward it */
        return mail_msg->uri;
}


static void
fill_uri_with_uid_for_imap (GMimeMessage *g_m_message, MailMessage *mail_msg, gpointer user_data)
{
        gchar       *headers;
        const gchar *pos_uid;

        g_return_if_fail (g_m_message);
        g_return_if_fail (mail_msg);

        if (mail_msg->uri) {
                g_free (mail_msg->uri);
        }

        mail_msg->uri = NULL;

        /* Currently, "g_mime_message_get_header(g_m_message, "X-UID")" does not work because GMIME
           only handles RFC822 headers... So we have to search X-UID header ourself. */

        headers = g_mime_message_get_headers (g_m_message);
        g_return_if_fail (headers);

        pos_uid = strstr (headers, "X-UID");

        if (pos_uid) {
                size_t      len;
                const gchar *end_of_line, *end_of_uid;
                gchar       *uid;

                len = strlen (pos_uid);
                if (len < 7) {  /* 7 = strlen ("X-UID: ") */
                        g_free (headers);
                        return;
                }

                pos_uid += 6;
                if (*pos_uid != ' ') {
                        /* I don't know what we're reading! */
                        g_free (headers);
                        return;
                }
                pos_uid++;

                end_of_line = strchr (pos_uid, '\n');
                if (!end_of_line) {
                        /* ??? this mail is not RFC822 compliant... */
                        g_free (headers);
                        return;
                }

                for (end_of_uid = end_of_line; end_of_uid > pos_uid; end_of_uid--) {
                        if (isdigit (*(end_of_uid - 1))) {
                                break;
                        }
                }

                if (end_of_uid == pos_uid) {
                        /* This UID is empty?! */
                        g_free (headers);
                        return;
                }

                uid = g_strndup (pos_uid, end_of_uid - pos_uid);

                /* URI is not complete because we need more infos... but we'll use UID somewhere else. */
                mail_msg->uri = g_strdup_printf (";UID=%s", uid);

                g_free (uid);
        }

        g_free (headers);
}


static gchar *
make_imap_email_uri (MailMessage *mail_msg, gpointer user_data)
{
  gchar *head_uid, *uri;
  const GSList *account;

  if (!mail_msg->uri) {
        /* no UID found in the mail message? We return... */
        return NULL;
  }

  head_uid = mail_msg->uri;
  uri = NULL;

  for (account = user_data; account; account = account->next) {
        const KMailAccount *acc = account->data;

        if (acc->file_and_imap_paths) {
                FileAndImapPaths **pair;

                for (pair = acc->file_and_imap_paths; *pair; pair++) {
                        if (g_str_has_suffix (mail_msg->parent_mail_file->path, (*pair)->file)) {
                                /* URI is something like "imap://foo@imap.yeah.fr/INBOX/;UID=1114282150" */
                                if (acc->login && acc->host && (*pair)->imap_path) {
                                        uri = g_strconcat ("imap://", acc->login, "@",
                                                           acc->host, (*pair)->imap_path, head_uid, NULL);
                                        break;
                                }
                        }
                }
        }
  }
  g_free (head_uid);

  return uri;
}


/* Try to expand environment variables/call some programs in a string
 * Supported string formats: $(foo), ${foo} and $foo.
 */
static gchar *
expand_string (const gchar *s)
{
        size_t  i, len;
        GString        *string;
        size_t        word_beg, word_end;
        gchar   *ret;

        g_return_val_if_fail (s, NULL);

        string = g_string_new ("");

        len = strlen (s);

        word_beg = word_end = 0;

        for (i = 0; i < len; i++) {
                gboolean eval_cmd = FALSE;
                gboolean delimited_word = FALSE;  /* used to distinct ${foo} or $(foo), from $foo */

                if (s[i + 1] == '\0' || s[i] == '$') {
                        gchar *word;

                        /* we save the previous non-expanded word */

                        if (s[i] == '$') {
                                word = g_strndup (s + word_beg, i - word_beg);
                        } else {
                                word = g_strndup (s + word_beg, i + 1 - word_beg);
                        }

                        g_string_append (string, word);
                        g_free (word);

                        if (s[i + 1] == '\0') {
                                break;
                        }
                }


                if (s[i] == '$') {
                        i++;

                        if (s[i] == '{' && i < len - 1) {
                                size_t j;

                                word_beg = i + 1;

                                for (j = word_beg; s[j] != '}' && j < len; j++)
                                        ;

                                if (s[j] == '}' && s[j - 1] != '{') {
                                        word_end = j - 1;
                                        delimited_word = TRUE;
                                } else {
                                        /* to not treat this word */
                                        word_end = word_beg;
                                }

                        } else if (s[i] == '(' && i < len - 1) {
                                size_t j;

                                word_beg = i + 1;

                                for (j = word_beg; s[j] != ')' && j < len; j++)
                                                ;

                                if (s[j] == ')' && s[j - 1] != '(') {
                                        word_end = j - 1;
                                        eval_cmd = TRUE;
                                        delimited_word = TRUE;
                                } else {
                                        /* to not treat this word */
                                        word_end = word_beg;
                                }

                        } else {
                                size_t j;

                                word_beg = i;

                                for (j = word_beg; s[j] != G_DIR_SEPARATOR && j < len; j++)
                                        ;

                                if (s[j] == G_DIR_SEPARATOR) {
                                        word_end = j - 1;

                                } else if (j == len) {
                                        if (j != i) {
                                                word_end = j - 1;
                                        } else {
                                                /* to not treat this word */
                                                word_end = word_beg;
                                        }
                                }
                        }


                        if (word_beg != word_end) {
                                gchar *word = g_strndup (s + word_beg, word_end - word_beg + 1);

                                if (eval_cmd) {
                                        FILE   *f;
                                        size_t nb_elm;

                                        f = popen (word, "r");

                                        if (f) {
                                                gchar   buf[128];
                                                GString        *tmp;

                                                tmp = g_string_new ("");

                                                for (;;) {
                                                        nb_elm = fread (buf, 1, 128, f);

                                                        if (nb_elm > 0) {
                                                                buf [nb_elm - 1] = '\0';
                                                                g_string_append (tmp, buf);
                                                        }

                                                        if (nb_elm < 128) {
                                                                break;
                                                        }
                                                }

                                                pclose (f);

                                                g_string_append (string, g_string_free (tmp, FALSE));
                                        }

                                } else {
                                        const gchar *expanded = g_getenv (word);

                                        if (expanded) {
                                                g_string_append (string, expanded);
                                        }
                                }

                                g_free (word);

                                if (delimited_word) {
                                        word_end++;  /* remove last character: '}' or ')' */
                                }

                                word_beg = word_end = word_end + 1;

                                /* for-loop will increment i so we have to start a bit earlier */
                                i = word_beg - 1;
                        }
                }
        }

        ret = g_string_free (string, FALSE);

        if (tracker_is_empty_string (ret)) {
                g_free (ret);
                ret = NULL;
        }

        return ret;
}
