/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_DB_JOURNAL_H__
#define __LIBTRACKER_DB_JOURNAL_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_DB_JOURNAL_ERROR_DOMAIN "TrackerDBJournal"
#define TRACKER_DB_JOURNAL_ERROR        tracker_db_journal_error_quark()
#define TRACKER_DB_JOURNAL_FILENAME     "tracker-store.journal"

typedef enum {
	TRACKER_DB_JOURNAL_START,
	TRACKER_DB_JOURNAL_START_TRANSACTION,
	TRACKER_DB_JOURNAL_START_ONTOLOGY_TRANSACTION,
	TRACKER_DB_JOURNAL_END_TRANSACTION,
	TRACKER_DB_JOURNAL_RESOURCE,
	TRACKER_DB_JOURNAL_INSERT_STATEMENT,
	TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID,
	TRACKER_DB_JOURNAL_DELETE_STATEMENT,
	TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID,
} TrackerDBJournalEntryType;

GQuark       tracker_db_journal_error_quark                  (void);

/*
 * Writer API
 */
gboolean     tracker_db_journal_init                         (const gchar *filename,
                                                              gboolean     truncate);
gboolean     tracker_db_journal_shutdown                     (void);

const gchar* tracker_db_journal_get_filename                 (void);
gsize        tracker_db_journal_get_size                     (void);

void         tracker_db_journal_set_rotating                 (gboolean     do_rotating,
                                                              gsize        chunk_size,
                                                              const gchar *rotate_to);

void         tracker_db_journal_get_rotating                 (gboolean    *do_rotating,
                                                              gsize       *chunk_size,
                                                              gchar      **rotate_to);

gboolean     tracker_db_journal_start_transaction            (time_t       time);
gboolean     tracker_db_journal_start_ontology_transaction   (time_t       time);

gboolean     tracker_db_journal_append_delete_statement      (gint         g_id,
                                                              gint         s_id,
                                                              gint         p_id,
                                                              const gchar *object);
gboolean     tracker_db_journal_append_delete_statement_id   (gint         g_id,
                                                              gint         s_id,
                                                              gint         p_id,
                                                              gint         o_id);
gboolean     tracker_db_journal_append_insert_statement      (gint         g_id,
                                                              gint         s_id,
                                                              gint         p_id,
                                                              const gchar *object);
gboolean     tracker_db_journal_append_insert_statement_id   (gint         g_id,
                                                              gint         s_id,
                                                              gint         p_id,
                                                              gint         o_id);
gboolean     tracker_db_journal_append_resource              (gint         s_id,
                                                              const gchar *uri);

gboolean     tracker_db_journal_rollback_transaction         (void);
gboolean     tracker_db_journal_commit_db_transaction           (void);

gboolean     tracker_db_journal_fsync                        (void);
gboolean     tracker_db_journal_truncate                     (gsize new_size);

/*
 * Reader API
 */
gboolean     tracker_db_journal_reader_init                  (const gchar  *filename);
gboolean     tracker_db_journal_reader_shutdown              (void);
TrackerDBJournalEntryType
             tracker_db_journal_reader_get_type              (void);

gboolean     tracker_db_journal_reader_next                  (GError      **error);
gint64       tracker_db_journal_reader_get_time              (void);
gboolean     tracker_db_journal_reader_get_resource          (gint         *id,
                                                              const gchar **uri);
gboolean     tracker_db_journal_reader_get_statement         (gint         *g_id,
                                                              gint         *s_id,
                                                              gint         *p_id,
                                                              const gchar **object);
gboolean     tracker_db_journal_reader_get_statement_id      (gint         *g_id,
                                                              gint         *s_id,
                                                              gint         *p_id,
                                                              gint         *o_id);
gsize        tracker_db_journal_reader_get_size_of_correct   (void);
gdouble      tracker_db_journal_reader_get_progress          (void);

gboolean     tracker_db_journal_reader_verify_last           (const gchar  *filename,
                                                              GError      **error);

G_END_DECLS

#endif /* __LIBTRACKER_DB_JOURNAL_H__ */
