/* Tracker
 * mbox routines
 * Copyright (C) 2006, Laurent Aguerreche
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _TRACKER_MBOX_H_
#define _TRACKER_MBOX_H_

#include <glib.h>
#include <gmime/gmime.h>

#include "tracker-utils.h"

#include "config.h"

#ifdef USING_SQLITE
#   include "tracker-db-sqlite.h"
#else
#   include "tracker-db-mysql.h"
#endif


typedef enum {
	MAIL_APP_EVOLUTION,
	MAIL_APP_KMAIL,
	MAIL_APP_THUNDERBIRD,
	MAIL_APP_UNKNOWN
} MailApplication;


typedef struct {
	char *name;
	char *addr;
} Person;


typedef struct {
	char		*mbox_uri;
	MailApplication	mail_app;
	GMimeStream	*stream;
	GMimeParser	*parser;
	guint64		next_email_offset;
} MailBox;


typedef struct {
	char *attachment_name;
	char *mime;
	char *tmp_decoded_file;
} MailAttachment;


typedef struct {
	MailBox		*parent_mbox;
	char		*uri;
	guint64		offset;		/* start address of the email */
	char		*message_id;
	gboolean	deleted;
	gboolean	junk;
	GSList		*references;	/* message_ids */
	GSList		*reply_to_id;	/* message_id of emails that it replies to */
	long		date;
	char		*mail_from;
	GSList		*mail_to;
	GSList		*mail_cc;
	GSList		*mail_bcc;
	char		*subject;
	char		*content_type;	/* text/plain or text/html etc. */
	char		*body;
	char		*path_to_attachments;
	GSList		*attachments;	/* names of all attachments */
} MailMessage;


void		tracker_watch_emails			(DBConnection *db_con);
void		tracker_end_email_watching		(void);
gboolean	tracker_is_in_a_application_mail_dir	(const char *uri);
gboolean	tracker_is_a_handled_mbox_file		(const char *uri);
MailBox *	tracker_mbox_parse_from_offset		(const char *uri, off_t offset);
void		tracker_close_mbox_and_free_memory	(MailBox *mb);
MailMessage *	tracker_mbox_parse_next			(MailBox *mb);
void		tracker_free_mail_message		(MailMessage *msg);
void		tracker_free_person			(Person *p);
gboolean	tracker_is_an_email_attachment		(const char *uri);
void		tracker_index_each_email_attachment	(DBConnection *db_con, const MailMessage *msg);
void		tracker_unlink_email_attachment		(const char *uri);

#endif
