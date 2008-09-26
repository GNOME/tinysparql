/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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
#include <glib.h>
#include <glib/gtestutils.h>


#include <libtracker-db/tracker-db-manager.h>
#include "tracker-db-manager-common.h"


static void
test_union_performance_xesam_view ()
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	GError		   *error = NULL;

	iface = tracker_db_manager_get_db_interfaces (7, TRACKER_DB_COMMON,
							  TRACKER_DB_XESAM,
							  TRACKER_DB_FILE_METADATA,
							  TRACKER_DB_FILE_CONTENTS,
							  TRACKER_DB_EMAIL_CONTENTS,
							  TRACKER_DB_EMAIL_METADATA,
							  TRACKER_DB_CACHE);

	g_assert (iface);

	result_set = tracker_db_interface_execute_query (iface, &error,
		"CREATE TEMPORARY VIEW XesamServicesView AS SELECT * FROM 'file-meta'.Services UNION SELECT * FROM 'email-meta'.Services");
	if (result_set)
		g_object_unref (result_set);

	g_assert (!error);

	result_set = tracker_db_interface_execute_query (iface, &error,
		"CREATE TEMPORARY VIEW XesamServiceMetaDataView AS SELECT * FROM 'file-meta'.ServiceMetaData UNION SELECT * FROM 'email-meta'.ServiceMetaData");
	if (result_set)
		g_object_unref (result_set);

	g_assert (!error);

	/* TODO: Start timer  */
	result_set = tracker_db_interface_execute_query (iface, &error,
		"SELECT M0.MetaDataValue "
		"FROM XesamServicesView S "
		"INNER JOIN XesamServiceMetaDataView M0 ON (S.ID = M0.ServiceID and "
		"M0.MetaDataID in (82)) WHERE (S.ServiceTypeID in (select TypeId from "
		"ServiceTypes where TypeName = 'Files' or Parent = 'Files')) AND "
		" (  (M0.MetaDataValue like '%%test%%')  )");

	/* TODO:  Stop timer  */
	if (result_set)
		g_object_unref (result_set);

	g_assert (!error);

	g_object_unref (iface);
}


static void
test_union_performance_xesam_union ()
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	GError		   *error = NULL;

	iface = tracker_db_manager_get_db_interfaces (7, TRACKER_DB_COMMON,
							  TRACKER_DB_XESAM,
							  TRACKER_DB_FILE_METADATA,
							  TRACKER_DB_FILE_CONTENTS,
							  TRACKER_DB_EMAIL_CONTENTS,
							  TRACKER_DB_EMAIL_METADATA,
							  TRACKER_DB_CACHE);

	g_assert (iface);

	g_assert (!error);

	/* TODO: Start timer  */
	result_set = tracker_db_interface_execute_query (iface, &error,
		"SELECT M0.MetaDataValue "
		"FROM 'file-meta'.Services S "
		"INNER JOIN 'file-meta'.ServiceMetaData M0 ON (S.ID = M0.ServiceID and "
		"M0.MetaDataID in (82)) WHERE (S.ServiceTypeID in (select TypeId from "
		"ServiceTypes where TypeName = 'Files' or Parent = 'Files')) AND "
		" (  (M0.MetaDataValue like '%%test%%')  ) "

		"UNION "

		"SELECT M0.MetaDataValue "
		"FROM 'email-meta'.Services S "
		"INNER JOIN 'email-meta'.ServiceMetaData M0 ON (S.ID = M0.ServiceID and "
		"M0.MetaDataID in (82)) WHERE (S.ServiceTypeID in (select TypeId from "
		"ServiceTypes where TypeName = 'Email' or Parent = 'Email')) AND "
		" (  (M0.MetaDataValue like '%%test%%')  ) ");

	/* TODO:  Stop timer  */
	if (result_set)
		g_object_unref (result_set);

	g_assert (!error);

	g_object_unref (iface);
}


int
main (int argc, char **argv)
{
	int result;
	gint first_time;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	/* Init */
	tracker_db_manager_init (TRACKER_DB_MANAGER_FORCE_REINDEX,
							 &first_time);

	g_test_add_func ("/libtracker-db/union-performance/xesam/view",
					 test_union_performance_xesam_view);

	g_test_add_func ("/libtracker-db/union-performance/xesam/union",
					 test_union_performance_xesam_union);

	result = g_test_run ();

	/* End */
	tracker_db_manager_shutdown ();

	return result;
}
