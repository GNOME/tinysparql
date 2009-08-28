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
#include "config.h"

#define _GNU_SOURCE

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tracker-db-journal.h"

static gchar *filename = NULL;
static FILE *journal = NULL;
static GMappedFile *mapped = NULL;
static gsize current_size = 0;

#define TRACKER_DB_JOURNAL_LOG_FILENAME		"log.sparql.txt"

static void
get_filename (void)
{
	if (!filename) {
		filename = g_build_filename (g_get_user_data_dir (),
		                             "tracker",
		                             "data",
		                             TRACKER_DB_JOURNAL_LOG_FILENAME,
		                             NULL);
	}
}

gsize
tracker_db_journal_get_size (void)
{
	return current_size;
}

const gchar*
tracker_db_journal_filename (void)
{
	get_filename ();
	return (const gchar *) filename;
}

void
tracker_db_journal_open (void)
{
	struct stat st;

	get_filename ();

	journal = fopen (filename, "a");

	if (stat (filename, &st) == 0) {
		current_size = (gsize) st.st_size;
	}
}

void
tracker_db_journal_log (const gchar *query)
{
	if (journal) {
		size_t len = strlen (query);
		write (fileno (journal), query, len);
		write (fileno (journal), "\n\0", 2);
		current_size += (len + 2);
	}
}

void 
tracker_db_journal_fsync (void)
{
	if (journal) {
		fsync (fileno (journal));
	}
}

void
tracker_db_journal_truncate (void)
{
	if (journal) {
		ftruncate(fileno (journal), 0);
		current_size = 0;
		fsync (fileno (journal));
	}
}

void
tracker_db_journal_close (void)
{
	if (journal) {
		fclose (journal);
		journal = NULL;
	}

	g_free (filename);
	filename = NULL;
}

TrackerJournalContents*
tracker_db_journal_get_contents (guint transaction_size)
{
	GPtrArray *lines;
	gsize max_pos, next_len;
	gchar *cur;

	get_filename ();

	if (!mapped) {
		GError *error = NULL;

		mapped = g_mapped_file_new (filename, FALSE, &error);

		if (error) {
			g_clear_error (&error);
			mapped = NULL;
			return NULL;
		}
	}

	lines = g_ptr_array_sized_new (transaction_size > 0 ? transaction_size : 2000);

	cur = g_mapped_file_get_contents (mapped);
	max_pos = (gsize) (cur + g_mapped_file_get_length (mapped));

	while (((gsize)cur) < max_pos) {
		next_len = strnlen (cur, max_pos - ((gsize)cur)) + 1;
		g_ptr_array_add (lines, cur);
		cur += next_len;
	}

	return (TrackerJournalContents *) lines;
}

void 
tracker_db_journal_free_contents (TrackerJournalContents *contents)
{
	if (mapped) {
		g_mapped_file_free (mapped);
		mapped = NULL;
	}

	g_ptr_array_free ((GPtrArray *)contents, TRUE);
}
