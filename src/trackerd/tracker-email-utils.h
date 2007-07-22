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



typedef enum {
	MAIL_TYPE_MBOX,
	MAIL_TYPE_IMAP,
	MAIL_TYPE_IMAP4,
	MAIL_TYPE_MAILDIR,
	MAIL_TYPE_MH
} MailType;


typedef struct {
	char *name;
	char *addr;
} MailPerson;


typedef struct {
	int		offset;
	int		mail_count;
	int		junk_count;
	int		delete_count;
	char 		*uri_prefix;	
	MailType	type;
} MailStore;


typedef struct {
	char		*path;
	MailApplication	mail_app;
	GMimeParser	*parser;
	GMimeStream	*stream;
	guint64		next_email_offset;
} MailFile;


typedef struct {
	char *attachment_name;
	char *mime;
	char *tmp_decoded_file;
} MailAttachment;


typedef enum {
	MIME_ENCODING_UNKNOWN,
	MIME_ENCODING_7BIT,
	MIME_ENCODING_8BIT,
	MIME_ENCODING_BINARY,
	MIME_ENCODING_BASE64,
	MIME_ENCODING_QUOTEDPRINTABLE,
	MIME_ENCODING_UUENCODE
} MimeEncoding;


typedef struct {
	gchar		*type;
	gchar		*subtype;
	gchar		*name;
	MimeEncoding	encoding;
} MimeInfos;


typedef struct {
	MailFile	*parent_mail_file;
	int		id;
	gboolean	is_mbox;
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
	GSList		*attachments;		/* names of attachments */
	MailStore	*store;
	
} MailMessage;




void		email_watch_directory				(const gchar *dir, const gchar *service);
void		email_watch_directories				(const GSList *dirs, const gchar *service);

typedef void (* LoadHelperFct) (GMimeMessage *g_m_message, MailMessage *msg);

gboolean	email_parse_and_save_mail_message		(DBConnection *db_con, MailApplication mail_app, const char *path, LoadHelperFct load_helper);
gboolean	email_parse_mail_file_and_save_new_emails	(DBConnection *db_con, MailApplication mail_app, const char *path, LoadHelperFct load_helper, MailStore *store);

gboolean	email_mh_is_in_a_mh_dir				(const char *path);
void		email_mh_watch_mail_messages			(DBConnection *db_con, const char *path);

gboolean	email_maildir_is_in_a_maildir_dir		(const char *path);
void		email_maildir_watch_mail_messages		(DBConnection *db_con, const char *path);

MailPerson *	email_allocate_mail_person			(void);
void		email_free_mail_person				(MailPerson *mp);

MailAttachment*	email_allocate_mail_attachment			(void);
void		email_free_mail_attachment			(MailAttachment *ma);

MailMessage *	email_allocate_mail_message			(void);
void		email_free_mail_message				(MailMessage *msg);

MailFile *	email_open_mail_file_at_offset			(MailApplication mail_app, const char *path, off_t offset, gboolean scan_from_for_mbox);
void		email_free_mail_file				(MailFile *mf);
MailMessage *	email_mail_file_parse_next			(MailFile *mf, LoadHelperFct load_helper);

MailMessage *	email_parse_mail_message_by_path		(MailApplication mail_app, const char *path, LoadHelperFct load_helper);

MimeInfos *	email_allocate_mime_infos			(void);
void		email_free_mime_infos				(MimeInfos *infos);
MimeInfos *	email_get_mime_infos_from_mime_file		(const gchar *mime_file);

void		email_index_each_email_attachment		(DBConnection *db_con, const MailMessage *msg);
gboolean	email_add_saved_mail_attachment_to_mail_message	(MailMessage *mail_msg, MailAttachment *ma);

gchar *         email_make_tmp_name_for_mail_attachment         (const gchar *filename);
gboolean	email_decode_mail_attachment_to_file		(const gchar *src, const gchar *dst, MimeEncoding encoding);

#endif
