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
