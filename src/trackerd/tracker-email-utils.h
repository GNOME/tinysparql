/* Tracker
 * routines for emails
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

#ifndef _TRACKER_EMAIL_BASE_H_
#define _TRACKER_EMAIL_BASE_H_

#include <gmime/gmime.h>

#include "tracker-email.h"


typedef enum {
	MAIL_APP_EVOLUTION,
	MAIL_APP_KMAIL,
	MAIL_APP_THUNDERBIRD,
	MAIL_APP_UNKNOWN
} MailApplication;


typedef struct {
	char *name;
	char *addr;
} MailPerson;


typedef struct {
	char		*path;
	MailApplication	mail_app;
	GMimeParser	*parser;
	guint64		next_email_offset;
} MailFile;


typedef struct {
	char *attachment_name;
	char *mime;
	char *tmp_decoded_file;
} MailAttachment;


typedef struct {
	MailFile	*parent_mail_file;
	char		*path;
	char		*uri;			/* uri to pass to a mail client to open it at mail message */
	guint64		offset;			/* start address of the email */
	char		*message_id;
	char		*reply_to;
	gboolean	deleted;
	gboolean	junk;
	GSList		*references;		/* message_ids */
	GSList		*in_reply_to_ids;	/* message_id of emails that it replies to */
	long		date;
	char		*from;
	GSList		*to;
	GSList		*cc;
	GSList		*bcc;
	char		*subject;
	char		*content_type;		/* text/plain or text/html etc. */
	char		*body;
	char		*path_to_attachments;
	GSList		*attachments;		/* names of attachments */
} MailMessage;


void		email_set_root_path_for_attachments		(const char *path);
const char *	email_get_root_path_for_attachments		(void);
void		email_free_root_path_for_attachments		(void);

typedef void (* LoadHelperFct) (GMimeMessage *g_m_message, MailMessage *msg);

gboolean	email_parse_and_save_mail_message		(DBConnection *db_con, MailApplication mail_app, const char *path, LoadHelperFct load_helper);
gboolean	email_parse_mail_file_and_save_new_emails	(DBConnection *db_con, MailApplication mail_app, const char *path, LoadHelperFct load_helper);

gboolean	email_mh_is_in_a_mh_dir				(const char *path);
void		email_mh_watch_mail_messages			(DBConnection *db_con, const char *path);

gboolean	email_maildir_is_in_a_maildir_dir		(const char *path);
void		email_maildir_watch_mail_messages		(DBConnection *db_con, const char *path);

MailPerson *	email_allocate_mail_person			(void);
void		email_free_mail_person				(MailPerson *mp);

MailMessage *	email_allocate_mail_message			(void);
void		email_free_mail_message				(MailMessage *msg);

MailFile *	email_open_mail_file_at_offset			(MailApplication mail_app, const char *path, off_t offset, gboolean scan_from_for_mbox);
void		email_free_mail_file				(MailFile *mf);
MailMessage *	email_mail_file_parse_next			(MailFile *mf, LoadHelperFct load_helper);

MailMessage *	email_parse_mail_message_by_path		(MailApplication mail_app, const char *path, LoadHelperFct load_helper);

void		email_index_each_email_attachment		(DBConnection *db_con, const MailMessage *msg);

#endif
