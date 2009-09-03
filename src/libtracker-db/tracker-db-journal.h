/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_DB_JOURNAL_H__
#define __TRACKER_DB_JOURNAL_H__

#include <glib.h>
#include <gio/gio.h>

#define TRACKER_DB_JOURNAL_MAX_SIZE	52428800

G_BEGIN_DECLS

typedef GPtrArray TrackerJournalContents;

const gchar* tracker_db_journal_filename (void);
void tracker_db_journal_open (void);
void tracker_db_journal_log (const gchar *query);
void tracker_db_journal_truncate (void);
void tracker_db_journal_close (void);
TrackerJournalContents* tracker_db_journal_get_contents (guint transaction_size);
void tracker_db_journal_free_contents (TrackerJournalContents *contents);
void tracker_db_journal_fsync (void);
gsize tracker_db_journal_get_size (void);

G_END_DECLS

#endif /* __TRACKER_DB_JOURNAL_H__ */
