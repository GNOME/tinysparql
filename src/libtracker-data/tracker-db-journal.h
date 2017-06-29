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

#define TRACKER_DB_JOURNAL_ERROR_DOMAIN       "TrackerDBJournal"
#define TRACKER_DB_JOURNAL_ERROR              tracker_db_journal_error_quark()
#define TRACKER_DB_JOURNAL_FILENAME          "tracker-store.journal"
#define TRACKER_DB_JOURNAL_ONTOLOGY_FILENAME "tracker-store.ontology.journal"

enum {
	TRACKER_DB_JOURNAL_ERROR_UNKNOWN = 0,
	TRACKER_DB_JOURNAL_ERROR_DAMAGED_JOURNAL_ENTRY,
	TRACKER_DB_JOURNAL_ERROR_COULD_NOT_WRITE,
	TRACKER_DB_JOURNAL_ERROR_COULD_NOT_CLOSE,
	TRACKER_DB_JOURNAL_ERROR_BEGIN_OF_JOURNAL
};

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
	TRACKER_DB_JOURNAL_UPDATE_STATEMENT,
	TRACKER_DB_JOURNAL_UPDATE_STATEMENT_ID,
} TrackerDBJournalEntryType;

typedef struct _TrackerDBJournal TrackerDBJournal;
typedef struct _TrackerDBJournalReader TrackerDBJournalReader;

GQuark       tracker_db_journal_error_quark                  (void);

/*
 * Writer API
 */
TrackerDBJournal *
             tracker_db_journal_new                          (GFile        *data_location,
                                                              gboolean      truncate,
                                                              GError      **error);
TrackerDBJournal *
             tracker_db_journal_ontology_new                 (GFile        *data_location,
                                                              GError      **error);
gboolean     tracker_db_journal_free                         (TrackerDBJournal  *writer,
                                                              GError           **error);

gsize        tracker_db_journal_get_size                     (TrackerDBJournal  *writer);

void         tracker_db_journal_set_rotating                 (gboolean     do_rotating,
                                                              gsize        chunk_size,
                                                              const gchar *rotate_to);

void         tracker_db_journal_get_rotating                 (gboolean    *do_rotating,
                                                              gsize       *chunk_size,
                                                              gchar      **rotate_to);

gboolean     tracker_db_journal_start_transaction            (TrackerDBJournal   *writer,
                                                              time_t              time);

gboolean     tracker_db_journal_append_delete_statement      (TrackerDBJournal   *writer,
                                                              gint                g_id,
                                                              gint                s_id,
                                                              gint                p_id,
                                                              const gchar        *object);
gboolean     tracker_db_journal_append_delete_statement_id   (TrackerDBJournal   *writer,
                                                              gint                g_id,
                                                              gint                s_id,
                                                              gint                p_id,
                                                              gint                o_id);
gboolean     tracker_db_journal_append_insert_statement      (TrackerDBJournal   *writer,
                                                              gint                g_id,
                                                              gint                s_id,
                                                              gint                p_id,
                                                              const gchar        *object);
gboolean     tracker_db_journal_append_insert_statement_id   (TrackerDBJournal   *writer,
                                                              gint                g_id,
                                                              gint                s_id,
                                                              gint                p_id,
                                                              gint                o_id);
gboolean     tracker_db_journal_append_update_statement      (TrackerDBJournal   *writer,
                                                              gint                g_id,
                                                              gint                s_id,
                                                              gint                p_id,
                                                              const gchar        *object);
gboolean     tracker_db_journal_append_update_statement_id   (TrackerDBJournal   *writer,
                                                              gint                g_id,
                                                              gint                s_id,
                                                              gint                p_id,
                                                              gint                o_id);
gboolean     tracker_db_journal_append_resource              (TrackerDBJournal   *writer,
                                                              gint                s_id,
                                                              const gchar        *uri);

gboolean     tracker_db_journal_rollback_transaction         (TrackerDBJournal   *writer);
gboolean     tracker_db_journal_commit_db_transaction        (TrackerDBJournal   *writer,
                                                              GError            **error);

gboolean     tracker_db_journal_fsync                        (TrackerDBJournal   *writer);
gboolean     tracker_db_journal_truncate                     (TrackerDBJournal   *writer,
                                                              gsize               new_size);

void         tracker_db_journal_remove                       (TrackerDBJournal *writer);

/*
 * Reader API
 */
TrackerDBJournalReader *
             tracker_db_journal_reader_new                   (GFile         *data_location,
                                                              GError       **error);
TrackerDBJournalReader *
             tracker_db_journal_reader_ontology_new          (GFile         *data_location,
                                                              GError       **error);
void         tracker_db_journal_reader_free                  (TrackerDBJournalReader *reader);
TrackerDBJournalEntryType
             tracker_db_journal_reader_get_entry_type        (TrackerDBJournalReader  *reader);

gboolean     tracker_db_journal_reader_next                  (TrackerDBJournalReader  *reader,
                                                              GError                 **error);
gint64       tracker_db_journal_reader_get_time              (TrackerDBJournalReader  *reader);
gboolean     tracker_db_journal_reader_get_resource          (TrackerDBJournalReader  *reader,
                                                              gint                    *id,
                                                              const gchar            **uri);
gboolean     tracker_db_journal_reader_get_statement         (TrackerDBJournalReader  *reader,
                                                              gint                    *g_id,
                                                              gint                    *s_id,
                                                              gint                    *p_id,
                                                              const gchar            **object);
gboolean     tracker_db_journal_reader_get_statement_id      (TrackerDBJournalReader  *reader,
                                                              gint                    *g_id,
                                                              gint                    *s_id,
                                                              gint                    *p_id,
                                                              gint                    *o_id);
gsize        tracker_db_journal_reader_get_size_of_correct   (TrackerDBJournalReader  *reader);
gdouble      tracker_db_journal_reader_get_progress          (TrackerDBJournalReader  *reader);

gboolean     tracker_db_journal_reader_verify_last           (GFile                   *data_location,
                                                              GError                 **error);

G_END_DECLS

#endif /* __LIBTRACKER_DB_JOURNAL_H__ */
