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
#include <errno.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-crc32.h>

#include "tracker-db-journal.h"

#define JOURNAL_FILENAME  "tracker-store.journal"
#define MIN_BLOCK_SIZE    1024

/*
 * data_format:
 * #... 0000 0000 (total size is 4 bytes)
 *            ||`- resource insert (all other bits must be 0 if 1)
 *            |`-- object type (1 = id, 0 = cstring)
 *            `--- operation type (0 = insert, 1 = delete)
 */

typedef enum {
	DATA_FORMAT_RESOURCE_INSERT  = 1 << 0,
	DATA_FORMAT_OBJECT_ID        = 1 << 1,
	DATA_FORMAT_OPERATION_DELETE = 1 << 2
} DataFormat;

static struct {
	gchar *filename;
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
} reader;

static struct {
	gchar *journal_filename;
	FILE *journal;
	gsize cur_size;
	guint cur_block_len;
	guint cur_block_alloc;
	gchar *cur_block;
	guint cur_entry_amount;
	guint cur_pos;
} writer;

static guint32
read_uint32 (const guint8 *data)
{
	return data[0] << 24 |
	       data[1] << 16 |
	       data[2] << 8 |
	       data[3];
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

static void
cur_block_kill (void)
{
	writer.cur_block_len = 0;
	writer.cur_pos = 0;
	writer.cur_entry_amount = 0;
	writer.cur_block_alloc = 0;

	g_free (writer.cur_block);
	writer.cur_block = NULL;
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

GQuark
tracker_db_journal_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_DB_JOURNAL_ERROR_DOMAIN);
}

gboolean
tracker_db_journal_init (const gchar *filename)
{
	struct stat st;

	g_return_val_if_fail (writer.journal == NULL, FALSE);

	writer.cur_block_len = 0;
	writer.cur_pos = 0;
	writer.cur_entry_amount = 0;
	writer.cur_block_alloc = 0;
	writer.cur_block = NULL;

	/* Used mostly for testing */
	if (G_UNLIKELY (filename)) {
		writer.journal_filename = g_strdup (filename);
	} else {
		writer.journal_filename = g_build_filename (g_get_user_data_dir (),
		                                            "tracker",
		                                            "data",
		                                            JOURNAL_FILENAME,
		                                            NULL);
	}

	writer.journal = g_fopen (writer.journal_filename, "a");

	if (g_stat (writer.journal_filename, &st) == 0) {
		writer.cur_size = (gsize) st.st_size;
	}

	if (writer.cur_size == 0) {
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

		writer.cur_size += 8;

		cur_block_kill ();
	}

	return TRUE;
}

gboolean
tracker_db_journal_shutdown (void)
{
	if (writer.journal == NULL) {
		return TRUE;
	}

	fclose (writer.journal);
	writer.journal = NULL;

	g_free (writer.journal_filename);
	writer.journal_filename = NULL;

	return TRUE;
}

gsize
tracker_db_journal_get_size (void)
{
	g_return_val_if_fail (writer.journal != NULL, FALSE);

	return writer.cur_size;
}

const gchar *
tracker_db_journal_get_filename (void)
{
	g_return_val_if_fail (writer.journal != NULL, FALSE);

	return (const gchar*) writer.journal_filename;
}

gboolean
tracker_db_journal_start_transaction (void)
{
	guint size;

	g_return_val_if_fail (writer.journal != NULL, FALSE);

	size = sizeof (guint32) * 3;
	cur_block_maybe_expand (size);

	/* Leave space for size, amount and crc
	 * Check and keep in sync the offset variable at
	 * tracker_db_journal_commit_transaction too */

	memset (writer.cur_block, 0, size);

	writer.cur_pos = writer.cur_block_len = size;
	writer.cur_entry_amount = 0;

	return TRUE;
}

gboolean
tracker_db_journal_append_delete_statement (guint32      s_id,
                                            guint32      p_id,
                                            const gchar *object)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal != NULL, FALSE);

	o_len = strlen (object);
	df = DATA_FORMAT_OPERATION_DELETE;

	size = (sizeof (guint32) * 3) + o_len + 1;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setstr (writer.cur_block, &writer.cur_pos, object, o_len);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_delete_statement_id (guint32 s_id,
                                               guint32 p_id,
                                               guint32 o_id)
{
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal != NULL, FALSE);

	df = DATA_FORMAT_OPERATION_DELETE | DATA_FORMAT_OBJECT_ID;
	size = sizeof (guint32) * 4;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, o_id);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_insert_statement (guint32      s_id,
                                            guint32      p_id,
                                            const gchar *object)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal != NULL, FALSE);

	o_len = strlen (object);
	df = 0x00;
	size = (sizeof (guint32) * 3) + o_len + 1;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setstr (writer.cur_block, &writer.cur_pos, object, o_len);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_insert_statement_id (guint32 s_id,
                                               guint32 p_id,
                                               guint32 o_id)
{
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal != NULL, FALSE);

	df = DATA_FORMAT_OBJECT_ID;
	size = sizeof (guint32) * 4;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, o_id);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_resource (guint32      s_id,
                                    const gchar *uri)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal != NULL, FALSE);

	o_len = strlen (uri);
	df = DATA_FORMAT_RESOURCE_INSERT;
	size = (sizeof (guint32) * 2) + o_len + 1;

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setstr (writer.cur_block, &writer.cur_pos, uri, o_len);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_rollback_transaction (void)
{
	g_return_val_if_fail (writer.journal != NULL, FALSE);

	cur_block_kill ();

	return TRUE;
}

gboolean
tracker_db_journal_commit_transaction (void)
{
	guint32 crc;
	guint begin_pos;
	guint size;
	guint offset;

	g_return_val_if_fail (writer.journal != NULL, FALSE);

	begin_pos = 0;
	size = sizeof (guint32);
	offset = sizeof (guint32) * 3;

	/* Expand by uint32 for the size check at the end of the entry */
	cur_block_maybe_expand (size);

	writer.cur_block_len += size;

	/* Write size and amount */
	cur_setnum (writer.cur_block, &begin_pos, writer.cur_block_len);
	cur_setnum (writer.cur_block, &begin_pos, writer.cur_entry_amount);

	/* Write size check to end of current journal data */
	cur_setnum (writer.cur_block, &writer.cur_pos, writer.cur_block_len);

	/* Calculate CRC from entry triples start (i.e. without size,
	 * amount and crc) until the end of the entry block.
	 *
	 * NOTE: the size check at the end is included in the CRC!
	 */
	crc = tracker_crc32 (writer.cur_block + offset, writer.cur_block_len - offset);
	cur_setnum (writer.cur_block, &begin_pos, crc);

	/* FIXME: What if we don't write all of len, needs improving. */
	if (write (fileno (writer.journal), writer.cur_block, writer.cur_block_len) == -1) {
		g_critical ("Could not write to journal, %s", g_strerror (errno));
		return FALSE;
	}

	/* Update journal size */
	writer.cur_size += writer.cur_block_len;

	/* Clean up for next transaction */
	cur_block_kill ();

	return TRUE;
}

gboolean
tracker_db_journal_fsync (void)
{
	g_return_val_if_fail (writer.journal != NULL, FALSE);

	return fsync (fileno (writer.journal)) == 0;
}

/*
 * Reader API
 */
gboolean
tracker_db_journal_reader_init (const gchar *filename)
{
	GError *error = NULL;
	gchar *filename_used;

	g_return_val_if_fail (reader.file == NULL, FALSE);

	/* Used mostly for testing */
	if (G_UNLIKELY (filename)) {
		filename_used = g_strdup (filename);
	} else {
		filename_used = g_build_filename (g_get_user_data_dir (),
		                                  "tracker",
		                                  "data",
		                                  JOURNAL_FILENAME,
		                                  NULL);
	}

	reader.type = TRACKER_DB_JOURNAL_START;
	reader.filename = filename_used;
	reader.file = g_mapped_file_new (reader.filename, FALSE, &error);

	if (error) {
		if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
			/* do not warn if the file does not exist, just return FALSE */
			g_warning ("Could not create TrackerDBJournalReader for file '%s', %s",
				   reader.filename,
				   error->message ? error->message : "no error given");
		}
		g_error_free (error);
		g_free (reader.filename);
		reader.filename = NULL;

		return FALSE;
	}

	reader.current = g_mapped_file_get_contents (reader.file);
	reader.end = reader.current + g_mapped_file_get_length (reader.file);

	/* verify journal file header */
	g_assert (reader.end - reader.current >= 8);

	g_assert_cmpint (reader.current[0], ==, 't');
	g_assert_cmpint (reader.current[1], ==, 'r');
	g_assert_cmpint (reader.current[2], ==, 'l');
	g_assert_cmpint (reader.current[3], ==, 'o');
	g_assert_cmpint (reader.current[4], ==, 'g');
	g_assert_cmpint (reader.current[5], ==, '\0');
	g_assert_cmpint (reader.current[6], ==, '0');
	g_assert_cmpint (reader.current[7], ==, '1');

	reader.current += 8;

	return TRUE;
}

gboolean
tracker_db_journal_reader_shutdown (void)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);

#if GLIB_CHECK_VERSION(2,22,0)
	g_mapped_file_unref (reader.file);
#else
	g_mapped_file_free (reader.file);
#endif

	reader.file = NULL;

	g_free (reader.filename);
	reader.filename = NULL;

	reader.current = NULL;
	reader.end = NULL;
	reader.entry_begin = NULL;
	reader.entry_end = NULL;
	reader.amount_of_triples = 0;
	reader.type = TRACKER_DB_JOURNAL_START;
	reader.uri = NULL;
	reader.s_id = 0;
	reader.p_id = 0;
	reader.o_id = 0;
	reader.object = NULL;

	return TRUE;
}

TrackerDBJournalEntryType
tracker_db_journal_reader_get_type (void)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);

	return reader.type;
}

gboolean
tracker_db_journal_reader_next (GError **error)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);

	/*
	 * Visual layout of the data in the binary journal:
	 *
	 * [
	 *  [magic]
	 *  [version]
	 *  [
	 *   [entry 
	 *    [size]
	 *    [amount]
	 *    [crc]
	 *    [id id id]
	 *    [id id string]
	 *    [id ...]
	 *    [size]
	 *   ]
	 *   [entry...]
	 *   [entry...]
	 *  ]
	 * ]
	 *
	 * Note: We automatically start at the first entry, upon init
	 * of the reader, we move past the [magic] and the [version].
	 */

	if (reader.type == TRACKER_DB_JOURNAL_START ||
	    reader.type == TRACKER_DB_JOURNAL_END_TRANSACTION) {
		/* Expect new transaction or end of file */
		guint32 entry_size;
		guint32 entry_size_check;
		guint32 crc32;

		/* Check the end is not before where we currently are */
		if (reader.current >= reader.end) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "End of journal reached");
			return FALSE;
		}

		/* Check the end is not smaller than the first uint32
		 * for reading the entry size.
		 */
		if (reader.end - reader.current < sizeof (guint32)) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %d < sizeof(guint32) at start/end of journal",
			             (gint) (reader.end - reader.current));
			return FALSE;
		}

		/* Read the first uint32 which contains the size */
		entry_size = read_uint32 (reader.current);

		/* Set the bounds for the entry */
		reader.entry_begin = reader.current;
		reader.entry_end = reader.entry_begin + entry_size;

		/* Check the end of the entry does not exceed the end
		 * of the journal.
		 */
		if (reader.end < reader.entry_end) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, end < entry end");
			return FALSE;
		}

		/* Move the current potision of the journal past the
		 * entry size we read earlier.
		 */
		reader.current += 4;

		/* compare with entry_size at end */
		entry_size_check = read_uint32 (reader.entry_end - 4);

		if (entry_size != entry_size_check) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %d != %d (entry size != entry size check)", 
			             entry_size, 
			             entry_size_check);
			return FALSE;
		}

		reader.amount_of_triples = read_uint32 (reader.current);
		reader.current += 4;

		crc32 = read_uint32 (reader.current);
		reader.current += 4;

		/* verify checksum */
		if (crc32 != tracker_crc32 (reader.entry_begin, entry_size)) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, crc32 failed");
			return FALSE;
		}

		reader.type = TRACKER_DB_JOURNAL_START_TRANSACTION;
		return TRUE;
	} else if (reader.amount_of_triples == 0) {
		/* end of transaction */

		reader.current += 4;
		if (reader.current != reader.entry_end) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %p != %p (end of transaction with 0 triples)",
			             reader.current,
			             reader.entry_end);
			return FALSE;
		}

		reader.type = TRACKER_DB_JOURNAL_END_TRANSACTION;
		return TRUE;
	} else {
		DataFormat df;
		gsize str_length;

		if (reader.end - reader.current < sizeof (guint32)) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %d < sizeof(guint32)",
			             (gint) (reader.end - reader.current));
			return FALSE;
		}

		df = read_uint32 (reader.current);
		reader.current += 4;

		if (df == DATA_FORMAT_RESOURCE_INSERT) {
			reader.type = TRACKER_DB_JOURNAL_RESOURCE;

			if (reader.end - reader.current < sizeof (guint32) + 1) {
				/* damaged journal entry */
				g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
				             "Damaged journal entry, %d < sizeof(guint32) + 1 for resource",
				             (gint) (reader.end - reader.current));
				return FALSE;
			}

			reader.s_id = read_uint32 (reader.current);
			reader.current += 4;

			str_length = strnlen (reader.current, reader.end - reader.current);
			if (str_length == reader.end - reader.current) {
				/* damaged journal entry (no terminating '\0' character) */
				g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
				             "Damaged journal entry, no terminating zero found for resource");
				return FALSE;

			}

			if (!g_utf8_validate (reader.current, -1, NULL)) {
				/* damaged journal entry (invalid UTF-8) */
				g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
				             "Damaged journal entry, invalid UTF-8 for resource");
				return FALSE;
			}

			reader.uri = reader.current;
			reader.current += str_length + 1;
		} else {
			if (df & DATA_FORMAT_OPERATION_DELETE) {
				if (df & DATA_FORMAT_OBJECT_ID) {
					reader.type = TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID;
				} else {
					reader.type = TRACKER_DB_JOURNAL_DELETE_STATEMENT;
				}
			} else {
				if (df & DATA_FORMAT_OBJECT_ID) {
					reader.type = TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID;
				} else {
					reader.type = TRACKER_DB_JOURNAL_INSERT_STATEMENT;
				}
			}

			if (reader.end - reader.current < 2 * sizeof (guint32)) {
				/* damaged journal entry */
				g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
				             "Damaged journal entry, %d < 2 * sizeof(guint32)",
				             (gint) (reader.end - reader.current));
				return FALSE;
			}

			reader.s_id = read_uint32 (reader.current);
			reader.current += 4;

			reader.p_id = read_uint32 (reader.current);
			reader.current += 4;

			if (df & DATA_FORMAT_OBJECT_ID) {
				if (reader.end - reader.current < sizeof (guint32)) {
					/* damaged journal entry */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
					             "Damaged journal entry, %d < sizeof(guint32) for data format 2",
					             (gint) (reader.end - reader.current));
					return FALSE;
				}

				reader.o_id = read_uint32 (reader.current);
				reader.current += 4;
			} else {
				if (reader.end - reader.current < 1) {
					/* damaged journal entry */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
					             "Damaged journal entry, %d < 1",
					             (gint) (reader.end - reader.current));
					return FALSE;
				}

				str_length = strnlen (reader.current, reader.end - reader.current);
				if (str_length == reader.end - reader.current) {
					/* damaged journal entry (no terminating '\0' character) */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
					             "Damaged journal entry, no terminating zero found");
					return FALSE;
				}

				if (!g_utf8_validate (reader.current, -1, NULL)) {
					/* damaged journal entry (invalid UTF-8) */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
					             "Damaged journal entry, invalid UTF-8");
					return FALSE;
				}

				reader.object = reader.current;
				reader.current += str_length + 1;
			}
		}

		reader.amount_of_triples--;
		return TRUE;
	}

	g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, "Unknown reason");

	return FALSE;
}

gboolean
tracker_db_journal_reader_get_resource (guint32      *id,
                                        const gchar **uri)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);
	g_return_val_if_fail (reader.type == TRACKER_DB_JOURNAL_RESOURCE, FALSE);

	*id = reader.s_id;
	*uri = reader.uri;

	return TRUE;
}

gboolean
tracker_db_journal_reader_get_statement (guint32      *s_id,
                                         guint32      *p_id,
                                         const gchar **object)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);
	g_return_val_if_fail (reader.type == TRACKER_DB_JOURNAL_INSERT_STATEMENT ||
	                      reader.type == TRACKER_DB_JOURNAL_DELETE_STATEMENT,
	                      FALSE);

	*s_id = reader.s_id;
	*p_id = reader.p_id;
	*object = reader.object;

	return TRUE;
}

gboolean
tracker_db_journal_reader_get_statement_id (guint32 *s_id,
                                            guint32 *p_id,
                                            guint32 *o_id)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);
	g_return_val_if_fail (reader.type == TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID ||
	                      reader.type == TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID,
	                      FALSE);

	*s_id = reader.s_id;
	*p_id = reader.p_id;
	*o_id = reader.o_id;

	return TRUE;
}
