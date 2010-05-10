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
#include <stdlib.h>

#include <glib/gstdio.h>

#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif

#include <libtracker-common/tracker-crc32.h>

#include "tracker-db-journal.h"

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

typedef struct {
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
	guint current_file;
	gchar *rotate_to;
} JournalReader;

typedef struct {
	gchar *journal_filename;
	int journal;
	gsize cur_size;
	guint cur_block_len;
	guint cur_block_alloc;
	gchar *cur_block;
	guint cur_entry_amount;
	guint cur_pos;
} JournalWriter;

static struct {
	gsize chunk_size;
	gboolean do_rotating;
	gchar *rotate_to;
} rotating_settings = {0};

static JournalReader reader = {0};
static JournalWriter writer = {0};

static gboolean tracker_db_journal_rotate (void);

static guint32
read_uint32 (const guint8 *data)
{
	return data[0] << 24 |
	       data[1] << 16 |
	       data[2] << 8 |
	       data[3];
}

void
tracker_db_journal_get_rotating (gboolean *do_rotating,
                                 gsize    *chunk_size,
                                 gchar   **rotate_to)
{
	*do_rotating = rotating_settings.do_rotating;
	*chunk_size = rotating_settings.chunk_size;
	if (rotating_settings.rotate_to) {
		*rotate_to = g_strdup (rotating_settings.rotate_to);
	} else {
		*rotate_to = NULL;
	}
}

void
tracker_db_journal_set_rotating (gboolean     do_rotating,
                                 gsize        chunk_size,
                                 const gchar *rotate_to)
{
	rotating_settings.do_rotating = do_rotating;
	rotating_settings.chunk_size = chunk_size;
	g_free (rotating_settings.rotate_to);
	if (rotate_to) {
		rotating_settings.rotate_to = g_strdup (rotate_to);
	} else {
		rotating_settings.rotate_to = NULL;
	}
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
cur_block_maybe_expand (JournalWriter *jwriter, guint len)
{
	guint want_alloc = jwriter->cur_block_len + len;

	if (want_alloc > jwriter->cur_block_alloc) {
		want_alloc = nearest_pow (want_alloc);
		want_alloc = MAX (want_alloc, MIN_BLOCK_SIZE);
		jwriter->cur_block = g_realloc (jwriter->cur_block, want_alloc);
		jwriter->cur_block_alloc = want_alloc;
	}
}

static void
cur_block_kill (JournalWriter *jwriter)
{
	jwriter->cur_block_len = 0;
	jwriter->cur_pos = 0;
	jwriter->cur_entry_amount = 0;
	jwriter->cur_block_alloc = 0;

	g_free (jwriter->cur_block);
	jwriter->cur_block = NULL;
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

static gboolean
db_journal_init_file (JournalWriter *jwriter, gboolean truncate)
{
	struct stat st;
	int flags;
	int mode;

	jwriter->cur_block_len = 0;
	jwriter->cur_pos = 0;
	jwriter->cur_entry_amount = 0;
	jwriter->cur_block_alloc = 0;
	jwriter->cur_block = NULL;

	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	flags = O_WRONLY | O_APPEND | O_CREAT | O_LARGEFILE;
	if (truncate) {
		/* existing journal contents are invalid: reindex where journal
		 * does not even contain a single valid entry
		 *
		 * or should be ignored: only for test cases
		 */
		flags |= O_TRUNC;
	}

	jwriter->journal = g_open (jwriter->journal_filename, flags, mode);

	if (jwriter->journal == -1) {
		g_critical ("Could not open journal for writing, %s", 
		            g_strerror (errno));

		g_free (jwriter->journal_filename);
		jwriter->journal_filename = NULL;
		return FALSE;
	}

	if (g_stat (jwriter->journal_filename, &st) == 0) {
		jwriter->cur_size = (gsize) st.st_size;
	}

	if (jwriter->cur_size == 0) {
		g_assert (jwriter->cur_block_len == 0);
		g_assert (jwriter->cur_block_alloc == 0);
		g_assert (jwriter->cur_block == NULL);
		g_assert (jwriter->cur_block == NULL);

		cur_block_maybe_expand (jwriter, 8);

		jwriter->cur_block[0] = 't';
		jwriter->cur_block[1] = 'r';
		jwriter->cur_block[2] = 'l';
		jwriter->cur_block[3] = 'o';
		jwriter->cur_block[4] = 'g';
		jwriter->cur_block[5] = '\0';
		jwriter->cur_block[6] = '0';
		jwriter->cur_block[7] = '3';

		if (!write_all_data (jwriter->journal, jwriter->cur_block, 8)) {
			g_free (jwriter->journal_filename);
			jwriter->journal_filename = NULL;
			return FALSE;
		}

		jwriter->cur_size += 8;
		cur_block_kill (jwriter);
	}

	return TRUE;
}

static gboolean
db_journal_writer_init (JournalWriter *jwriter,
                        gboolean       truncate,
                        gboolean       global_writer,
                        const gchar   *filename)
{
	gchar *directory;
	gint mode;

	directory = g_path_get_dirname (filename);
	if (g_strcmp0 (directory, ".")) {
		mode = S_IRWXU | S_IRWXG | S_IRWXO;
		if (g_mkdir_with_parents (directory, mode)) {
			g_critical ("tracker data directory does not exist and "
			            "could not be created: %s",
			            g_strerror (errno));
			g_free (directory);
			return FALSE;
		}
	}
	g_free (directory);

	jwriter->journal_filename = g_strdup (filename);

	return db_journal_init_file (jwriter, truncate);
}

gboolean
tracker_db_journal_init (const gchar *filename,
                         gboolean     truncate)
{
	gboolean ret;
	const gchar *filename_use;
	gchar *filename_free = NULL;

	g_return_val_if_fail (writer.journal == 0, FALSE);

	if (filename == NULL) {
		/* Used mostly for testing */
		filename_use = g_build_filename (g_get_user_data_dir (),
		                                 "tracker",
		                                 "data",
		                                 TRACKER_DB_JOURNAL_FILENAME,
		                                 NULL);
		filename_free = (gchar *) filename_use;
	} else {
		filename_use = filename;
	}

	ret = db_journal_writer_init (&writer, truncate, TRUE, filename_use);
	g_free (filename_free);

	return ret;
}

static gboolean
db_journal_writer_shutdown (JournalWriter *jwriter)
{
	g_free (jwriter->journal_filename);
	jwriter->journal_filename = NULL;

	if (jwriter->journal == 0) {
		return TRUE;
	}

	if (close (jwriter->journal) != 0) {
		g_warning ("Could not close journal, %s", 
		           g_strerror (errno));
		return FALSE;
	}

	jwriter->journal = 0;

	return TRUE;
}

gboolean
tracker_db_journal_shutdown (void)
{
	return db_journal_writer_shutdown (&writer);
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

static gboolean
db_journal_writer_start_transaction (JournalWriter    *jwriter,
                                     time_t            time,
                                     TransactionFormat kind)
{
	guint size;

	g_return_val_if_fail (jwriter->journal > 0, FALSE);

	size = sizeof (guint32) * 3;
	cur_block_maybe_expand (jwriter, size);

	/* Leave space for size, amount and crc
	 * Check and keep in sync the offset variable at
	 * tracker_db_journal_commit_db_transaction too */

	memset (jwriter->cur_block, 0, size);

	jwriter->cur_pos = jwriter->cur_block_len = size;
	jwriter->cur_entry_amount = 0;

	/* add timestamp */
	cur_block_maybe_expand (jwriter, sizeof (gint32));
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), time);
	jwriter->cur_block_len += sizeof (gint32);

	/* Add format */
	cur_block_maybe_expand (jwriter, sizeof (gint32));
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), kind);
	jwriter->cur_block_len += sizeof (gint32);

	return TRUE;
}

gboolean
tracker_db_journal_start_transaction (time_t time)
{
	return db_journal_writer_start_transaction (&writer, time,
	                                            TRANSACTION_FORMAT_DATA);
}

gboolean
tracker_db_journal_start_ontology_transaction (time_t time)
{
	return db_journal_writer_start_transaction (&writer, time,
	                                            TRANSACTION_FORMAT_ONTOLOGY);
}

static gboolean
db_journal_writer_append_delete_statement (JournalWriter *jwriter,
                                           gint           g_id,
                                           gint           s_id,
                                           gint           p_id,
                                           const gchar   *object)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (jwriter->journal > 0, FALSE);
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

	cur_block_maybe_expand (jwriter, size);

	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), df);
	if (g_id > 0) {
		cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), g_id);
	}
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), s_id);
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), p_id);
	cur_setstr (jwriter->cur_block, &(jwriter->cur_pos), object, o_len);

	jwriter->cur_entry_amount++;
	jwriter->cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_delete_statement (gint         g_id,
                                            gint         s_id,
                                            gint         p_id,
                                            const gchar *object)
{
	return db_journal_writer_append_delete_statement (&writer,
	                                                  g_id, s_id, p_id, object);
}

static gboolean
db_journal_writer_append_delete_statement_id  (JournalWriter *jwriter,
                                               gint           g_id,
                                               gint           s_id,
                                               gint           p_id,
                                               gint           o_id)
{
	DataFormat df;
	gint size;

	g_return_val_if_fail (jwriter->journal > 0, FALSE);
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

	cur_block_maybe_expand (jwriter, size);

	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), df);
	if (g_id > 0) {
		cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), g_id);
	}
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), s_id);
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), p_id);
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), o_id);

	jwriter->cur_entry_amount++;
	jwriter->cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_delete_statement_id (gint g_id,
                                               gint s_id,
                                               gint p_id,
                                               gint o_id)
{
	return db_journal_writer_append_delete_statement_id (&writer,
	                                                     g_id, s_id, p_id, o_id);
}

static gboolean
db_journal_writer_append_insert_statement (JournalWriter *jwriter,
                                           gint           g_id,
                                           gint           s_id,
                                           gint           p_id,
                                           const gchar   *object)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (jwriter->journal > 0, FALSE);
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

	cur_block_maybe_expand (jwriter, size);

	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), df);
	if (g_id > 0) {
		cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), g_id);
	}
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), s_id);
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), p_id);
	cur_setstr (jwriter->cur_block, &(jwriter->cur_pos), object, o_len);

	jwriter->cur_entry_amount++;
	jwriter->cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_insert_statement (gint         g_id,
                                            gint         s_id,
                                            gint         p_id,
                                            const gchar *object)
{
	return db_journal_writer_append_insert_statement (&writer,
	                                                  g_id, s_id, p_id, object);
}

static gboolean
db_journal_writer_append_insert_statement_id (JournalWriter *jwriter,
                                              gint           g_id,
                                              gint           s_id,
                                              gint           p_id,
                                              gint           o_id)
{
	DataFormat df;
	gint size;

	g_return_val_if_fail (jwriter->journal > 0, FALSE);
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

	cur_block_maybe_expand (jwriter, size);

	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), df);
	if (g_id > 0) {
		cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), g_id);
	}
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), s_id);
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), p_id);
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), o_id);

	jwriter->cur_entry_amount++;
	jwriter->cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_insert_statement_id (gint g_id,
                                               gint s_id,
                                               gint p_id,
                                               gint o_id)
{
	return db_journal_writer_append_insert_statement_id (&writer,
	                                                     g_id, s_id, p_id, o_id);
}

static gboolean
db_journal_writer_append_resource (JournalWriter *jwriter,
                                   gint           s_id,
                                   const gchar   *uri)
{
	gint o_len;
	DataFormat df;
	gint size;

	g_return_val_if_fail (jwriter->journal > 0, FALSE);

	o_len = strlen (uri);
	df = DATA_FORMAT_RESOURCE_INSERT;
	size = (sizeof (guint32) * 2) + o_len + 1;

	cur_block_maybe_expand (jwriter, size);

	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), df);
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), s_id);
	cur_setstr (jwriter->cur_block, &(jwriter->cur_pos), uri, o_len);

	jwriter->cur_entry_amount++;
	jwriter->cur_block_len += size;

	return TRUE;
}

gboolean
tracker_db_journal_append_resource (gint         s_id,
                                    const gchar *uri)
{
	return db_journal_writer_append_resource (&writer, s_id, uri);
}

gboolean
tracker_db_journal_rollback_transaction (void)
{
	g_return_val_if_fail (writer.journal > 0, FALSE);

	cur_block_kill (&writer);

	return TRUE;
}

gboolean
tracker_db_journal_truncate (gsize new_size)
{
	g_return_val_if_fail (writer.journal > 0, FALSE);

	return (ftruncate (writer.journal, new_size) != -1);
}

static gboolean
db_journal_writer_commit_db_transaction (JournalWriter *jwriter)
{
	guint32 crc;
	guint begin_pos;
	guint size;
	guint offset;

	g_return_val_if_fail (jwriter->journal > 0, FALSE);

	begin_pos = 0;
	size = sizeof (guint32);
	offset = sizeof (guint32) * 3;

	/* Expand by uint32 for the size check at the end of the entry */
	cur_block_maybe_expand (jwriter, size);

	jwriter->cur_block_len += size;

	/* Write size and amount */
	cur_setnum (jwriter->cur_block, &begin_pos, jwriter->cur_block_len);
	cur_setnum (jwriter->cur_block, &begin_pos, jwriter->cur_entry_amount);

	/* Write size check to end of current journal data */
	cur_setnum (jwriter->cur_block, &(jwriter->cur_pos), jwriter->cur_block_len);

	/* Calculate CRC from entry triples start (i.e. without size,
	 * amount and crc) until the end of the entry block.
	 *
	 * NOTE: the size check at the end is included in the CRC!
	 */
	crc = tracker_crc32 (jwriter->cur_block + offset, jwriter->cur_block_len - offset);
	cur_setnum (jwriter->cur_block, &begin_pos, crc);

	if (!write_all_data (jwriter->journal, jwriter->cur_block, jwriter->cur_block_len)) {
		g_critical ("Could not write to journal, %s", g_strerror (errno));
		return FALSE;
	}

	/* Update journal size */
	jwriter->cur_size += jwriter->cur_block_len;

	/* Clean up for next transaction */
	cur_block_kill (jwriter);

	return TRUE;
}

gboolean
tracker_db_journal_commit_db_transaction (void)
{
	gboolean ret;

	ret = db_journal_writer_commit_db_transaction (&writer);

	if (ret) {
		if (rotating_settings.do_rotating && (writer.cur_size > rotating_settings.chunk_size)) {
			if (!tracker_db_journal_rotate ()) {
				g_critical ("Could not rotate journal, %s", g_strerror (errno));
				ret = FALSE;
			}
		}
	}

	return ret;
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


static gchar*
reader_get_next_filepath (JournalReader *jreader)
{
	gchar *filename_open = NULL;
	gchar *test;

	test = g_strdup_printf ("%s.%d", jreader->filename, jreader->current_file + 1);

	if (g_file_test (test, G_FILE_TEST_EXISTS)) {
		jreader->current_file++;
		filename_open = test;
	} else if (rotating_settings.rotate_to != NULL) {
		gchar *filename;
		GFile *dest_dir, *possible;

		/* This is where chunks are being rotated to */
		dest_dir = g_file_new_for_path (rotating_settings.rotate_to);

		filename = g_path_get_basename (test);
		g_free (test);
		possible = g_file_get_child (dest_dir, filename);
		g_object_unref (dest_dir);
		g_free (filename);

		if (g_file_query_exists (possible, NULL)) {
			jreader->current_file++;
			filename_open = g_file_get_path (possible);
		}
		g_object_unref (possible);
	}

	if (filename_open == NULL) {
		filename_open = g_strdup (jreader->filename);
		/* Last file is the active journal file */
		jreader->current_file = 0;
	}

	return filename_open;
}

static gboolean
db_journal_reader_init (JournalReader *jreader,
                        gboolean global_reader,
                        const gchar *filename)
{
	GError *error = NULL;
	gchar *filename_used;
	gchar *filename_open;

	g_return_val_if_fail (jreader->file == NULL, FALSE);

	/* Used mostly for testing */
	if (G_UNLIKELY (filename)) {
		filename_used = g_strdup (filename);
	} else {
		filename_used = g_build_filename (g_get_user_data_dir (),
		                                  "tracker",
		                                  "data",
		                                  TRACKER_DB_JOURNAL_FILENAME,
		                                  NULL);
	}

	jreader->filename = filename_used;

	reader.current_file = 0;
	if (global_reader) {
		filename_open = reader_get_next_filepath (jreader);
	} else {
		filename_open = g_strdup (filename_used);
	}

	jreader->type = TRACKER_DB_JOURNAL_START;
	jreader->file = g_mapped_file_new (filename_open, FALSE, &error);

	g_free (filename_open);

	if (error) {
		if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
			/* do not warn if the file does not exist, just return FALSE */
			g_warning ("Could not create TrackerDBJournalReader for file '%s', %s",
			           jreader->filename,
			           error->message ? error->message : "no error given");
		}
		g_error_free (error);
		g_free (jreader->filename);
		jreader->filename = NULL;

		return FALSE;
	}

	jreader->last_success = jreader->start = jreader->current = 
		g_mapped_file_get_contents (jreader->file);

	jreader->end = jreader->current + g_mapped_file_get_length (jreader->file);

	/* verify journal file header */
	if (jreader->end - jreader->current < 8) {
		tracker_db_journal_reader_shutdown ();
		return FALSE;
	}

	if (memcmp (jreader->current, "trlog\00003", 8)) {
		tracker_db_journal_reader_shutdown ();
		return FALSE;
	}

	jreader->current += 8;

	return TRUE;
}

gboolean
tracker_db_journal_reader_init (const gchar *filename)
{
	return db_journal_reader_init (&reader, TRUE, filename);
}

gsize
tracker_db_journal_reader_get_size_of_correct (void)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);

	return (gsize) (reader.last_success - reader.start);
}

static gboolean
reader_next_file (GError **error)
{
	gchar *filename_open;
	GError *new_error = NULL;

	filename_open = reader_get_next_filepath (&reader);

#if GLIB_CHECK_VERSION(2,22,0)
	g_mapped_file_unref (reader.file);
#else
	g_mapped_file_free (reader.file);
#endif

	reader.file = g_mapped_file_new (filename_open, FALSE, &new_error);

	if (new_error) {
		g_propagate_error (error, new_error);
		return FALSE;
	}

	reader.last_success = reader.start = reader.current = 
		g_mapped_file_get_contents (reader.file);

	reader.end = reader.current + g_mapped_file_get_length (reader.file);

	/* verify journal file header */
	if (reader.end - reader.current < 8) {
		g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
		             "Damaged journal entry at begin of journal");
		tracker_db_journal_reader_shutdown ();
		return FALSE;
	}

	if (memcmp (reader.current, "trlog\00003", 8)) {
		g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
		             "Damaged journal entry at begin of journal");
		tracker_db_journal_reader_shutdown ();
		return FALSE;
	}

	reader.current += 8;

	reader.type = TRACKER_DB_JOURNAL_END_TRANSACTION;

	reader.entry_begin = NULL;
	reader.entry_end = NULL;
	reader.amount_of_triples = 0;

	g_free (filename_open);

	return TRUE;
}

static gboolean
db_journal_reader_shutdown (JournalReader *jreader)
{
	g_return_val_if_fail (jreader->file != NULL, FALSE);

#if GLIB_CHECK_VERSION(2,22,0)
	g_mapped_file_unref (jreader->file);
#else
	g_mapped_file_free (jreader->file);
#endif

	jreader->file = NULL;

	g_free (jreader->filename);
	jreader->filename = NULL;

	jreader->last_success = NULL;
	jreader->start = NULL;
	jreader->current = NULL;
	jreader->end = NULL;
	jreader->entry_begin = NULL;
	jreader->entry_end = NULL;
	jreader->amount_of_triples = 0;
	jreader->type = TRACKER_DB_JOURNAL_START;
	jreader->uri = NULL;
	jreader->g_id = 0;
	jreader->s_id = 0;
	jreader->p_id = 0;
	jreader->o_id = 0;
	jreader->object = NULL;

	return TRUE;
}

gboolean
tracker_db_journal_reader_shutdown (void)
{
	return db_journal_reader_shutdown (&reader);
}

TrackerDBJournalEntryType
tracker_db_journal_reader_get_type (void)
{
	g_return_val_if_fail (reader.file != NULL, FALSE);

	return reader.type;
}

static gboolean
db_journal_reader_next (JournalReader *jreader, gboolean global_reader, GError **error)
{
	g_return_val_if_fail (jreader->file != NULL, FALSE);

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

	if (jreader->type == TRACKER_DB_JOURNAL_START ||
	    jreader->type == TRACKER_DB_JOURNAL_END_TRANSACTION) {
		/* Expect new transaction or end of file */
		guint32 entry_size;
		guint32 entry_size_check;
		guint32 crc;
		guint32 crc_check;
		TransactionFormat t_kind;

		/* Check the end is not before where we currently are */
		if (jreader->current >= jreader->end) {
				/* Return FALSE as there is no further entry but
				 * do not set error as it's not an error case. */
				if (global_reader && jreader->current_file != 0)
					return reader_next_file (error);
				else
					return FALSE;
		}

		/* Check the end is not smaller than the first uint32
		 * for reading the entry size.
		 */
		if (jreader->end - jreader->current < sizeof (guint32)) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %d < sizeof(guint32) at start/end of journal",
			             (gint) (jreader->end - jreader->current));
			return FALSE;
		}

		/* Read the first uint32 which contains the size */
		entry_size = read_uint32 (jreader->current);

		/* Check that entry is big enough for header and footer */
		if (entry_size < 5 * sizeof (guint32)) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0,
			             "Damaged journal entry, size %d < 5 * sizeof(guint32)",
			             (gint) entry_size);
			return FALSE;
		}

		/* Set the bounds for the entry */
		jreader->entry_begin = jreader->current;
		jreader->entry_end = jreader->entry_begin + entry_size;

		/* Check the end of the entry does not exceed the end
		 * of the journal.
		 */
		if (jreader->end < jreader->entry_end) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, end < entry end");
			return FALSE;
		}

		/* Move the current potision of the journal past the
		 * entry size we read earlier.
		 */
		jreader->current += 4;

		/* Read entry size check at the end of the entry */
		entry_size_check = read_uint32 (jreader->entry_end - 4);

		if (entry_size != entry_size_check) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %d != %d (entry size != entry size check)", 
			             entry_size, 
			             entry_size_check);
			return FALSE;
		}

		/* Read the amount of triples */
		jreader->amount_of_triples = read_uint32 (jreader->current);
		jreader->current += 4;

		/* Read the crc */
		crc_check = read_uint32 (jreader->current);
		jreader->current += 4;

		/* Calculate the crc */
		crc = tracker_crc32 (jreader->entry_begin + (sizeof (guint32) * 3), entry_size - (sizeof (guint32) * 3));

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
		jreader->time = read_uint32 (jreader->current);
		jreader->current += 4;

		t_kind = read_uint32 (jreader->current);
		jreader->current += 4;

		if (t_kind == TRANSACTION_FORMAT_DATA)
			jreader->type = TRACKER_DB_JOURNAL_START_TRANSACTION;
		else
			jreader->type = TRACKER_DB_JOURNAL_START_ONTOLOGY_TRANSACTION;

		return TRUE;
	} else if (jreader->amount_of_triples == 0) {
		/* end of transaction */

		jreader->current += 4;
		if (jreader->current != jreader->entry_end) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %p != %p (end of transaction with 0 triples)",
			             jreader->current,
			             jreader->entry_end);
			return FALSE;
		}

		jreader->type = TRACKER_DB_JOURNAL_END_TRANSACTION;
		jreader->last_success = jreader->current;

		return TRUE;
	} else {
		DataFormat df;
		gsize str_length;

		if (jreader->end - jreader->current < sizeof (guint32)) {
			/* damaged journal entry */
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry, %d < sizeof(guint32)",
			             (gint) (jreader->end - jreader->current));
			return FALSE;
		}

		df = read_uint32 (jreader->current);
		jreader->current += 4;

		if (df == DATA_FORMAT_RESOURCE_INSERT) {
			jreader->type = TRACKER_DB_JOURNAL_RESOURCE;

			if (jreader->end - jreader->current < sizeof (guint32) + 1) {
				/* damaged journal entry */
				g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
				             "Damaged journal entry, %d < sizeof(guint32) + 1 for resource",
				             (gint) (jreader->end - jreader->current));
				return FALSE;
			}

			jreader->s_id = read_uint32 (jreader->current);
			jreader->current += 4;

			str_length = strnlen (jreader->current, jreader->end - jreader->current);
			if (str_length == jreader->end - jreader->current) {
				/* damaged journal entry (no terminating '\0' character) */
				g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
				             "Damaged journal entry, no terminating zero found for resource");
				return FALSE;

			}

			if (!g_utf8_validate (jreader->current, -1, NULL)) {
				/* damaged journal entry (invalid UTF-8) */
				g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
				             "Damaged journal entry, invalid UTF-8 for resource");
				return FALSE;
			}

			jreader->uri = jreader->current;
			jreader->current += str_length + 1;
		} else {
			if (df & DATA_FORMAT_OPERATION_DELETE) {
				if (df & DATA_FORMAT_OBJECT_ID) {
					jreader->type = TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID;
				} else {
					jreader->type = TRACKER_DB_JOURNAL_DELETE_STATEMENT;
				}
			} else {
				if (df & DATA_FORMAT_OBJECT_ID) {
					jreader->type = TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID;
				} else {
					jreader->type = TRACKER_DB_JOURNAL_INSERT_STATEMENT;
				}
			}

			if (df & DATA_FORMAT_GRAPH) {
				if (jreader->end - jreader->current < sizeof (guint32)) {
					/* damaged journal entry */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
						     "Damaged journal entry, %d < sizeof(guint32)",
						     (gint) (jreader->end - jreader->current));
					return FALSE;
				}

				/* named graph */
				jreader->g_id = read_uint32 (jreader->current);
				jreader->current += 4;
			} else {
				/* default graph */
				jreader->g_id = 0;
			}

			if (jreader->end - jreader->current < 2 * sizeof (guint32)) {
				/* damaged journal entry */
				g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
				             "Damaged journal entry, %d < 2 * sizeof(guint32)",
				             (gint) (jreader->end - jreader->current));
				return FALSE;
			}

			jreader->s_id = read_uint32 (jreader->current);
			jreader->current += 4;

			jreader->p_id = read_uint32 (jreader->current);
			jreader->current += 4;

			if (df & DATA_FORMAT_OBJECT_ID) {
				if (jreader->end - jreader->current < sizeof (guint32)) {
					/* damaged journal entry */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
					             "Damaged journal entry, %d < sizeof(guint32) for data format 2",
					             (gint) (jreader->end - jreader->current));
					return FALSE;
				}

				jreader->o_id = read_uint32 (jreader->current);
				jreader->current += 4;
			} else {
				if (jreader->end - jreader->current < 1) {
					/* damaged journal entry */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
					             "Damaged journal entry, %d < 1",
					             (gint) (jreader->end - jreader->current));
					return FALSE;
				}

				str_length = strnlen (jreader->current, jreader->end - jreader->current);
				if (str_length == jreader->end - jreader->current) {
					/* damaged journal entry (no terminating '\0' character) */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
					             "Damaged journal entry, no terminating zero found");
					return FALSE;
				}

				if (!g_utf8_validate (jreader->current, -1, NULL)) {
					/* damaged journal entry (invalid UTF-8) */
					g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
					             "Damaged journal entry, invalid UTF-8");
					return FALSE;
				}

				jreader->object = jreader->current;
				jreader->current += str_length + 1;
			}
		}

		jreader->amount_of_triples--;
		return TRUE;
	}

	g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, "Unknown reason");

	return FALSE;
}

gboolean
tracker_db_journal_reader_next (GError **error)
{
	return db_journal_reader_next (&reader, TRUE, error);
}

gboolean
tracker_db_journal_reader_verify_last (const gchar  *filename,
                                       GError      **error)
{
	guint32 entry_size_check;
	gboolean success = FALSE;
	JournalReader jreader = { 0 };

	if (db_journal_reader_init (&jreader, FALSE, filename)) {
		entry_size_check = read_uint32 (jreader.end - 4);

		if (jreader.end - entry_size_check < jreader.current) {
			g_set_error (error, TRACKER_DB_JOURNAL_ERROR, 0, 
			             "Damaged journal entry at end of journal");
			db_journal_reader_shutdown (&jreader);
			return FALSE;
		}

		jreader.current = jreader.end - entry_size_check;
		success = db_journal_reader_next (&jreader, FALSE, NULL);
		db_journal_reader_shutdown (&jreader);
	}

	return success;
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
	/* TODO: Fix this now that multiple chunks can exist */
	return (((gdouble)(reader.current - reader.start)) / percent);
}

static void
on_chunk_copied_delete (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
	GFile *source = G_FILE (source_object);
	GError *error = NULL;

	if (g_file_copy_finish (source, res, &error)) {
		g_file_delete (G_FILE (source_object), NULL, &error);
	}

	if (error) {
		g_critical ("Error moving rotated journal chunk: '%s'", error->message);
		g_error_free (error);
	}
}

static gboolean
tracker_db_journal_rotate (void)
{
	gchar *fullpath;
	static gint max = -1;
	static gboolean needs_move;

	if (max == -1) {
		gchar *directory;
		GDir *journal_dir;
		const gchar *f_name;

		directory = g_path_get_dirname (writer.journal_filename);
		needs_move = (g_strcmp0 (rotating_settings.rotate_to, directory) != 0);
		journal_dir = g_dir_open (directory, 0, NULL);

		f_name = g_dir_read_name (journal_dir);

		while (f_name) {
			gchar *ptr;
			guint cur;

			if (f_name) {

				if (!g_str_has_prefix (f_name, TRACKER_DB_JOURNAL_FILENAME ".")) {
					f_name = g_dir_read_name (journal_dir);
					continue;
				}

				ptr = strrchr (f_name, '.');
				if (ptr) {
					ptr++;
					cur = atoi (ptr);
					max = MAX (cur, max);
				}
			} 

			f_name = g_dir_read_name (journal_dir);
		}

		g_dir_close (journal_dir);
		g_free (directory);
	}

	tracker_db_journal_fsync ();

	if (close (writer.journal) != 0) {
		g_warning ("Could not close journal, %s", 
		           g_strerror (errno));
		return FALSE;
	}

	fullpath = g_strdup_printf ("%s.%d", writer.journal_filename, ++max);

	g_rename (writer.journal_filename, fullpath);

	g_free (fullpath);

	if (max > 1 && needs_move) {
		GFile *source, *destination;
		GFile *dest_dir;
		gchar *filename;

		fullpath = g_strdup_printf ("%s.%d", writer.journal_filename, max - 1);
		source = g_file_new_for_path (fullpath);
		dest_dir = g_file_new_for_path (rotating_settings.rotate_to);
		filename = g_path_get_basename (fullpath);
		destination = g_file_get_child (dest_dir, filename);
		g_object_unref (dest_dir);
		g_free (filename);

		g_file_copy_async (source, destination, G_FILE_COPY_OVERWRITE, 0,
		                   NULL, NULL, NULL, on_chunk_copied_delete, NULL);

		g_object_unref (destination);
		g_object_unref (source);
		g_free (fullpath);
	}

	return db_journal_init_file (&writer, TRUE);
}
