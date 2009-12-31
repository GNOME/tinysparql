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

#include "config.h"

#define _GNU_SOURCE

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libtracker-common/tracker-crc32.h>

#include "tracker-db-journal.h"

static struct {
	GMappedFile *file;
	const gchar *current;
	const gchar *end;
	const gchar *entry_begin;
	const gchar *entry_end;
	guint32 amount_of_triples;
	TrackerDBJournalEntryType type;
	const gchar *uri;
	guint32 s_id;
	guint32 p_id;
	guint32 o_id;
	const gchar *object;
} journal_reader;

static struct {
	gchar *filename;
	FILE *journal;
	gsize current_size;
	guint cur_block_len;
	guint cur_block_alloc;
	gchar *cur_block;
	guint cur_entry_amount;
	guint cur_pos;
} writer;

#define TRACKER_DB_JOURNAL_LOG_FILENAME  "tracker-store.journal"
#define MIN_BLOCK_SIZE                   1024

gsize
tracker_db_journal_get_size (void)
{
	return writer.current_size;
}

const gchar *
tracker_db_journal_filename (void)
{
	if (!writer.filename) {
		writer.filename = g_build_filename (g_get_user_data_dir (),
		                                    "tracker",
		                                    "data",
		                                    TRACKER_DB_JOURNAL_LOG_FILENAME,
		                                    NULL);
	}

	return (const gchar *) writer.filename;
}

static void
kill_cur_block (void)
{
	writer.cur_block_len = 0;
	writer.cur_pos = 0;
	writer.cur_entry_amount = 0;
	writer.cur_block_alloc = 0;
	g_free (writer.cur_block);
	writer.cur_block = NULL;
}

static gint
nearest_pow (gint num)
{
	gint n = 1;
	while (n < num)
		n <<= 1;
	return n;
}

static void
cur_block_maybe_expand (guint len)
{
	guint want_alloc = writer.cur_block_len + len;

	if (want_alloc > writer.cur_block_alloc) {
		want_alloc = nearest_pow (want_alloc);
		want_alloc = MAX (want_alloc, MIN_BLOCK_SIZE);
		writer.cur_block = g_realloc (writer.cur_block, want_alloc);
		writer.cur_block_alloc = want_alloc;
	}
}

void
tracker_db_journal_open (const gchar *filen)
{
	struct stat st;

	writer.cur_block_len = 0;
	writer.cur_pos = 0;
	writer.cur_entry_amount = 0;
	writer.cur_block_alloc = 0;
	writer.cur_block = NULL;

	if (!filen) {
		tracker_db_journal_filename ();
	} else {
		writer.filename = g_strdup (filen);
	}

	writer.journal = fopen (writer.filename, "a");

	if (stat (writer.filename, &st) == 0) {
		writer.current_size = (gsize) st.st_size;
	}

	if (writer.current_size == 0) {
		g_assert (writer.cur_block_len == 0);
		g_assert (writer.cur_block_alloc == 0);
		g_assert (writer.cur_block == NULL);
		g_assert (writer.cur_block == NULL);

		cur_block_maybe_expand (8);

		writer.cur_block[0] = 't';
		writer.cur_block[1] = 'r';
		writer.cur_block[2] = 'l';
		writer.cur_block[3] = 'o';
		writer.cur_block[4] = 'g';
		writer.cur_block[5] = '\0';
		writer.cur_block[6] = '0';
		writer.cur_block[7] = '1';

		write (fileno (writer.journal), writer.cur_block, 8);

		writer.current_size += 8;

		kill_cur_block ();
	}
}

void
tracker_db_journal_start_transaction (void)
{
	guint size = sizeof (guint32) * 3;

	cur_block_maybe_expand (size);

	/* Leave space for size, amount and crc 
	 * Check and keep in sync the offset variable at 
	 * tracker_db_journal_commit_transaction too */

	memset (writer.cur_block, 0, size);

	writer.cur_pos = writer.cur_block_len = size;
	writer.cur_entry_amount = 0;
}

static void
cur_setnum (gchar   *dest,
            guint   *pos,
            guint32  val)
{
	memset (dest + (*pos)++, val >> 24 & 0xff, 1);
	memset (dest + (*pos)++, val >> 16 & 0xff, 1);
	memset (dest + (*pos)++, val >>  8 & 0xff, 1);
	memset (dest + (*pos)++, val >>  0 & 0xff, 1);
}

static void
cur_setstr (gchar       *dest,
            guint       *pos,
            const gchar *str,
            gsize        len)
{
	memcpy (dest + *pos, str, len);
	(*pos) += len;
	memset (dest + (*pos)++, 0 & 0xff, 1);
}

void
tracker_db_journal_append_delete_statement (guint32      s_id,
                                            guint32      p_id,
                                            const gchar *object)
{
	gint o_len = strlen (object);
	gchar data_format = 0x04;
	gint size = (sizeof (guint32) * 3) + o_len + 1;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, data_format);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setstr (writer.cur_block, &writer.cur_pos, object, o_len);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;
}


void
tracker_db_journal_append_delete_statement_id (guint32 s_id,
                                               guint32 p_id,
                                               guint32 o_id)
{
	gchar data_format = 0x06;
	gint size = sizeof (guint32) * 4;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, data_format);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, o_id);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;
}

void
tracker_db_journal_append_insert_statement (guint32      s_id,
                                            guint32      p_id,
                                            const gchar *object)
{
	gint o_len = strlen (object);
	gchar data_format = 0x00;
	gint size = (sizeof (guint32) * 3) + o_len + 1;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, data_format);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setstr (writer.cur_block, &writer.cur_pos, object, o_len);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;
}

void
tracker_db_journal_append_insert_statement_id (guint32 s_id,
                                               guint32 p_id,
                                               guint32 o_id)
{
	gchar data_format = 0x02;
	gint size = sizeof (guint32) * 4;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, data_format);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, o_id);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;
}

void
tracker_db_journal_append_resource (guint32      s_id,
                                    const gchar *uri)
{
	gint o_len = strlen (uri);
	gchar data_format = 0x01;
	gint size = (sizeof (guint32) * 2) + o_len + 1;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, data_format);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setstr (writer.cur_block, &writer.cur_pos, uri, o_len);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;
}

void
tracker_db_journal_rollback_transaction (void)
{
	kill_cur_block ();
}

void
tracker_db_journal_commit_transaction (void)
{
	guint32 crc;
	guint begin_pos = 0;
	guint size = sizeof (guint32);
	guint offset = sizeof(guint32) * 3;

	g_assert (writer.journal);

	cur_block_maybe_expand (size);

	writer.cur_block_len += size;

	cur_setnum (writer.cur_block, &begin_pos, writer.cur_block_len);
	cur_setnum (writer.cur_block, &begin_pos, writer.cur_entry_amount);

	cur_setnum (writer.cur_block, &writer.cur_pos, writer.cur_block_len);

	/* CRC is calculated from entries until appended amount int */

	crc = tracker_crc32 (writer.cur_block + offset, writer.cur_block_len - offset);
	cur_setnum (writer.cur_block, &begin_pos, crc);

	write (fileno (writer.journal), writer.cur_block, writer.cur_block_len);

	writer.current_size += writer.cur_block_len;

	kill_cur_block ();
}

void 
tracker_db_journal_fsync (void)
{
	g_assert (writer.journal);

	fsync (fileno (writer.journal));
}

void
tracker_db_journal_close (void)
{
	g_assert (writer.journal);

	fclose (writer.journal);
	writer.journal = NULL;

	g_free (writer.filename);
	writer.filename = NULL;
}

void
tracker_db_journal_reader_init (const gchar *filen)
{
	if (!filen) {
		tracker_db_journal_filename ();
	} else {
		writer.filename = g_strdup (filen);
	}

	/* TODO error handling */
	journal_reader.file = g_mapped_file_new (writer.filename, FALSE, NULL);
	journal_reader.current = g_mapped_file_get_contents (journal_reader.file);
	journal_reader.end = journal_reader.current + g_mapped_file_get_length (journal_reader.file);

	/* verify journal file header */
	g_assert (journal_reader.end - journal_reader.current >= 8);
	g_assert (memcmp (journal_reader.current, "trlog\001", 8) == 0);
	journal_reader.current += 8;
}

static guint32 read_uint32 (const guint8 *data)
{
	return data[0] << 24 |
	       data[1] << 16 |
	       data[2] << 8 |
	       data[3];
}

gboolean
tracker_db_journal_next (void)
{
	if (journal_reader.type == TRACKER_DB_JOURNAL_START ||
	    journal_reader.type == TRACKER_DB_JOURNAL_END_TRANSACTION) {
		/* expect new transaction or end of file */

		guint32 entry_size;
		guint32 crc32;

		if (journal_reader.current >= journal_reader.end) {
			/* end of journal reached */
			return FALSE;
		}

		if (journal_reader.end - journal_reader.current < sizeof (guint32)) {
			/* damaged journal entry */
			return FALSE;
		}

		journal_reader.entry_begin = journal_reader.current;
		entry_size = read_uint32 (journal_reader.current);
		journal_reader.entry_end = journal_reader.entry_begin + entry_size;
		if (journal_reader.end < journal_reader.entry_end) {
			/* damaged journal entry */
			return FALSE;
		}
		journal_reader.current += 4;

		/* compare with entry_size at end */
		if (entry_size != read_uint32 (journal_reader.entry_end - 4)) {
			/* damaged journal entry */
			return FALSE;
		}

		journal_reader.amount_of_triples = read_uint32 (journal_reader.current);
		journal_reader.current += 4;

		crc32 = read_uint32 (journal_reader.current);
		journal_reader.current += 4;

		/* verify checksum */
		if (crc32 != tracker_crc32 (journal_reader.entry_begin, entry_size)) {
			/* damaged journal entry */
			return FALSE;
		}

		journal_reader.type = TRACKER_DB_JOURNAL_START_TRANSACTION;
		return TRUE;
	} else if (journal_reader.amount_of_triples == 0) {
		/* end of transaction */

		journal_reader.current += 4;
		if (journal_reader.current != journal_reader.entry_end) {
			/* damaged journal entry */
			return FALSE;
		}

		journal_reader.type = TRACKER_DB_JOURNAL_END_TRANSACTION;
		return TRUE;
	} else {
		guint32 data_format;
		gsize str_length;

		if (journal_reader.end - journal_reader.current < sizeof (guint32)) {
			/* damaged journal entry */
			return FALSE;
		}

		data_format = read_uint32 (journal_reader.current);
		journal_reader.current += 4;

		if (data_format == 1) {
			journal_reader.type = TRACKER_DB_JOURNAL_RESOURCE;

			if (journal_reader.end - journal_reader.current < sizeof (guint32) + 1) {
				/* damaged journal entry */
				return FALSE;
			}

			journal_reader.s_id = read_uint32 (journal_reader.current);
			journal_reader.current += 4;

			str_length = strnlen (journal_reader.current, journal_reader.end - journal_reader.current);
			if (str_length == journal_reader.end - journal_reader.current) {
				/* damaged journal entry (no terminating '\0' character) */
				return FALSE;
			}
			if (!g_utf8_validate (journal_reader.current, -1, NULL)) {
				/* damaged journal entry (invalid UTF-8) */
				return FALSE;
			}
			journal_reader.uri = journal_reader.current;
			journal_reader.current += str_length + 1;
		} else {
			if (data_format & 4) {
				if (data_format & 2) {
					journal_reader.type = TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID;
				} else {
					journal_reader.type = TRACKER_DB_JOURNAL_DELETE_STATEMENT;
				}
			} else {
				if (data_format & 2) {
					journal_reader.type = TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID;
				} else {
					journal_reader.type = TRACKER_DB_JOURNAL_INSERT_STATEMENT;
				}
			}

			if (journal_reader.end - journal_reader.current < 2 * sizeof (guint32)) {
				/* damaged journal entry */
				return FALSE;
			}

			journal_reader.s_id = read_uint32 (journal_reader.current);
			journal_reader.current += 4;

			journal_reader.p_id = read_uint32 (journal_reader.current);
			journal_reader.current += 4;

			if (data_format & 2) {
				if (journal_reader.end - journal_reader.current < sizeof (guint32)) {
					/* damaged journal entry */
					return FALSE;
				}

				journal_reader.o_id = read_uint32 (journal_reader.current);
				journal_reader.current += 4;
			} else {
				if (journal_reader.end - journal_reader.current < 1) {
					/* damaged journal entry */
					return FALSE;
				}

				str_length = strnlen (journal_reader.current, journal_reader.end - journal_reader.current);
				if (str_length == journal_reader.end - journal_reader.current) {
					/* damaged journal entry (no terminating '\0' character) */
					return FALSE;
				}
				if (!g_utf8_validate (journal_reader.current, -1, NULL)) {
					/* damaged journal entry (invalid UTF-8) */
					return FALSE;
				}
				journal_reader.object = journal_reader.current;
				journal_reader.current += str_length + 1;
			}
		}

		journal_reader.amount_of_triples--;
		return TRUE;
	}

	return FALSE;
}

TrackerDBJournalEntryType
tracker_db_journal_get_type (void)
{
	return journal_reader.type;
}

void
tracker_db_journal_get_resource (guint32      *id,
                                 const gchar **uri)
{
	g_return_if_fail (journal_reader.type == TRACKER_DB_JOURNAL_RESOURCE);

	*id = journal_reader.s_id;
	*uri = journal_reader.uri;
}

void
tracker_db_journal_get_statement (guint32      *s_id,
                                  guint32      *p_id,
                                  const gchar **object)
{
	g_return_if_fail (journal_reader.type == TRACKER_DB_JOURNAL_INSERT_STATEMENT ||
	                  journal_reader.type == TRACKER_DB_JOURNAL_DELETE_STATEMENT);

	*s_id = journal_reader.s_id;
	*p_id = journal_reader.p_id;
	*object = journal_reader.object;
}

void
tracker_db_journal_get_statement_id (guint32    *s_id,
                                     guint32    *p_id,
                                     guint32    *o_id)
{
	g_return_if_fail (journal_reader.type == TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID ||
	                  journal_reader.type == TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID);

	*s_id = journal_reader.s_id;
	*p_id = journal_reader.p_id;
	*o_id = journal_reader.o_id;
}
