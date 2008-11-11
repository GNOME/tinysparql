/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
 * Copyright (C) 2008, Nokia
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

#include <string.h>
#include <stdlib.h>

#include "tracker-data-live-search.h"
#include "tracker-data-manager.h"
#include "tracker-data-schema.h"


static gboolean
db_exec_proc_no_reply (TrackerDBInterface *iface,
		       const gchar	  *procedure,
		       ...)
{
	TrackerDBResultSet *result_set;
	va_list args;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), FALSE);
	g_return_val_if_fail (procedure != NULL, FALSE);

	va_start (args, procedure);
	result_set = tracker_db_interface_execute_vprocedure (iface,
							      NULL,
							      procedure,
							      args);
	va_end (args);

	if (result_set) {
		g_object_unref (result_set);
	}

	return TRUE;
}

TrackerDBResultSet *
tracker_data_live_search_get_hit_count (TrackerDBInterface *iface,
				        const gchar	   *search_id)
{
	/* SELECT count(*)
	 * FROM LiveSearches
	 * WHERE SearchID = ? */

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (search_id != NULL, NULL);

	return tracker_data_manager_exec_proc (iface,
				     "GetLiveSearchHitCount",
				     search_id,
				     NULL);
}

void
tracker_data_live_search_start (TrackerDBInterface *iface,
			        const gchar	   *from_query,
			        const gchar	   *join_query,
			        const gchar	   *where_query,
			        const gchar	   *search_id)
{
	/* INSERT
	 * INTO LiveSearches
	 * SELECT ID, SEARCH_ID FROM_QUERY WHERE_QUERY */

	g_return_if_fail (TRACKER_IS_DB_INTERFACE (iface));
	g_return_if_fail (from_query != NULL);
	g_return_if_fail (join_query != NULL);
	g_return_if_fail (where_query != NULL);
	g_return_if_fail (search_id != NULL);

	g_message ("INSERT INTO cache.LiveSearches SELECT S.ID, '%s' %s %s %s",
				  search_id,
				  from_query,
				  join_query,
				  where_query);

	tracker_data_manager_exec_no_reply (iface,
				  "INSERT INTO cache.LiveSearches SELECT S.ID, '%s' %s %s %s",
				  search_id,
				  from_query,
				  join_query,
				  where_query);
}

void
tracker_data_live_search_stop (TrackerDBInterface *iface,
			       const gchar	  *search_id)
{
	/* DELETE
	 * FROM LiveSearches as X
	 * WHERE E.SearchID = ? */

	g_return_if_fail (TRACKER_IS_DB_INTERFACE (iface));
	g_return_if_fail (search_id != NULL);

	db_exec_proc_no_reply (iface,
			       "LiveSearchStopSearch",
			       search_id,
			       NULL);
}

TrackerDBResultSet *
tracker_data_live_search_get_ids (TrackerDBInterface *iface,
				  const gchar        *search_id)
{
	/* Contract, in @result:
	 * ServiceID is #1 */

	/*
	 * SELECT X.ServiceID
	 * FROM LiveSearches as X
	 * WHERE X.SearchID = SEARCH_ID
	 */

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (search_id != NULL, NULL);

	return tracker_data_manager_exec_proc (iface,
				     "GetLiveSearchAllIDs",
				     search_id,
				     NULL);
}

TrackerDBResultSet *
tracker_data_live_search_get_new_ids (TrackerDBInterface *iface,
				      const gchar        *search_id,
				      const gchar        *from_query,
				      const gchar        *query_joins,
				      const gchar        *where_query)
{
	TrackerDBResultSet *result_set;

	/* Contract, in @result:
	 * ServiceID is #1
	 * EventType is #2 */

	/*
	 * SELECT E.ServiceID, E.EventType
	 * FROM_QUERY, LiveSearches as X, Events as E
	 * QUERY_JOINS
	 * WHERE_QUERY
	 * AND X.ServiceID = E.ServiceID
	 * AND X.SearchID = 'SEARCH_ID'
	 * AND E.EventType = 'Update'
	 * UNION
	 * SELECT E.ServiceID, E.EventType
	 * FROM_QUERY, Events as E
	 * QUERY_JOINS
	 * WHERE_QUERY
	 * AND E.ServiceID = S.ID
	 * AND E.EventType = 'Create'
	 */

	/*
	 * INSERT INTO LiveSearches
	 * SELECT E.ServiceID, 'SEARCH_ID'
	 * FROM_QUERY, Events as E
	 * QUERY_JOINS
	 * WHERE_QUERY
	 * AND E.ServiceID = S.ID
	 * AND E.EventType = 'Create'
	 */

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (search_id != NULL, NULL);
	g_return_val_if_fail (from_query != NULL, NULL);
	g_return_val_if_fail (query_joins != NULL, NULL);
	g_return_val_if_fail (where_query != NULL, NULL);

	// We need to add 'file-meta' and 'email-meta' here

	result_set = tracker_data_manager_exec (iface,
				      "SELECT E.ServiceID, E.EventType "
				      "%s%s cache.LiveSearches as X, Events as E " /* FROM   A1 */
				       "%s"				     /* JOINS  A2 */
				       "%s"				     /* WHERE  A3 */
				      "%sX.ServiceID = E.ServiceID "
				      "AND X.SearchID = '%s' "		     /*        A4 */
				      "AND E.EventType = 'Update' "
				      "UNION "
				      "SELECT E.ServiceID, E.EventType "
				      "%s%s Events as E "		     /* FROM   B1 */
				      "%s"				     /* JOINS  B2 */
				      "%s"				     /* WHERE  B3 */
				      "%sE.ServiceID = S.ID "
				      "AND E.EventType = 'Create' ",
				      from_query ? from_query : "FROM",      /*        A1 */
				      from_query ? "," : "",		     /*        A1 */
				      query_joins,			     /*        A2 */
				      where_query ? where_query : "WHERE",   /*        A3 */
				      where_query ? "AND " : "",	     /*        A3 */
				      search_id,			     /*        A4 */
				      from_query ? from_query : "FROM",      /*        B1 */
				      from_query ? "," : "",		     /*        B1 */
				      query_joins,			     /*        B2 */
				      where_query ? where_query : "WHERE",   /*        B3 */
				      where_query ? "AND " : "");	     /*        B3 */

	tracker_data_manager_exec_no_reply (iface,
				  "INSERT INTO cache.LiveSearches "
				   "SELECT E.ServiceID, '%s' "		     /*        B0 */
				  "%s%s Events as E "			     /* FROM   B1 */
				  "%s"					     /* JOINS  B2 */
				   "%s"					     /* WHERE  B3 */
				  "%sE.ServiceID = S.ID"
				  "AND E.EventType = 'Create' ",
				  search_id,				     /*        B0 */
				  from_query ? from_query : "FROM",	     /*        B1 */
				  from_query ? "," : "",		     /*        B1 */
				  query_joins,				     /*        B2 */
				  where_query ? where_query : "WHERE",	     /*        B3 */
				  where_query ? "AND " : "");		     /*        B3 */

	return result_set;
}

TrackerDBResultSet*
tracker_data_live_search_get_deleted_ids (TrackerDBInterface *iface,
					  const gchar	     *search_id)
{
	/* SELECT E.ServiceID
	 * FROM Events as E, LiveSearches as X
	 * WHERE E.ServiceID = X.ServiceID
	 * AND X.SearchID = ?
	 * AND E.EventType IS 'Delete' */

	/* DELETE FROM LiveSearches AS Y WHERE Y.ServiceID IN
	 * SELECT ServiceID FROM Events as E, LiveSearches as X
	 * WHERE E.ServiceID = X.ServiceID
	 * AND X.SearchID = ?
	 * AND E.EventType IS 'Delete' */

	TrackerDBResultSet *result_set;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (search_id != NULL, NULL);

	result_set = tracker_data_manager_exec_proc (iface,
					   "GetLiveSearchDeletedIDs",
					   search_id,
					   NULL);

	db_exec_proc_no_reply (iface,
			       "DeleteLiveSearchDeletedIDs",
			       search_id,
			       NULL);
	return result_set;
}

/* FIXME This function should be moved with other help-functions somewhere.
 * It is used by xesam_live_search parsing. */

static GList *
add_live_search_metadata_field (TrackerDBInterface *iface,
				GSList **fields,
				const char *xesam_name)
{
	TrackerDBResultSet *result_set;
	TrackerFieldData   *field_data;
	gboolean	    field_exists;
	const GSList	   *l;
	GList		   *reply;
	gboolean	    valid;

	reply = NULL;
	field_exists = FALSE;
	field_data = NULL;
	valid = TRUE;

	/* Do the xesam mapping */

	g_debug ("add metadata field");

	result_set = tracker_data_manager_exec_proc (iface, "GetXesamMetaDataMappings",xesam_name, NULL);

	if (!result_set) {
		return NULL;
	}

	while (valid) {
		gchar *field_name;

		tracker_db_result_set_get (result_set, 0, &field_name, -1);

		/* Check if field is already in list */
		for (l = *fields; l; l = l->next) {
			const gchar *this_field_name;

			this_field_name = tracker_field_data_get_field_name (l->data);

			if (!this_field_name) {
				continue;
			}

			if (strcasecmp (this_field_name, field_name) != 0) {
				continue;
			}

			field_exists = TRUE;

			break;
		}

		if (!field_exists) {
			field_data = tracker_data_schema_get_metadata_field (iface,
								    "Files",
								    field_name,
								    g_slist_length (*fields),
								    FALSE,
								    FALSE);

			if (field_data) {
				*fields = g_slist_prepend (*fields, field_data);
			}
		}

		reply = g_list_append (reply, field_data);
		valid = tracker_db_result_set_iter_next (result_set);
		g_free (field_name);
	}

	return reply;
}



TrackerDBResultSet *
tracker_data_live_search_get_hit_data (TrackerDBInterface *iface,
				       const gchar	  *search_id,
				       GStrv		   field_names)
{
	TrackerDBResultSet *result;
	GSList		   *fields = NULL;
	GSList		   *l = NULL;
	GString		   *sql_select;
	GString		   *sql_join;
	gint		    i = 0;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (search_id != NULL, NULL);

	sql_select = g_string_new ("X.ServiceID, ");
	sql_join = g_string_new ("");

	while (field_names[i]) {
		GList *field_data_list = NULL;

		field_data_list = add_live_search_metadata_field (iface,
								  &fields,
								  field_names[i]);

		if (!field_data_list) {
			g_warning ("Asking for a non-mapped xesam field: %s", field_names[i]);
			g_string_free (sql_select, TRUE);
			g_string_free (sql_join, TRUE);
			return NULL;
		}

		if (i) {
			g_string_append_printf (sql_select, ",");
		}

		g_string_append_printf (sql_select, " %s",
					tracker_field_data_get_select_field (field_data_list->data) );

		i++;
	}

	for (l = fields; l; l = l->next) {
		gchar *field_name;

		field_name = tracker_data_schema_metadata_field_get_related_names (iface,
								    tracker_field_data_get_field_name (l->data));
		g_string_append_printf (sql_join,
					"INNER JOIN 'files-meta'.%s %s ON (X.ServiceID = %s.ServiceID AND %s.MetaDataID in (%s))\n ",
					tracker_field_data_get_table_name (l->data),
					tracker_field_data_get_alias (l->data),
					tracker_field_data_get_alias (l->data),
					tracker_field_data_get_alias (l->data),
					field_name);
		g_free (field_name);
	}

	g_debug("Query : SELECT %s FROM cache.LiveSearches as X \n"
		"%s"
		"WHERE X.SearchID = '%s'",
		sql_select->str, sql_join->str, search_id);

	result = tracker_data_manager_exec (iface,
				  "SELECT %s FROM cache.LiveSearches as X \n"
				  "%s"
				  "WHERE X.SearchID = '%s'",
				  sql_select->str, sql_join->str, search_id);

	g_string_free (sql_select, TRUE);
	g_string_free (sql_join, TRUE);

	return result;
}

