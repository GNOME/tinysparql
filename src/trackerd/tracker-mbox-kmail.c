/* Tracker
 * mbox routines
 * Copyright (C) 2006, Laurent Aguerreche
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glib/gstdio.h>

#include "tracker-mbox-kmail.h"

#ifdef HAVE_INOTIFY
#   include "tracker-inotify.h"
#else
#   ifdef HAVE_FAM
#      include "tracker-fam.h"
#   endif
#endif


extern Tracker *tracker;


static char *kmail_mail_dir = NULL; /* this directory is variable so we use a variable here */


/* Try to expand environment variables/call some programs in a string
 * Supported strings: $(foo), ${foo} and $foo.
 */
static char *
expand (const char *s)
{
	size_t	i, len;
	GString	*string;
	size_t	word_beg, word_end;
	char *ret;

	if (!s) {
		return NULL;
	}

	string = g_string_new ("");


	len = strlen (s);

	word_beg = word_end = 0;

	for (i = 0; i < len; i++) {
		gboolean eval_cmd;
		gboolean delimited_word;  /* used to distinct ${foo}, or $(foo) from $foo */

		eval_cmd = FALSE;
		delimited_word = FALSE;


		if (s[i + 1] == '\0' || s[i] == '$') {
			char *word;

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
				char *word;

				word = g_strndup (s + word_beg, word_end - word_beg + 1);

				if (eval_cmd) {
					FILE   *f;
					size_t nb_elm;

					f = popen (word, "r");

					if (f) {
						char buf[128];
						GString *tmp;

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
					const char *expanded;

					expanded = g_getenv (word);

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

	if (ret && ret[0] == '\0') {
		/* we don't want to return a empty string */
		g_free (ret);
		ret = NULL;
	}

	return ret;
}


static char *
find_kmail_dir (void)
{
	char *kmailrc;

	kmailrc = g_build_filename (g_get_home_dir (), ".kde/share/config/kmailrc", NULL);

	if (!g_access (kmailrc, F_OK) && !g_access (kmailrc, R_OK)) {
		GKeyFile *key_file;

		key_file = g_key_file_new ();

		g_key_file_load_from_file (key_file, kmailrc, G_KEY_FILE_NONE, NULL);

		g_free (kmailrc);


		if (g_key_file_has_key (key_file, "General", "folders", NULL)) {
			char *tmp, *kmail_dir;

			tmp = g_key_file_get_string (key_file, "General", "folders", NULL);

			kmail_dir = expand (tmp);

			g_free (tmp);

			return kmail_dir;
		}
	}

	return NULL;
}


static GSList *
find_kmail_mboxes (const char *kmail_dir)
{
	char   *filenames[] = {"inbox", "sent-mail", NULL};
	char   **filename;
	GSList *list_of_mboxes;

	if (!kmail_dir) {
		return NULL;
	}


	list_of_mboxes = NULL;

	for (filename = filenames; *filename; filename++) {
		char *file;

		file = g_build_filename (kmail_dir, *filename, NULL);

		if (!g_access (file, F_OK) && !g_access (file, R_OK)) {
			list_of_mboxes = g_slist_prepend (list_of_mboxes, file);
		} else {
			g_free (file);
		}
	}

	return list_of_mboxes;
}


void
init_kmail_mboxes_module (void)
{
	char *kmail_dir;

	kmail_dir = find_kmail_dir ();

	if (!kmail_mail_dir && kmail_dir) {
		kmail_mail_dir = kmail_dir;
	}
}


void
finalize_kmail_mboxes_module (void)
{
	if (kmail_mail_dir) {
		g_free (kmail_mail_dir);

		kmail_mail_dir = NULL;
	}
}


GSList *
watch_emails_of_kmail (DBConnection *db_con)
{
	GSList *list_of_mboxes;
	const GSList *tmp;

	if (!db_con || !kmail_mail_dir) {
		return NULL;
	}

	list_of_mboxes = find_kmail_mboxes (kmail_mail_dir);

	for (tmp = list_of_mboxes; tmp; tmp = g_slist_next (tmp)) {
		const char *uri;
		char	   *base_dir;
		FileInfo   *info;

		uri = (char *) tmp->data;

		base_dir = g_path_get_dirname (uri);

		if (!tracker_is_directory_watched (base_dir, db_con)) {
			tracker_add_watch_dir (base_dir, db_con);
		}

		g_free (base_dir);


		info = tracker_create_file_info (uri, TRACKER_ACTION_CHECK, 0, WATCH_OTHER);

		info->is_directory = FALSE;
		info->mime = g_strdup ("application/mbox");

		g_async_queue_push (tracker->file_process_queue, info);
	}

	return list_of_mboxes;
}


void
get_status_of_kmail_email (GMimeMessage *g_m_message, MailMessage *msg)
{
}


void
get_uri_of_kmail_email (GMimeMessage *g_m_message, MailMessage *msg)
{
	const char *field;
	char	   *tmp_from, *from;

	if (!g_m_message || !msg || !msg->parent_mbox) {
		return;
	}

	/* We need that line:
	   From laurent.aguerreche@free.fr Fri Sep 22 00:58:37 2006
	   and we need ALL the line (i.e. "From " + email address + date).

	   This line is at the beginning of each header but GMIME does not give access to it so we reproduce it.
	*/

	field = g_mime_message_get_header (g_m_message, "From");

	tmp_from = g_strdup (field);

	from = NULL;

	if (tmp_from) {
		char *email_addr_beg;

		email_addr_beg = strchr (tmp_from, '<');

		if (email_addr_beg) {
			size_t len;

			email_addr_beg++;

			len = strlen (email_addr_beg);

			if (len > 1) {
				from = g_strndup (email_addr_beg, len - 1);
			}
		} else {
			from = g_strdup (tmp_from);
		}

		g_free (tmp_from);
	}


	msg->uri = NULL;

	if (from) {
		//char   *uri;
		time_t date;
		int    gmt_offset;

		g_mime_message_get_date (g_m_message, &date, &gmt_offset);

		msg->uri = g_strdup_printf ("mbox:%s/From %s %s", msg->parent_mbox->mbox_uri, from, ctime (&date));

		g_free (from);
	}
}


gboolean
is_in_a_kmail_mail_dir (const char *uri)
{
	if (!kmail_mail_dir || !uri) {
		return FALSE;
	}

	return g_str_has_prefix (uri, kmail_mail_dir);
}
