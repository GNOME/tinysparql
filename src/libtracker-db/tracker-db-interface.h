/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Nokia
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

#ifndef __TRACKER_DB_INTERFACE_H__
#define __TRACKER_DB_INTERFACE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_DB_INTERFACE	    (tracker_db_interface_get_type ())
#define TRACKER_DB_INTERFACE(obj)	    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_DB_INTERFACE, TrackerDBInterface))
#define TRACKER_IS_DB_INTERFACE(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_DB_INTERFACE))
#define TRACKER_DB_INTERFACE_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), TRACKER_TYPE_DB_INTERFACE, TrackerDBInterfaceIface))

#define TRACKER_TYPE_DB_RESULT_SET	    (tracker_db_result_set_get_type ())
#define TRACKER_DB_RESULT_SET(o)	    (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DB_RESULT_SET, TrackerDbResultSet))
#define TRACKER_DB_RESULT_SET_CLASS(c)	    (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_DB_RESULT_SET, TrackerDbResultSetClass))
#define TRACKER_IS_DB_RESULT_SET(o)	    (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DB_RESULT_SET))
#define TRACKER_IS_DB_RESULT_SET_CLASS(c)   (G_TYPE_CHECK_CLASS_TYPE ((o),    TRACKER_TYPE_DB_RESULT_SET))
#define TRACKER_DB_RESULT_SET_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_DB_RESULT_SET, TrackerDbResultSetClass))

#define TRACKER_TYPE_DB_BLOB		    (tracker_db_blob_get_type ())

#define TRACKER_DB_INTERFACE_ERROR	    (tracker_db_interface_error_quark ())

typedef enum {
	TRACKER_DB_QUERY_ERROR,
	TRACKER_DB_CORRUPT
} TrackerDBInterfaceError;

typedef struct TrackerDBInterface TrackerDBInterface;
typedef struct TrackerDBInterfaceIface TrackerDBInterfaceIface;
typedef struct TrackerDBResultSet TrackerDBResultSet;
typedef struct TrackerDBResultSetClass TrackerDBResultSetClass;

struct TrackerDBInterfaceIface {
	GTypeInterface iface;

	void		     (* set_procedure_table)   (TrackerDBInterface  *interface,
							GHashTable	    *procedure_table);
	TrackerDBResultSet * (* execute_procedure)     (TrackerDBInterface  *interface,
							GError		   **error,
							const gchar	    *procedure,
							va_list		     args);
	TrackerDBResultSet * (* execute_procedure_len) (TrackerDBInterface  *interface,
							GError		   **error,
							const gchar	    *procedure,
							va_list		     args);
	TrackerDBResultSet * (* execute_query)	       (TrackerDBInterface  *interface,
							GError		   **error,
							const gchar	    *query);

};

struct TrackerDBResultSet {
	GObject parent_class;
};

struct TrackerDBResultSetClass {
	GObjectClass parent_class;
};


GQuark tracker_db_interface_error_quark (void);

GType tracker_db_interface_get_type (void);
GType tracker_db_result_set_get_type (void);
GType tracker_db_blob_get_type (void);


/* Functions to create queries/procedures */
TrackerDBResultSet *	tracker_db_interface_execute_vquery	 (TrackerDBInterface   *interface,
								  GError	     **error,
								  const gchar	       *query,
								  va_list		args);
TrackerDBResultSet *	tracker_db_interface_execute_query	 (TrackerDBInterface   *interface,
								  GError	     **error,
								  const gchar	       *query,
								  ...) G_GNUC_PRINTF (3, 4);
void			tracker_db_interface_set_procedure_table (TrackerDBInterface   *interface,
								  GHashTable	       *procedure_table);
TrackerDBResultSet *	tracker_db_interface_execute_vprocedure  (TrackerDBInterface   *interface,
								  GError	     **error,
								  const gchar	       *procedure,
								  va_list		args);
TrackerDBResultSet *	tracker_db_interface_execute_procedure	 (TrackerDBInterface   *interface,
								  GError	     **error,
								  const gchar	       *procedure,
								  ...) G_GNUC_NULL_TERMINATED;

TrackerDBResultSet *	tracker_db_interface_execute_vprocedure_len (TrackerDBInterface   *interface,
								     GError		**error,
								     const gchar	  *procedure,
								     va_list		   args);
TrackerDBResultSet *	tracker_db_interface_execute_procedure_len  (TrackerDBInterface   *interface,
								     GError		**error,
								     const gchar	  *procedure,
								     ...) G_GNUC_NULL_TERMINATED;

gboolean		tracker_db_interface_start_transaction	    (TrackerDBInterface   *interface);
gboolean		tracker_db_interface_end_transaction	    (TrackerDBInterface   *interface);


/* Semi private TrackerDBResultSet functions */
TrackerDBResultSet *	  _tracker_db_result_set_new	       (guint		    cols);
void			  _tracker_db_result_set_append        (TrackerDBResultSet *result_set);
void			  _tracker_db_result_set_set_value     (TrackerDBResultSet *result_set,
								guint		    column,
								const GValue	   *value);
void			  _tracker_db_result_set_get_value     (TrackerDBResultSet *result_set,
								guint		    column,
								GValue		   *value);

/* Functions to deal with the resultset */
void			  tracker_db_result_set_get	       (TrackerDBResultSet *result_set,
								...);
void			  tracker_db_result_set_rewind	       (TrackerDBResultSet *result_set);
gboolean		  tracker_db_result_set_iter_next      (TrackerDBResultSet *result_set);
guint			  tracker_db_result_set_get_n_columns  (TrackerDBResultSet *result_set);
guint			  tracker_db_result_set_get_n_rows     (TrackerDBResultSet *result_set);


G_END_DECLS

#endif /* __TRACKER_DB_INTERFACE_H__ */
