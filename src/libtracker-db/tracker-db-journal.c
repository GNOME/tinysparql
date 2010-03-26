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

#include "config.h"

#define _GNU_SOURCE

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-crc32.h>

#include "tracker-db-journal.h"

#define JOURNAL_FILENAME  "tracker-store.journal"
#define MIN_BLOCK_SIZE    1024

/*
 * data_format:
 * #... 0000 0000 (total size is 4 bytes)
 *           |||`- resource insert (all other bits must be 0 if 1)
 *           ||`-- object type (1 = id, 0 = cstring)
 *           |`--- operation type (0 = insert, 1 = delete)
 *           `---- graph (0 = default graph, 1 = named graph)
 */

typedef enum {
	DATA_FORMAT_RESOURCE_INSERT  = 1 << 0,
	DATA_FORMAT_OBJECT_ID        = 1 << 1,
	DATA_FORMAT_OPERATION_DELETE = 1 << 2,
	DATA_FORMAT_GRAPH            = 1 << 3
} DataFormat;

typedef enum {
	TRANSACTION_FORMAT_DATA      = 1 << 0,
	TRANSACTION_FORMAT_ONTOLOGY  = 1 << 1,
} TransactionFormat;

static struct {
	gchar *filename;
	GMappedFile *file;
	const gchar *current;
	const gchar *end;
	const gchar *entry_begin;
	const gchar *entry_end;
	const gchar *last_success;
	const gchar *start;
	guint32 amount_of_triples;
	gint64 time;
	TrackerDBJournalEntryType type;
	const gchar *uri;
	gint g_id;
	gint s_id;
	gint p_id;
	gint o_id;
	const gchar *object;
} reader;

static struct {
	gchar *journal_filename;
	int journal;
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

static gboolean
write_all_data (int    fd, 
                gchar *data, 
                gsize  len)
{
	gssize written;
	gboolean result;

	result = FALSE;

	while (len > 0) {
		written = write (fd, data, len);
		
		if (written < 0) {
			if (errno == EAGAIN) {
				continue;
			}
			goto out;
		} else if (written == 0) {
			goto out; /* WTH? Don't loop forever*/
		}
		
		len -= written;
		data += written;
	}

	result = TRUE; /* Succeeded! */

out:

	return result;
}

GQuark
tracker_db_journal_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_DB_JOURNAL_ERROR_DOMAIN);
}

gboolean
tracker_db_journal_init (const gchar *filename, gboolean truncate)
{
	gchar *directory;
	struct stat st;
	int flags;
	int mode;

	g_return_val_if_fail (writer.journal == 0, FALSE);

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

	directory = g_path_get_dirname (writer.journal_filename);
	if (g_strcmp0 (directory, ".")) {
		mode = S_IRWXU | S_IRWXG | S_IRWXO;
		if (g_mkdir_with_parents (directory, mode)) {
			g_critical ("tracker data directory does not exist and "
				    "could not be created: %s",
				    g_strerror (errno));

			g_free (directory);
			g_free (writer.journal_filename);
			writer.journal_filename = NULL;

			return FALSE;
		}
	}
	g_free (directory);

	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	flags = O_WRONLY | O_APPEND | O_CREAT;
	if (truncate) {
		/* existing journal contents are invalid: reindex where journal
		 * does not even contain a single valid entry
		 *
		 * or should be ignored: only for test cases
		 */
		flags |= O_TRUNC;
	}
	writer.journal = g_open (writer.journal_filename, flags, mode);

	if (writer.journal == -1) {
		g_critical ("Could not open journal for writing, %s", 
		            g_strerror (errno));

		g_free (writer.journal_filename);
		writer.journal_filename = NULL;
		return FALSE;
	}

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
		writer.cur_block[7] = '2';

		if (!write_all_data (writer.journal, writer.cur_block, 8)) {
			g_free (writer.journal_filename);
			writer.journal_filename = NULL;
			return FALSE;
		}

		writer.cur_size += 8;
		cur_block_kill ();
	}

	return TRUE;
}

gboolean
tracker_db_journal_shutdown (void)
{
	if (writer.journal == 0) {
		return TRUE;
	}

	if (close (writer.journal) != 0) {
		g_warning ("Could not close journal, %s", 
		           g_strerror (errno));
		return FALSE;
	}

	writer.journal = 0;

	g_free (writer.journal_filename);
	writer.journal_filename = NULL;

	return TRUE;
}

gsize
tracker_db_journal_get_size (void)
{
	g_return_val_if_fail (writer.journal > 0, FALSE);

	return writer.cur_size;
}

const gchar *
tracker_db_journal_get_filename (void)
{
	/* Journal doesn't have to be open to get the filename, for example when
	 * the file didn't exist and it was attempted opened in only read mode. */
	return (const gchar*) writer.journal_filename;
}

gboolean
tracker_db_journal_start_transaction (time_t time)
{
	guint size;

	g_return_val_if_fail (writer.journal > 0, FALSE);

	size = sizeof (guint32) * 3;
	cur_block_maybe_expand (size);

	/* Leave space for size, amount and crc
	 * Check and keep in sync the offset variable at
	 * tracker_db_journal_commit_db_transaction too */

	memset (writer.cur_block, 0, size);

	writer.cur_pos = writer.cur_block_len = size;
	writer.cur_entry_amount = 0;

	/* add timestamp */
	cur_block_maybe_expand (sizeof (gint32));
	cur_setnum (writer.cur_block, &writer.cur_pos, time);
	writer.cur_block_len += sizeof (gint32);

	/* Add format */
	cur_block_maybe_expand (sizeof (gint32));
	cur_setnum (writer.cur_block, &writer.cur_pos, TRANSACTION_FORMAT_DATA);
	writer.cur_block_len += sizeof (gint32);

	return TRUE;
}


gboolean
tracker_db_journal_start_ontology_transaction (time_t time)
{
	guint size;

	g_return_val_if_fail (writer.journal > 0, FALSE);

	size = sizeof (guint32) * 3;
	cur_block_maybe_expand (size);

	/* Leave space for size, amount and crc
	 * Check and keep in sync the offset variable at
	 * tracker_db_journal_commit_db_transaction too */

	memset (writer.cur_block, 0, size);

	writer.cur_pos = writer.cur_block_len = size;
	writer.cur_entry_amount = 0;

	/* add timestamp */
	cur_block_maybe_expand (sizeof (gint32));
	cur_setnum (writer.cur_block, &writer.cur_pos, time);
	writer.cur_block_len += sizeof (gint32);

	/* Add format */
	cur_block_maybe_expand (sizeof (gint32));
	cur_setnum (writer.cur_block, &writer.cur_pos, TRANSACTION_FORMAT_ONTOLOGY);
	writer.cur_block_len += sizeof (gint32);

	return TRUE;
}

gboolean
tracker_db_journal_append_delete_statement (gint         g_id,
                                            gint         s_id,
                                            gint         p_id,
                                            const gchar *object)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal > 0, FALSE);
	g_return_val_if_fail (g_id >= 0, FALSE);
	g_return_val_if_fail (s_id > 0, FALSE);
	g_return_val_if_fail (p_id > 0, FALSE);
	g_return_val_if_fail (object != NULL, FALSE);

	o_len = strlen (object);
	if (g_id == 0) {
		df = DATA_FORMAT_OPERATION_DELETE;
		size = (sizeof (guint32) * 3) + o_len + 1;
	} else {
		df = DATA_FORMAT_OPERATION_DELETE | DATA_FORMAT_GRAPH;
		size = (sizeof (guint32) * 4) + o_len + 1;
	}

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	if (g_id > 0) {
		cur_setnum (writer.cur_block, &writer.cur_pos, g_id);
	}
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setstr (writer.cur_block, &writer.cur_pos, object, o_len);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_delete_statement_id (gint g_id,
                                               gint s_id,
                                               gint p_id,
                                               gint o_id)
{
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal > 0, FALSE);
	g_return_val_if_fail (g_id >= 0, FALSE);
	g_return_val_if_fail (s_id > 0, FALSE);
	g_return_val_if_fail (p_id > 0, FALSE);
	g_return_val_if_fail (o_id > 0, FALSE);

	if (g_id == 0) {
		df = DATA_FORMAT_OPERATION_DELETE | DATA_FORMAT_OBJECT_ID;
		size = sizeof (guint32) * 4;
	} else {
		df = DATA_FORMAT_OPERATION_DELETE | DATA_FORMAT_OBJECT_ID | DATA_FORMAT_GRAPH;
		size = sizeof (guint32) * 5;
	}

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	if (g_id > 0) {
		cur_setnum (writer.cur_block, &writer.cur_pos, g_id);
	}
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, o_id);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_insert_statement (gint         g_id,
                                            gint         s_id,
                                            gint         p_id,
                                            const gchar *object)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal > 0, FALSE);
	g_return_val_if_fail (g_id >= 0, FALSE);
	g_return_val_if_fail (s_id > 0, FALSE);
	g_return_val_if_fail (p_id > 0, FALSE);
	g_return_val_if_fail (object != NULL, FALSE);

	o_len = strlen (object);
	if (g_id == 0) {
		df = 0x00;
		size = (sizeof (guint32) * 3) + o_len + 1;
	} else {
		df = DATA_FORMAT_GRAPH;
		size = (sizeof (guint32) * 4) + o_len + 1;
	}

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	if (g_id > 0) {
		cur_setnum (writer.cur_block, &writer.cur_pos, g_id);
	}
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setstr (writer.cur_block, &writer.cur_pos, object, o_len);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_insert_statement_id (gint g_id,
                                               gint s_id,
                                               gint p_id,
                                               gint o_id)
{
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal > 0, FALSE);
	g_return_val_if_fail (g_id >= 0, FALSE);
	g_return_val_if_fail (s_id > 0, FALSE);
	g_return_val_if_fail (p_id > 0, FALSE);
	g_return_val_if_fail (o_id > 0, FALSE);

	if (g_id == 0) {
		df = DATA_FORMAT_OBJECT_ID;
		size = sizeof (guint32) * 4;
	} else {
		df = DATA_FORMAT_OBJECT_ID | DATA_FORMAT_GRAPH;
		size = sizeof (guint32) * 5;
	}

	cur_block_maybe_expand (size);

	cur_setnum (writer.cur_block, &writer.cur_pos, df);
	if (g_id > 0) {
		cur_setnum (writer.cur_block, &writer.cur_pos, g_id);
	}
	cur_setnum (writer.cur_block, &writer.cur_pos, s_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, p_id);
	cur_setnum (writer.cur_block, &writer.cur_pos, o_id);

	writer.cur_entry_amount++;
	writer.cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_resource (gint         s_id,
                                    const gchar *uri)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (writer.journal > 0, FALSE);

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
	g_return_val_if_fail (writer.journal > 0, FALSE);

	cur_block_kill ();

	return TRUE;
}

gboolean
tracker_db_journal_truncate (gsize new_size)
{
	g_return_val_if_fail (writer.journal > 0, FALSE);

	return (ftruncate (writer.journal, new_size) != -1);
}

gboolean
tracker_db_journal_commit_db_transaction (void)
{
	guint32 crc;
	guint begin_pos;
	guint size;
	guint offset;

	g_return_val_if_fail (writer.journal > 0, FALSE);

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

	if (!write_all_data (writer.journal, writer.cur_block, writer.cur_block_len)) {
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
	g_return_val_if_fail (writer.journal > 0, FALSE);

	return fsync (writer.journal) == 0;
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

	reader.last_success = reader.start = reader.current = 
		g_mapped_file_get_contents (reader.file);

	reader.end = reader.current + g_mapped_file_get_length (reader.file);

	/* verify journal file header */
	if (reader.end - reader.current < 8) {
		tracker_db_journal_reader_shutdown ();
		return FALSE;
	}

	if (memcmp (reader.current, "trlog\00002", 8)) {
		tracker_db_journal_reader_shutdown ();
		return FALSE;
	}

	reader.current += 8;

	return TRUE;
}

gsize
tracker_db_journal_reader_get_size_of_correct (void)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);

	return (gsize) (reader.last_success - reader.start);
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

	reader.last_success = NULL;
	reader.start = NULL;
	reader.current = NULL;
	reader.end = NULL;
	reader.entry_begin = NULL;
	reader.entry_end = NULL;
	reader.amount_of_triples = 0;
	reader.type = TRACKER_DB_JOURNAL_START;
	reader.uri = NULL;
	reader.g_id = 0;
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
tracker_db_journal_reader_verify_last (GError **error)
{
	guint32 entry_size_check;
	gboolean success = FALSE;

	if (tracker_db_journal_reader_init (NULL)) {
		entry_size_check = read_uint32 (reader.end - 4);

		if (reader.end - entry_size_check < reader.current) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry at end of journal");
			return FALSE;
		}

		reader.current = reader.end - entry_size_check;
		success = tracker_db_journal_reader_next (NULL);
		tracker_db_journal_reader_shutdown ();
	}

	return success;
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
	 *    [time]
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
		guint32 crc;
		guint32 crc_check;
		TransactionFormat t_kind;

		/* Check the end is not before where we currently are */
		if (reader.current >= reader.end) {
			/* Return FALSE as there is no further entry but
			 * do not set error as it's not an error case.
			 */
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

		/* Check that entry is big enough for header and footer */
		if (entry_size < 5 * sizeof (guint32)) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0,
			             "Damaged journal entry, size %d < 5 * sizeof(guint32)",
			             (gint) entry_size);
			return FALSE;
		}

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

		/* Read entry size check at the end of the entry */
		entry_size_check = read_uint32 (reader.entry_end - 4);

		if (entry_size != entry_size_check) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %d != %d (entry size != entry size check)", 
			             entry_size, 
			             entry_size_check);
			return FALSE;
		}

		/* Read the amount of triples */
		reader.amount_of_triples = read_uint32 (reader.current);
		reader.current += 4;

		/* Read the crc */
		crc_check = read_uint32 (reader.current);
		reader.current += 4;

		/* Calculate the crc */
		crc = tracker_crc32 (reader.entry_begin + (sizeof (guint32) * 3), entry_size - (sizeof (guint32) * 3));

		/* Verify checksum */
		if (crc != crc_check) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, 0x%.8x != 0x%.8x (crc32 failed)",
			             crc,
			             crc_check);
			return FALSE;
		}

		/* Read the timestamp */
		reader.time = read_uint32 (reader.current);
		reader.current += 4;

		t_kind = read_uint32 (reader.current);
		reader.current += 4;

		if (t_kind == TRANSACTION_FORMAT_DATA)
			reader.type = TRACKER_DB_JOURNAL_START_TRANSACTION;
		else
			reader.type = TRACKER_DB_JOURNAL_START_ONTOLOGY_TRANSACTION;

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
		reader.last_success = reader.current;

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

			if (df & DATA_FORMAT_GRAPH) {
				if (reader.end - reader.current < sizeof (guint32)) {
					/* damaged journal entry */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
						     "Damaged journal entry, %d < sizeof(guint32)",
						     (gint) (reader.end - reader.current));
					return FALSE;
				}

				/* named graph */
				reader.g_id = read_uint32 (reader.current);
				reader.current += 4;
			} else {
				/* default graph */
				reader.g_id = 0;
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

gint64
tracker_db_journal_reader_get_time (void)
{
	return reader.time;
}

gboolean
tracker_db_journal_reader_get_resource (gint         *id,
                                        const gchar **uri)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);
	g_return_val_if_fail (reader.type == TRACKER_DB_JOURNAL_RESOURCE, FALSE);

	*id = reader.s_id;
	*uri = reader.uri;

	return TRUE;
}

gboolean
tracker_db_journal_reader_get_statement (gint         *g_id,
                                         gint         *s_id,
                                         gint         *p_id,
                                         const gchar **object)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);
	g_return_val_if_fail (reader.type == TRACKER_DB_JOURNAL_INSERT_STATEMENT ||
	                      reader.type == TRACKER_DB_JOURNAL_DELETE_STATEMENT,
	                      FALSE);

	if (g_id) {
		*g_id = reader.g_id;
	}
	*s_id = reader.s_id;
	*p_id = reader.p_id;
	*object = reader.object;

	return TRUE;
}

gboolean
tracker_db_journal_reader_get_statement_id (gint *g_id,
                                            gint *s_id,
                                            gint *p_id,
                                            gint *o_id)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);
	g_return_val_if_fail (reader.type == TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID ||
	                      reader.type == TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID,
	                      FALSE);

	if (g_id) {
		*g_id = reader.g_id;
	}
	*s_id = reader.s_id;
	*p_id = reader.p_id;
	*o_id = reader.o_id;

	return TRUE;
}

gdouble
tracker_db_journal_reader_get_progress (void)
{
	gdouble percent = ((gdouble)(reader.end - reader.start));
	return ((gdouble)(reader.current - reader.start)) / percent;
}

