/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <glib.h>

#include <libtracker-data/tracker-data.h>

#include "tracker-db-manager-common.h"

void
test_assert_tables_in_db (TrackerDB db, gchar *query)
{
	g_assert (test_assert_query_run (db, query));
}

static void
test_creation_common_db ()
{
	test_assert_tables_in_db (TRACKER_DB_COMMON, "SELECT * FROM MetaDataTypes");
}

static void
test_creation_cache_db ()
{
	test_assert_tables_in_db (TRACKER_DB_CACHE, "SELECT * FROM SearchResults1");
}

static void
test_creation_file_meta_db ()
{
	test_assert_tables_in_db (TRACKER_DB_FILE_METADATA, "SELECT * FROM ServiceMetaData");
}

static void
test_creation_file_contents_db ()
{
	test_assert_tables_in_db (TRACKER_DB_FILE_CONTENTS, "SELECT * FROM ServiceContents");
}

static void
test_creation_email_meta_db ()
{
	test_assert_tables_in_db (TRACKER_DB_EMAIL_METADATA, "SELECT * FROM ServiceMetadata");
}

static void
test_creation_email_contents_db ()
{
	test_assert_tables_in_db (TRACKER_DB_FILE_CONTENTS, "SELECT * FROM ServiceContents");
}


int
main (int argc, char **argv) {

	int result;
	gint first_time;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	/* Init */
	tracker_db_manager_init (TRACKER_DB_MANAGER_FORCE_REINDEX,
	                         &first_time);

	g_test_add_func ("/libtracker-db/tracker-db-manager/unattach/common_db_tables",
	                 test_creation_common_db);

	g_test_add_func ("/libtracker-db/tracker-db-manager/unattach/cache_db_tables",
	                 test_creation_cache_db);

	g_test_add_func ("/libtracker-db/tracker-db-manager/unattach/file_meta_db_tables",
	                 test_creation_file_meta_db);

	g_test_add_func ("/libtracker-db/tracker-db-manager/unattach/file_contents_db_tables",
	                 test_creation_file_contents_db);

	g_test_add_func ("/libtracker-db/tracker-db-manager/unattach/email_meta_db_tables",
	                 test_creation_email_meta_db);

	g_test_add_func ("/libtracker-db/tracker-db-manager/unattach/email_contents_db_tables",
	                 test_creation_email_contents_db);

	result = g_test_run ();

	/* End */
	tracker_db_manager_shutdown ();

	return result;
}
