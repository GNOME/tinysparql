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

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <zlib.h>
#include <locale.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-field.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-nfs-lock.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-db-manager.h"
#include "tracker-db-interface-sqlite.h"

/* ZLib buffer settings */
#define ZLIB_BUF_SIZE		      8192

/* Default memory settings for databases */
#define TRACKER_DB_PAGE_SIZE_DONT_SET -1

/* Size is in bytes and is currently 2Gb */
#define TRACKER_DB_MAX_FILE_SIZE      2000000000 

/* Set current database version we are working with */
#define TRACKER_DB_VERSION_NOW        TRACKER_DB_VERSION_4
#define TRACKER_DB_VERSION_FILE       "db-version.txt"

typedef enum {
	TRACKER_DB_LOCATION_DATA_DIR,
	TRACKER_DB_LOCATION_USER_DATA_DIR,
	TRACKER_DB_LOCATION_SYS_TMP_DIR,
} TrackerDBLocation;

typedef enum {
	TRACKER_DB_VERSION_UNKNOWN, /* Unknown */
	TRACKER_DB_VERSION_1,       /* Version 0.6.6  (before indexer-split) */
	TRACKER_DB_VERSION_2,       /* Version 0.6.90 (after  indexer-split) */
	TRACKER_DB_VERSION_3,       /* Version 0.6.91 (stable release) */
	TRACKER_DB_VERSION_4,       /* Version 0.6.92 (current TRUNK) */
	TRACKER_DB_VERSION_5        /* Version 0.7    (vstore branch) */
} TrackerDBVersion;

typedef struct {
	TrackerDB	    db;
	TrackerDBLocation   location;
	TrackerDBInterface *iface;
	const gchar	   *file;
	const gchar	   *name;
	gchar		   *abs_filename;
	gint		    cache_size;
	gint		    page_size;
	gboolean	    add_functions;
	gboolean	    attached;
	gboolean	    is_index;
	guint64             mtime;
} TrackerDBDefinition;

typedef struct {
	GString *string;     /* The string we are accumulating */
} AggregateData;

static TrackerDBDefinition dbs[] = {
	{ TRACKER_DB_UNKNOWN,
	  TRACKER_DB_LOCATION_USER_DATA_DIR,
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  32,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  FALSE,
	  FALSE,
 	  0 },
	{ TRACKER_DB_COMMON,
	  TRACKER_DB_LOCATION_USER_DATA_DIR,
	  NULL,
	  "common.db",
	  "common",
	  NULL,
	  32,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  FALSE,
	  FALSE,
 	  0 },
	{ TRACKER_DB_CACHE,
	  TRACKER_DB_LOCATION_SYS_TMP_DIR,
	  NULL,
	  "cache.db",
	  "cache",
	  NULL,
	  128,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  FALSE,
	  FALSE,
 	  0 },
	{ TRACKER_DB_FILE_METADATA,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "file-meta.db",
	  "file-meta",
	  NULL,
	  512,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  TRUE,
	  FALSE,
	  FALSE,
 	  0 },
	{ TRACKER_DB_FILE_FULLTEXT,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "file-fulltext.db",
	  "file-fulltext",
	  NULL,
	  512,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  TRUE,
	  FALSE,
	  TRUE,
 	  0 },
	{ TRACKER_DB_FILE_CONTENTS,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "file-contents.db",
	  "file-contents",
	  NULL,
	  1024,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  FALSE,
	  FALSE,
 	  0 },
	{ TRACKER_DB_EMAIL_METADATA,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "email-meta.db",
	  "email-meta",
	  NULL,
	  512,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  TRUE,
	  FALSE,
 	  0 },
	{ TRACKER_DB_EMAIL_FULLTEXT,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "email-fulltext.db",
	  "email-fulltext",
	  NULL,
	  512,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  TRUE,
	  FALSE,
	  TRUE,
 	  0 },
	{ TRACKER_DB_EMAIL_CONTENTS,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "email-contents.db",
	  "email-contents",
	  NULL,
	  512,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  FALSE,
	  FALSE,
 	  0 },
};

static gboolean		   db_exec_no_reply    (TrackerDBInterface *iface,
						const gchar	   *query,
						...);
static TrackerDBInterface *db_interface_create (TrackerDB	    db);

static gboolean		   initialized;
static GHashTable	  *prepared_queries;
static gchar		  *services_dir;
static gchar		  *sql_dir;
static gchar		  *data_dir;
static gchar		  *user_data_dir;
static gchar		  *sys_tmp_dir;
static gpointer		   db_type_enum_class_pointer;
static TrackerDBInterface *file_iface;
static TrackerDBInterface *email_iface;

static const gchar *
location_to_directory (TrackerDBLocation location)
{
	switch (location) {
	case TRACKER_DB_LOCATION_DATA_DIR:
		return data_dir;
	case TRACKER_DB_LOCATION_USER_DATA_DIR:
		return user_data_dir;
	case TRACKER_DB_LOCATION_SYS_TMP_DIR:
		return sys_tmp_dir;
	default:
		return NULL;
	};
}

static void
load_sql_file (TrackerDBInterface *iface,
	       const gchar	  *file,
	       const gchar	  *delimiter)
{
	gchar *path, *content, **queries;
	gint   count;
	gint   i;

	path = g_build_filename (sql_dir, file, NULL);

	if (!delimiter) {
		delimiter = ";";
	}

	if (!g_file_get_contents (path, &content, NULL, NULL)) {
		g_critical ("Cannot read SQL file:'%s', please reinstall tracker"
			    " or check read permissions on the file if it exists", file);
		g_assert_not_reached ();
	}

	queries = g_strsplit (content, delimiter, -1);

	for (i = 0, count = 0; queries[i]; i++) {
		GError *error = NULL;
		gchar  *sql;

		/* Skip white space, including control characters */
		for (sql = queries[i]; sql && g_ascii_isspace (sql[0]); sql++);

		if (!sql || sql[0] == '\0') {
			continue;
		}

		tracker_db_interface_execute_query (iface, &error, "%s", sql);

		if (error) {
			g_warning ("Error loading query:'%s' #%d, %s", file, i, error->message);
			g_error_free (error);
			continue;
		}

		count++;
	}

	g_message ("  Loaded SQL file:'%s' (%d queries)", file, count);

	g_strfreev (queries);
	g_free (content);
	g_free (path);
}

static void
load_metadata_file (TrackerDBInterface *iface,
		    const gchar        *filename)
{
	GKeyFile      *key_file = NULL;
	GError        *error = NULL;
	gchar	      *service_file, *str_id;
	gchar	     **groups, **keys;
	TrackerField  *def;
	gint	       id, i, j;

	g_message ("Loading metadata file '%s'", filename);

	key_file = g_key_file_new ();
	service_file = g_build_filename (services_dir, filename, NULL);

	if (!g_key_file_load_from_file (key_file, service_file, G_KEY_FILE_NONE, &error)) {
		g_critical ("Couldn't load service file, %s", 
			    error ? error->message : "no error given");
		g_clear_error (&error);
		g_free (service_file);
		g_key_file_free (key_file);
		return;
	}

	groups = g_key_file_get_groups (key_file, NULL);

	for (i = 0; groups[i]; i++) {
		def = tracker_ontology_get_field_by_name (groups[i]);

		if (!def) {
			g_message ("  Adding ontology metadata:'%s'", groups[i]);
			tracker_db_interface_execute_procedure (iface,
								NULL,
								"InsertMetadataType",
								groups[i],
								NULL);
			id = tracker_db_interface_sqlite_get_last_insert_id (TRACKER_DB_INTERFACE_SQLITE (iface));
		} else {
			id = atoi (tracker_field_get_id (def));
			g_error ("Duplicated metadata description %s", groups[i]);
		}

		str_id = tracker_guint_to_string (id);
		keys = g_key_file_get_keys (key_file, groups[i], NULL, NULL);

		for (j = 0; keys[j]; j++) {
			gchar *value, *new_value;

			value = g_key_file_get_locale_string (key_file, groups[i], keys[j], NULL, NULL);

			if (!value) {
				continue;
			}

			new_value = tracker_string_boolean_to_string_gint (value);
			g_free (value);

			if (strcasecmp (keys[j], "Parent") == 0) {
				tracker_db_interface_execute_procedure (iface,
									NULL,
									"InsertMetaDataChildren",
									str_id,
									new_value,
									NULL);
			} else if (strcasecmp (keys[j], "DataType") == 0) {
				GEnumValue *enum_value;

				enum_value = g_enum_get_value_by_nick (g_type_class_peek (TRACKER_TYPE_FIELD_TYPE), new_value);

				if (enum_value) {
					tracker_db_interface_execute_query (iface, NULL,
									    "update MetaDataTypes set DataTypeID = %d where ID = %d",
									    enum_value->value, id);
				} else {
					g_critical ("Field '%s' doesn't have a valid data type '%s'", groups[i], new_value);
				}
			} else {
				gchar *esc_value;

				esc_value = tracker_escape_string (new_value);

				tracker_db_interface_execute_query (iface, NULL,
								    "update MetaDataTypes set  %s = '%s' where ID = %d",
								    keys[j], esc_value, id);

				g_free (esc_value);
			}

			g_free (new_value);
		}

		g_free (str_id);
		g_strfreev (keys);
	}

	g_strfreev (groups);
	g_free (service_file);
	g_key_file_free (key_file);
}

static void
load_service_file (TrackerDBInterface *iface,
		   const gchar	      *filename)
{
	TrackerService	*service;
	GKeyFile	*key_file = NULL;
	GError          *error = NULL;
	gchar		*service_file, *str_id;
	gchar	       **groups, **keys;
	gint		 i, j, id;

	g_message ("Loading service file '%s'", filename);

	service_file = g_build_filename (services_dir, filename, NULL);

	key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (key_file, service_file, G_KEY_FILE_NONE, &error)) {
		g_critical ("Couldn't load service file, %s", 
			    error ? error->message : "no error given");
		g_clear_error (&error);
		g_free (service_file);
		g_key_file_free (key_file);
		return;
	}

	groups = g_key_file_get_groups (key_file, NULL);

	for (i = 0; groups[i]; i++) {
		service = tracker_ontology_get_service_by_name (groups[i]);

		if (!service) {
			g_message ("Adding ontology service type:'%s'", groups[i]);
			tracker_db_interface_execute_procedure (iface,
								NULL,
								"InsertServiceType",
								groups[i],
								NULL);
			id = tracker_db_interface_sqlite_get_last_insert_id (TRACKER_DB_INTERFACE_SQLITE (iface));
		} else {
			id = tracker_service_get_id (service);
		}

		str_id = tracker_guint_to_string (id);

		keys = g_key_file_get_keys (key_file, groups[i], NULL, NULL);

		for (j = 0; keys[j]; j++) {
			if (strcasecmp (keys[j], "TabularMetadata") == 0) {
				gchar **tab_array;
				gint	k;

				tab_array = g_key_file_get_string_list (key_file,
									groups[i],
									keys[j],
									NULL,
									NULL);

				for (k = 0; tab_array[k]; k++) {
					tracker_db_interface_execute_procedure (iface,
										NULL,
										"InsertServiceTabularMetadata",
										str_id,
										tab_array[k],
										NULL);
				}

				g_strfreev (tab_array);
			} else if (strcasecmp (keys[j], "TileMetadata") == 0) {
				gchar **tab_array;
				gint	k;

				tab_array = g_key_file_get_string_list (key_file,
									groups[i],
									keys[j],
									NULL,
									NULL);

				for (k = 0; tab_array[k]; k++) {
					tracker_db_interface_execute_procedure (iface,
										NULL,
										"InsertServiceTileMetadata",
										str_id,
										tab_array[k],
										NULL);
				}

				g_strfreev (tab_array);
			} else if (strcasecmp (keys[j], "Mimes") == 0) {
				gchar **tab_array;
				gint	k;

				tab_array = g_key_file_get_string_list (key_file,
									groups[i],
									keys[j],
									NULL,
									NULL);

				for (k = 0; tab_array[k]; k++) {
					tracker_db_interface_execute_procedure (iface, NULL,
										"InsertMimes",
										tab_array[k],
										NULL);
					tracker_db_interface_execute_query (iface,
									    NULL,
									    "update FileMimes set ServiceTypeID = %s where Mime = '%s'",
									    str_id,
									    tab_array[k]);
				}

				g_strfreev (tab_array);
			} else if (strcasecmp (keys[j], "MimePrefixes") == 0) {
				gchar **tab_array;
				gint	k;

				tab_array = g_key_file_get_string_list (key_file,
									groups[i],
									keys[j],
									NULL,
									NULL);

				for (k = 0; tab_array[k]; k++) {
					tracker_db_interface_execute_procedure (iface,
										NULL,
										"InsertMimePrefixes",
										tab_array[k],
										NULL);
					tracker_db_interface_execute_query (iface,
									    NULL,
									    "update FileMimePrefixes set ServiceTypeID = %s where MimePrefix = '%s'",
									    str_id,
									    tab_array[k]);
				}

				g_strfreev (tab_array);
			} else {
				gchar *value, *new_value, *esc_value;

				value = g_key_file_get_string (key_file, groups[i], keys[j], NULL);
				new_value = tracker_string_boolean_to_string_gint (value);
				esc_value = tracker_escape_string (new_value);

				/* Special case "Parent */
				if (g_ascii_strcasecmp (keys[j], "parent") == 0) {
					TrackerDBResultSet *result_set;
					gchar *query;

					query = g_strdup_printf ("SELECT TypeId FROM ServiceTypes WHERE TypeName = '%s'",
								 esc_value);
					result_set = tracker_db_interface_execute_query (iface, NULL, "%s", query);
					g_free (query);

					if (result_set) {
						GValue value = {0, };
						GValue transform = {0, };

						g_value_init (&transform, G_TYPE_STRING);
						
						_tracker_db_result_set_get_value (result_set, 0, &value);
						if (g_value_transform (&value, &transform)) {
							tracker_db_interface_execute_query (iface,
											    NULL,
											    "UPDATE ServiceTypes SET ParentId = '%s' WHERE TypeID = %s",
											    g_value_get_string (&transform),
											    str_id);

						}
						
						g_value_unset (&value);
						g_value_unset (&transform);
						g_object_unref (result_set);
					}
				}
				
				tracker_db_interface_execute_query (iface,
								    NULL,
								    "UPDATE ServiceTypes SET %s = '%s' WHERE TypeID = %s",
								    keys[j],
								    esc_value,
								    str_id);

				g_free (esc_value);
				g_free (value);
				g_free (new_value);
			}
		}

		g_free (str_id);
		g_strfreev (keys);
	}

	g_key_file_free (key_file);
	g_strfreev (groups);
	g_free (service_file);
}

static gboolean
db_exec_no_reply (TrackerDBInterface *iface,
		  const gchar	     *query,
		  ...)
{
	TrackerDBResultSet *result_set;
	va_list		    args;

	tracker_nfs_lock_obtain ();

	va_start (args, query);
	result_set = tracker_db_interface_execute_vquery (iface, NULL, query, args);
	va_end (args);

	if (result_set) {
		g_object_unref (result_set);
	}

	tracker_nfs_lock_release ();

	return TRUE;
}


static gboolean
load_prepared_queries (void)
{
	GTimer	    *t;
	GError	    *error = NULL;
	GMappedFile *mapped_file;
	GStrv	     queries;
	gchar	    *filename;
	gdouble      secs;

	g_message ("Loading prepared queries...");

	filename = g_build_filename (sql_dir, "sqlite-stored-procs.sql", NULL);

	t = g_timer_new ();

	mapped_file = g_mapped_file_new (filename, FALSE, &error);

	if (error || !mapped_file) {
		g_warning ("Could not get contents of SQL file:'%s', %s",
			   filename,
			   error ? error->message : "no error given");

		if (mapped_file) {
			g_mapped_file_free (mapped_file);
		}

		g_timer_destroy (t);
		g_free (filename);

		return FALSE;
	}

	g_message ("Loaded prepared queries file:'%s' size:%" G_GSIZE_FORMAT " bytes",
		   filename,
		   g_mapped_file_get_length (mapped_file));

	queries = g_strsplit (g_mapped_file_get_contents (mapped_file), "\n", -1);
	g_free (filename);

	if (queries) {
		GStrv p;
		gchar *start;
		gchar *end;

		start = NULL;
		end = NULL;

		for (p = queries; *p; p++) {
			GStrv        details;
			gchar       *line;
			const gchar *str;

			line = NULL;

			/* Check for comments */
			if (start) {
				if ((str = strstr (*p, "*/")) != NULL) {
					str += 2;
					end = g_strndup (str, strlen (*p) - strlen (str));
				} else {
					continue;
				}
			} else {
				if ((str = strstr (*p, "/*")) != NULL) {
					start = g_strndup (*p, str - *p);

					if ((str = strstr (*p, "*/")) != NULL) {
						str += 2;
						end = g_strndup (str, strlen (*p) - strlen (str));
					} else {
						continue;
					}
				} 			
			}

			/* Remove comments */
			if (start && end) {
				if (start[0] != '\0' && end[0] != '\0') {
					line = g_strconcat (start, end, NULL);
				}

				g_free (start);
				g_free (end);

				start = NULL;
				end = NULL;

				if (!line) {
					continue;
				}
			} else {
				line = *p;
			}

			/* Check for comments and empty lines */
			if (line[0] == '#' || line[0] == '\0') {
				if (line != *p) {
					g_free (line);
				}

				continue;
			}

			/* Continue processing */
			details = g_strsplit (line, " ", 2);

			if (line != *p) {
				g_free (line);
			}

			if (!details) {
				continue;
			} 
			else if (!details[0] || !details[1]) {
				g_strfreev (details);
				continue;
			}

			g_message ("  Adding query:'%s'", g_strstrip (details[0]));

			g_hash_table_insert (prepared_queries,
					     g_strdup (g_strstrip (details[0])),
					     g_strdup (g_strstrip (details[1])));
			g_strfreev (details);
		}

		g_strfreev (queries);
	}

	secs = g_timer_elapsed (t, NULL);
	g_timer_destroy (t);
	g_mapped_file_free (mapped_file);

	g_message ("Found %d prepared queries in %4.4f seconds",
		   g_hash_table_size (prepared_queries),
		   secs);

	return TRUE;
}

static TrackerField *
db_row_to_field_def (TrackerDBResultSet *result_set)
{
	TrackerField	 *field_def;
	TrackerFieldType  field_type;
	gchar		 *id_str, *field_name, *name;
	gint		  weight, id;
	gboolean	  embedded, multiple_values, delimited, filtered, store_metadata;

	field_def = tracker_field_new ();

	tracker_db_result_set_get (result_set,
				   0, &id,
				   1, &name,
				   2, &field_type,
				   3, &field_name,
				   4, &weight,
				   5, &embedded,
				   6, &multiple_values,
				   7, &delimited,
				   8, &filtered,
				   9, &store_metadata,
				   -1);

	id_str = tracker_gint_to_string (id);

	tracker_field_set_id (field_def, id_str);
	tracker_field_set_name (field_def, name);
	tracker_field_set_data_type (field_def, field_type);
	tracker_field_set_field_name (field_def, field_name);
	tracker_field_set_weight (field_def, weight);
	tracker_field_set_embedded (field_def, embedded);
	tracker_field_set_multiple_values (field_def, multiple_values);
	tracker_field_set_delimited (field_def, delimited);
	tracker_field_set_filtered (field_def, filtered);
	tracker_field_set_store_metadata (field_def, store_metadata);

	g_free (id_str);
	g_free (field_name);
	g_free (name);

	return field_def;
}

static TrackerService *
db_row_to_service (TrackerDBResultSet *result_set)
{
	TrackerService *service;
	GSList	       *new_list = NULL;
	gint		id, i;
	gchar	       *name, *parent, *content_metadata, *property_prefix = NULL;
	gboolean	enabled, embedded, has_metadata, has_fulltext;
	gboolean	has_thumbs, show_service_files, show_service_directories;

	service = tracker_service_new ();

	tracker_db_result_set_get (result_set,
				   0, &id,
				   1, &name,
				   2, &parent,
				   3, &property_prefix,
				   4, &enabled,
				   5, &embedded,
				   6, &has_metadata,
				   7, &has_fulltext,
				   8, &has_thumbs,
				   9, &content_metadata,
				   11, &show_service_files,
				   12, &show_service_directories,
				   -1);

	tracker_service_set_id (service, id);
	tracker_service_set_name (service, name);
	tracker_service_set_parent (service, parent);
	tracker_service_set_property_prefix (service, property_prefix);
	tracker_service_set_enabled (service, enabled);
	tracker_service_set_embedded (service, embedded);
	tracker_service_set_has_metadata (service, has_metadata);
	tracker_service_set_has_full_text (service, has_fulltext);
	tracker_service_set_has_thumbs (service, has_thumbs);
	tracker_service_set_content_metadata (service, content_metadata);

	tracker_service_set_show_service_files (service, show_service_files);
	tracker_service_set_show_service_directories (service, show_service_directories);

	for (i = 13; i < 24; i++) {
		gchar *metadata;

		tracker_db_result_set_get (result_set, i, &metadata, -1);

		if (metadata) {
			new_list = g_slist_prepend (new_list, metadata);
		}
	}

	/* FIXME: is this necessary?
	 * This values are set as key metadata in default.service already
	 */
#if 0
	/* Hack to prevent db change late in the cycle, check the
	 * service name matches "Applications", then add some voodoo.
	 */
	if (strcmp (name, "Applications") == 0) {
		/* These strings should be definitions at the top of
		 * this file somewhere really.
		 */
		new_list = g_slist_prepend (new_list, g_strdup ("App:DisplayName"));
		new_list = g_slist_prepend (new_list, g_strdup ("App:Exec"));
		new_list = g_slist_prepend (new_list, g_strdup ("App:Icon"));
	}
#endif

	new_list = g_slist_reverse (new_list);

	tracker_service_set_key_metadata (service, new_list);
	g_slist_foreach (new_list, (GFunc) g_free, NULL);
	g_slist_free (new_list);

	g_free (name);
	g_free (parent);
	g_free (property_prefix);
	g_free (content_metadata);

	return service;
}

static GSList *
db_mime_query (TrackerDBInterface *iface,
	       const gchar	  *stored_proc,
	       gint		   service_id)
{
	TrackerDBResultSet *result_set;
	GSList		   *result = NULL;
	gchar		   *service_id_str;

	service_id_str = g_strdup_printf ("%d", service_id);

	result_set = tracker_db_interface_execute_procedure (iface,
							     NULL,
							     stored_proc,
							     service_id_str,
							     NULL);
	g_free (service_id_str);

	if (result_set) {
		gchar	 *str;
		gboolean  valid = TRUE;

		while (valid) {
			tracker_db_result_set_get (result_set, 0, &str, -1);
			result = g_slist_prepend (result, str);
			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	return result;
}

static GSList *
db_get_mimes_for_service_id (TrackerDBInterface *iface,
			     gint		 service_id)
{
	return db_mime_query (iface, "GetMimeForServiceId", service_id);
}

static GSList *
db_get_mime_prefixes_for_service_id (TrackerDBInterface *iface,
				     gint		 service_id)
{
	return db_mime_query (iface, "GetMimePrefixForServiceId", service_id);
}


/* Converts date/time in UTC format to ISO 8160 standardised format for display */
static GValue
function_date_to_str (TrackerDBInterface *interface,
		      gint		  argc,
		      GValue		  values[])
{
	GValue	result = { 0, };
	gchar  *str;

	str = tracker_date_to_string (g_value_get_double (&values[0]));
	g_value_init (&result, G_TYPE_STRING);
	g_value_take_string (&result, str);

	return result;
}

static GValue
function_regexp (TrackerDBInterface *interface,
		 gint		     argc,
		 GValue		     values[])
{
	GValue	result = { 0, };
	regex_t	regex;
	int	ret;

	if (argc != 2) {
		g_critical ("Invalid argument count");
		return result;
	}

	ret = regcomp (&regex,
		       g_value_get_string (&values[0]),
		       REG_EXTENDED | REG_NOSUB);

	if (ret != 0) {
		g_critical ("Error compiling regular expression");
		return result;
	}

	ret = regexec (&regex,
		       g_value_get_string (&values[1]),
		       0, NULL, 0);

	g_value_init (&result, G_TYPE_INT);
	g_value_set_int (&result, (ret == REG_NOMATCH) ? 0 : 1);
	regfree (&regex);

	return result;
}

static void
function_group_concat_step (TrackerDBInterface *interface,
			    void               *aggregate_context,
			    gint		argc,
			    GValue		values[])
{
	AggregateData *p;

	g_return_if_fail (argc == 1);

	p = aggregate_context;

	if (!p->string) {
		p->string = g_string_new ("");
	} else {
		p->string = g_string_append (p->string, "|");
	}
	
	if (G_VALUE_HOLDS_STRING (&values[0])) {
		p->string = g_string_append (p->string, g_value_get_string (&values[0]));
	}
}

static GValue
function_group_concat_final (TrackerDBInterface *interface,
			     void               *aggregate_context)
{
	GValue result = { 0, };
	AggregateData *p;

	p = aggregate_context;

	g_value_init (&result, G_TYPE_STRING);
	g_value_set_string (&result, p->string->str);

	g_string_free (p->string, TRUE);

	return result;
}

static GValue
function_get_service_name (TrackerDBInterface *interface,
			   gint		       argc,
			   GValue	       values[])
{
	GValue result = { 0, };
	const gchar *str;

	str = tracker_ontology_get_service_by_id (g_value_get_int (&values[0]));
	g_value_init (&result, G_TYPE_STRING);
	g_value_set_string (&result, str);

	return result;
}

static GValue
function_get_service_type (TrackerDBInterface *interface,
			   gint		       argc,
			   GValue	       values[])
{
	GValue result = { 0, };
	gint   id;

	id = tracker_ontology_get_service_id_by_name (g_value_get_string (&values[0]));
	g_value_init (&result, G_TYPE_INT);
	g_value_set_int (&result, id);

	return result;
}

static GValue
function_get_max_service_type (TrackerDBInterface *interface,
			       gint		   argc,
			       GValue		   values[])
{
	GValue result = { 0, };
	gint   id;

	id = tracker_ontology_get_service_id_by_name (g_value_get_string (&values[0]));
	g_value_init (&result, G_TYPE_INT);
	g_value_set_int (&result, id);

	return result;
}

static gchar *
function_uncompress_string (const gchar *ptr,
			    gint	 size,
			    gint	*uncompressed_size)
{
	z_stream       zs;
	gchar	      *buf, *swap;
	unsigned char  obuf[ZLIB_BUF_SIZE];
	gint	       rv, asiz, bsiz, osiz;

	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;

	if (inflateInit2 (&zs, 15) != Z_OK) {
		return NULL;
	}

	asiz = size * 2 + 16;

	if (asiz < ZLIB_BUF_SIZE) {
		asiz = ZLIB_BUF_SIZE;
	}

	if (!(buf = malloc (asiz))) {
		inflateEnd (&zs);
		return NULL;
	}

	bsiz = 0;
	zs.next_in = (unsigned char *)ptr;
	zs.avail_in = size;
	zs.next_out = obuf;
	zs.avail_out = ZLIB_BUF_SIZE;

	while ((rv = inflate (&zs, Z_NO_FLUSH)) == Z_OK) {
		osiz = ZLIB_BUF_SIZE - zs.avail_out;

		if (bsiz + osiz >= asiz) {
			asiz = asiz * 2 + osiz;

			if (!(swap = realloc (buf, asiz))) {
				free (buf);
				inflateEnd (&zs);
				return NULL;
			}

			buf = swap;
		}

		memcpy (buf + bsiz, obuf, osiz);
		bsiz += osiz;
		zs.next_out = obuf;
		zs.avail_out = ZLIB_BUF_SIZE;
	}

	if (rv != Z_STREAM_END) {
		free (buf);
		inflateEnd (&zs);
		return NULL;
	}
	osiz = ZLIB_BUF_SIZE - zs.avail_out;

	if (bsiz + osiz >= asiz) {
		asiz = asiz * 2 + osiz;

		if (!(swap = realloc (buf, asiz))) {
			free (buf);
			inflateEnd (&zs);
			return NULL;
		}

		buf = swap;
	}

	memcpy (buf + bsiz, obuf, osiz);
	bsiz += osiz;
	buf[bsiz] = '\0';
	*uncompressed_size = bsiz;
	inflateEnd (&zs);

	return buf;
}

static GByteArray *
function_compress_string (const gchar *text)
{
	GByteArray *array;
	z_stream zs;
	gchar *buf, *swap;
	guchar obuf[ZLIB_BUF_SIZE];
	gint rv, asiz, bsiz, osiz, size;

	size = strlen (text);

	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;

	if (deflateInit2 (&zs, 6, Z_DEFLATED, 15, 6, Z_DEFAULT_STRATEGY) != Z_OK) {
		return NULL;
	}

	asiz = size + 16;

	if (asiz < ZLIB_BUF_SIZE) {
		asiz = ZLIB_BUF_SIZE;
	}

	if (!(buf = malloc (asiz))) {
		deflateEnd (&zs);
		return NULL;
	}

	bsiz = 0;
	zs.next_in = (unsigned char *) text;
	zs.avail_in = size;
	zs.next_out = obuf;
	zs.avail_out = ZLIB_BUF_SIZE;

	while ((rv = deflate (&zs, Z_FINISH)) == Z_OK) {
		osiz = ZLIB_BUF_SIZE - zs.avail_out;

		if (bsiz + osiz > asiz) {
			asiz = asiz * 2 + osiz;

			if (!(swap = realloc (buf, asiz))) {
				free (buf);
				deflateEnd (&zs);
				return NULL;
			}

			buf = swap;
		}

		memcpy (buf + bsiz, obuf, osiz);
		bsiz += osiz;
		zs.next_out = obuf;
		zs.avail_out = ZLIB_BUF_SIZE;
	}

	if (rv != Z_STREAM_END) {
		free (buf);
		deflateEnd (&zs);
		return NULL;
	}

	osiz = ZLIB_BUF_SIZE - zs.avail_out;

	if (bsiz + osiz + 1 > asiz) {
		asiz = asiz * 2 + osiz;

		if (!(swap = realloc (buf, asiz))) {
			free (buf);
			deflateEnd (&zs);
			return NULL;
		}

		buf = swap;
	}

	memcpy (buf + bsiz, obuf, osiz);
	bsiz += osiz;
	buf[bsiz] = '\0';

	array = g_byte_array_new ();
	g_byte_array_append (array, (const guint8 *) buf, bsiz);

	g_free (buf);

	deflateEnd (&zs);

	return array;
}

static GValue
function_uncompress (TrackerDBInterface *interface,
		     gint		 argc,
		     GValue		 values[])
{
	GByteArray *array;
	GValue	    result = { 0, };
	gchar	   *output;
	gint	    len;

	array = g_value_get_boxed (&values[0]);

	if (!array) {
		return result;
	}

	output = function_uncompress_string ((const gchar *) array->data,
					     array->len,
					     &len);

	if (!output) {
		g_warning ("Uncompress failed");
		return result;
	}

	g_value_init (&result, G_TYPE_STRING);
	g_value_take_string (&result, output);

	return result;
}

static GValue
function_compress (TrackerDBInterface *interface,
		   gint		       argc,
		   GValue	       values[])
{
	GByteArray *array;
	GValue result = { 0, };
	const gchar *text;

	text = g_value_get_string (&values[0]);

	array = function_compress_string (text);

	if (!array) {
		g_warning ("Compress failed");
		return result;
	}

	g_value_init (&result, TRACKER_TYPE_DB_BLOB);
	g_value_take_boxed (&result, array);

	return result;
}

static GValue
function_replace (TrackerDBInterface *interface,
		  gint		      argc,
		  GValue	      values[])
{
	GValue result = { 0, };
	gchar *str;

	str = tracker_string_replace (g_value_get_string (&values[0]),
				      g_value_get_string (&values[1]),
				      g_value_get_string (&values[2]));

	g_value_init (&result, G_TYPE_STRING);
	g_value_take_string (&result, str);

	return result;
}

static GValue
function_collate_key (TrackerDBInterface *interface,
		      gint                argc,
		      GValue              values[])
{
	GValue result = { 0 };
	gchar *collate_key;

	collate_key = g_utf8_collate_key (g_value_get_string (&values[0]), -1);

	g_value_init (&result, G_TYPE_STRING);
	g_value_take_string (&result, collate_key);

	return result;
}

static void
db_set_params (TrackerDBInterface *iface,
	       gint		   cache_size,
	       gint		   page_size,
	       gboolean		   add_functions)
{
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA synchronous = NORMAL;");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA count_changes = 0;");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA temp_store = FILE;");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA encoding = \"UTF-8\"");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA auto_vacuum = 0;");

	if (page_size != TRACKER_DB_PAGE_SIZE_DONT_SET) {
		g_message ("  Setting page size to %d", page_size);
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA page_size = %d", page_size);
	}

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d", cache_size);
	g_message ("  Setting cache size to %d", cache_size);

	if (add_functions) {
		g_message ("  Adding functions (FormatDate, etc)");

		/* Create user defined functions that can be used in sql */
		tracker_db_interface_sqlite_create_function (iface,
							     "FormatDate",
							     function_date_to_str,
							     1);
		tracker_db_interface_sqlite_create_function (iface,
							     "GetServiceName",
							     function_get_service_name,
							     1);
		tracker_db_interface_sqlite_create_function (iface,
							     "GetServiceTypeID",
							     function_get_service_type,
							     1);
		tracker_db_interface_sqlite_create_function (iface,
							     "GetMaxServiceTypeID",
							     function_get_max_service_type,
							     1);
		tracker_db_interface_sqlite_create_function (iface,
							     "REGEXP",
							     function_regexp,
							     2);

		tracker_db_interface_sqlite_create_function (iface,
							     "uncompress",
							     function_uncompress,
							     1);
		tracker_db_interface_sqlite_create_function (iface,
							     "compress",
							     function_compress,
							     1);
		tracker_db_interface_sqlite_create_function (iface,
							     "replace",
							     function_replace,
							     3);
		
		tracker_db_interface_sqlite_create_aggregate (iface,
							      "group_concat",
							      function_group_concat_step,
							      1,
							      function_group_concat_final,
							      sizeof(AggregateData));

		tracker_db_interface_sqlite_create_function (iface,
							     "CollateKey",
							     function_collate_key,
							     1);
	}
}

static void
db_get_static_data (TrackerDBInterface *iface)
{
	TrackerDBResultSet *result_set;

	/* Get static metadata info */
	result_set = tracker_db_interface_execute_procedure (iface,
							     NULL,
							     "GetMetadataTypes",
							     NULL);

	if (result_set) {
		gboolean valid = TRUE;
		gint	 id;

		while (valid) {
			TrackerDBResultSet *result_set2;
			TrackerField	   *def;
			GSList		   *child_ids = NULL;

			def = db_row_to_field_def (result_set);

			result_set2 = tracker_db_interface_execute_procedure (iface,
									      NULL,
									      "GetMetadataAliases",
									      tracker_field_get_id (def),
									      NULL);

			if (result_set2) {
				valid = TRUE;

				while (valid) {
					tracker_db_result_set_get (result_set2, 1, &id, -1);
					child_ids = g_slist_prepend (child_ids,
								     tracker_gint_to_string (id));

					valid = tracker_db_result_set_iter_next (result_set2);
				}

				tracker_field_set_child_ids (def, child_ids);
				g_object_unref (result_set2);

				g_slist_foreach (child_ids, (GFunc) g_free, NULL);
				g_slist_free (child_ids);
			}

			tracker_ontology_field_add (def);
			g_object_unref (def);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	/* Get static service info */
	result_set = tracker_db_interface_execute_procedure (iface,
							     NULL,
							     "GetAllServices",
							     NULL);

	if (result_set) {
		gboolean valid = TRUE;

		while (valid) {
			TrackerService *service;
			GSList	       *mimes, *mime_prefixes;
			const gchar    *name;
			gint		id;

			service = db_row_to_service (result_set);

			if (!service) {
				continue;
			}

			id = tracker_service_get_id (service);
			name = tracker_service_get_name (service);

			mimes = db_get_mimes_for_service_id (iface, id);
			mime_prefixes = db_get_mime_prefixes_for_service_id (iface, id);

			g_message ("Loading ontology service:'%s' with id:%d and mimes:%d",
				   name,
				   id,
				   g_slist_length (mimes));

			tracker_ontology_service_add (service,
						      mimes,
						      mime_prefixes);

			g_slist_free (mimes);
			g_slist_free (mime_prefixes);
			g_object_unref (service);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}
}

static const gchar *
db_type_to_string (TrackerDB db)
{
	GType	    type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	type = tracker_db_get_type ();
	enum_class = G_ENUM_CLASS (g_type_class_peek (type));
	enum_value = g_enum_get_value (enum_class, db);

	if (!enum_value) {
		return "unknown";
	}

	return enum_value->value_nick;
}

static TrackerDBInterface *
db_interface_get (TrackerDB  type,
		  gboolean  *create)
{
	TrackerDBInterface *iface;
	const gchar	   *path;

	path = dbs[type].abs_filename;

	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		*create = TRUE;
	} else {
		*create = FALSE;
	}

	g_message ("%s database... '%s' (%s)",
		   *create ? "Creating" : "Loading",
		   path,
		   db_type_to_string (type));

	iface = tracker_db_interface_sqlite_new (path);

	tracker_db_interface_set_procedure_table (iface,
						  prepared_queries);

	db_set_params (iface,
		       dbs[type].cache_size,
		       dbs[type].page_size,
		       dbs[type].add_functions);

	return iface;
}

static TrackerDBInterface *
db_interface_get_common (void)
{
	TrackerDBInterface *iface;
	gboolean	    create;

	iface = db_interface_get (TRACKER_DB_COMMON, &create);

	if (create) {
		GDir        *services;
		const gchar *conf_file;

		tracker_db_interface_start_transaction (iface);

		/* Create tables */
		load_sql_file (iface, "sqlite-tracker.sql", NULL);
		load_sql_file (iface, "sqlite-metadata.sql", NULL);
		load_sql_file (iface, "sqlite-service-types.sql", NULL);

		/*
		 * Loading .service and .metadata files. "default." first because
		 * contain the parent categories. 
		 */
		load_service_file (iface, "default.service");
		load_metadata_file (iface, "default.metadata");

		services = g_dir_open (services_dir, 0, NULL);

		conf_file = g_dir_read_name (services);

		while (conf_file) {
			if (!strcmp (conf_file, "default.service") ||
			    !strcmp (conf_file, "default.metadata")) {
				conf_file = g_dir_read_name (services);
				continue;
			}

			if (g_str_has_suffix (conf_file, ".service")) {
				load_service_file (iface, conf_file);
			}

			if (g_str_has_suffix (conf_file, ".metadata")) {
				load_metadata_file (iface, conf_file);
			}

			conf_file = g_dir_read_name (services);
		}

		g_dir_close (services);

		tracker_db_interface_end_transaction (iface);
	}

	/* Load static data into tracker ontology */
	db_get_static_data (iface);

	return iface;
}

static TrackerDBInterface *
db_interface_get_cache (void)
{
	TrackerDBInterface *iface;
	gboolean	    create;

	iface = db_interface_get (TRACKER_DB_CACHE, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);
		load_sql_file (iface, "sqlite-cache.sql", NULL);
		tracker_db_interface_end_transaction (iface);
	}

	return iface;
}

static TrackerDBInterface *
db_interface_get_file_metadata (void)
{
	TrackerDBInterface *iface;
	gboolean	    create;

	iface = db_interface_get (TRACKER_DB_FILE_METADATA, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);
		load_sql_file (iface, "sqlite-service.sql", NULL);
		load_sql_file (iface, "sqlite-service-triggers.sql", "!");
		tracker_db_interface_end_transaction (iface);
	}

	return iface;
}

static TrackerDBInterface *
db_interface_get_file_fulltext (void)
{
	TrackerDBInterface *iface;
	gboolean	    create;

	iface = db_interface_get (TRACKER_DB_FILE_FULLTEXT, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);
		load_sql_file (iface, "sqlite-fulltext.sql", NULL);
		tracker_db_interface_end_transaction (iface);
	}

	return iface;
}

static TrackerDBInterface *
db_interface_get_file_contents (void)
{
	TrackerDBInterface *iface;
	gboolean	    create;

	iface = db_interface_get (TRACKER_DB_FILE_CONTENTS, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);
		load_sql_file (iface, "sqlite-contents.sql", NULL);
		tracker_db_interface_end_transaction (iface);
	}

	tracker_db_interface_sqlite_create_function (iface,
						     "uncompress",
						     function_uncompress,
						     1);
	tracker_db_interface_sqlite_create_function (iface,
						     "compress",
						     function_compress,
						     1);

	return iface;
}



static TrackerDBInterface *
db_interface_get_email_metadata (void)
{
	TrackerDBInterface *iface;
	gboolean	    create;

	iface = db_interface_get (TRACKER_DB_EMAIL_METADATA, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);
		load_sql_file (iface, "sqlite-service.sql", NULL);
		load_sql_file (iface, "sqlite-email.sql", NULL);
		load_sql_file (iface, "sqlite-service-triggers.sql", "!");
		tracker_db_interface_end_transaction (iface);
	}

	return iface;
}

static TrackerDBInterface *
db_interface_get_email_fulltext (void)
{
	TrackerDBInterface *iface;
	gboolean	    create;

	iface = db_interface_get (TRACKER_DB_EMAIL_FULLTEXT, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);
		load_sql_file (iface, "sqlite-fulltext.sql", NULL);
		tracker_db_interface_end_transaction (iface);
	}

	return iface;
}

static TrackerDBInterface *
db_interface_get_email_contents (void)
{
	TrackerDBInterface *iface;
	gboolean	    create;

	iface = db_interface_get (TRACKER_DB_EMAIL_CONTENTS, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);
		load_sql_file (iface, "sqlite-contents.sql", NULL);
		tracker_db_interface_end_transaction (iface);
	}

	tracker_db_interface_sqlite_create_function (iface,
						     "uncompress",
						     function_uncompress,
						     1);
	tracker_db_interface_sqlite_create_function (iface,
						     "compress",
						     function_compress,
						     1);

	return iface;
}


static TrackerDBInterface *
db_interface_create (TrackerDB db)
{
	switch (db) {
	case TRACKER_DB_UNKNOWN:
		return NULL;

	case TRACKER_DB_COMMON:
		return db_interface_get_common ();

	case TRACKER_DB_CACHE:
		return db_interface_get_cache ();

	case TRACKER_DB_FILE_METADATA:
		return db_interface_get_file_metadata ();
		
	case TRACKER_DB_FILE_FULLTEXT:
		return db_interface_get_file_fulltext ();	

	case TRACKER_DB_FILE_CONTENTS:
		return db_interface_get_file_contents ();

	case TRACKER_DB_EMAIL_METADATA:
		return db_interface_get_email_metadata ();
		
	case TRACKER_DB_EMAIL_FULLTEXT:
		return db_interface_get_email_fulltext ();	
		
	case TRACKER_DB_EMAIL_CONTENTS:
		return db_interface_get_email_contents ();

	default:
		g_critical ("This TrackerDB type:%d->'%s' has no interface set up yet!!",
			    db,
			    db_type_to_string (db));
		return NULL;
	}
}

static void
db_manager_remove_all (void)
{
	guint i;

	g_message ("Removing all database files");

	/* NOTE: We don't have to be initialized for this so we
	 * calculate the absolute directories here. 
	 */
	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		g_message ("  Removing database:'%s'",
			   dbs[i].abs_filename);
		g_unlink (dbs[i].abs_filename);
	}
}

static TrackerDBVersion
db_get_version (void)
{
	TrackerDBVersion  version;
	gchar            *filename;

	filename = g_build_filename (data_dir, TRACKER_DB_VERSION_FILE, NULL);

	if (G_LIKELY (g_file_test (filename, G_FILE_TEST_EXISTS))) {
		gchar *contents;

		/* Check version is correct */
		if (G_LIKELY (g_file_get_contents (filename, &contents, NULL, NULL))) {
			if (contents && strlen (contents) <= 2) {
				version = atoi (contents);
			} else {
				g_message ("  Version file content size is either 0 or bigger than expected");

				version = TRACKER_DB_VERSION_UNKNOWN;
			} 

			g_free (contents);
		} else {
			g_message ("  Could not get content of file '%s'", filename);

			version = TRACKER_DB_VERSION_UNKNOWN;
		}
	} else {
		g_message ("  Could not find database version file:'%s'", filename);
		g_message ("  Current databases are either old or no databases are set up yet");

		version = TRACKER_DB_VERSION_UNKNOWN;
	}

	g_free (filename);

	return version;
}

static void
db_set_version (void)
{
	GError *error = NULL;
	gchar  *filename;
	gchar  *str;

	filename = g_build_filename (data_dir, TRACKER_DB_VERSION_FILE, NULL);
	g_message ("  Creating version file '%s'", filename);

	str = g_strdup_printf ("%d", TRACKER_DB_VERSION_NOW);

	if (!g_file_set_contents (filename, str, -1, &error)) {
		g_message ("  Could not set file contents, %s",
			   error ? error->message : "no error given");
		g_clear_error (&error);
	}

	g_free (str);
	g_free (filename);
}

static void
db_manager_analyze (TrackerDB db)
{
	TrackerDBInterface *iface;
	guint64             current_mtime;

	current_mtime = tracker_file_get_mtime (dbs[db].abs_filename);

	if (current_mtime > dbs[db].mtime) {
		g_message ("  Analyzing DB:'%s'", dbs[db].name);
		iface = tracker_db_manager_get_db_interface (db);
		db_exec_no_reply (iface, "ANALYZE %s.Services", dbs[db].name);

		/* Remember current mtime for future */
		dbs[db].mtime = current_mtime;
	} else {
		g_message ("  Not updating DB:'%s', no changes since last optimize", dbs[db].name);
	}
}

GType
tracker_db_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_DB_COMMON,
			  "TRACKER_DB_COMMON",
			  "common" },
			{ TRACKER_DB_CACHE,
			  "TRACKER_DB_CACHE",
			  "cache" },
			{ TRACKER_DB_FILE_METADATA,
			  "TRACKER_DB_FILE_METADATA",
			  "file metadata" },
			{ TRACKER_DB_FILE_CONTENTS,
			  "TRACKER_DB_FILE_CONTENTS",
			  "file contents" },
			{ TRACKER_DB_EMAIL_METADATA,
			  "TRACKER_DB_EMAIL_METADATA",
			  "email metadata" },
			{ TRACKER_DB_EMAIL_CONTENTS,
			  "TRACKER_DB_EMAIL_CONTENTS",
			  "email contents" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TrackerDB", values);
	}

	return etype;
}

static void
tracker_db_manager_ensure_locale (void)
{
	TrackerDBInterface *common, *iface;
	TrackerDBResultSet *result_set;
	const gchar *current_locale;
	gchar *stored_locale = NULL;

	current_locale = setlocale (LC_COLLATE, NULL);

	common = dbs[TRACKER_DB_COMMON].iface;
	result_set = tracker_db_interface_execute_procedure (common, NULL, "GetCollationLocale", NULL);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &stored_locale, -1);
		g_object_unref (result_set);
	}

	if (g_strcmp0 (current_locale, stored_locale) != 0) {
		guint collate_key;
		/* Locales differ, update collate keys */
		g_message ("Updating DB locale dependent data to: %s\n", current_locale);

		iface = dbs[TRACKER_DB_FILE_METADATA].iface;
		tracker_db_interface_execute_procedure (iface, NULL, "UpdateMetadataCollation", NULL);

		for (collate_key = 1; collate_key<6; collate_key++) {
			tracker_db_interface_execute_query (iface, NULL,
			   		    "UPDATE Services SET KeyMetadataCollation%d=CollateKey(KeyMetadata%d)",
					    collate_key, collate_key);
		} 

		iface = dbs[TRACKER_DB_EMAIL_METADATA].iface;
		tracker_db_interface_execute_procedure (iface, NULL, "UpdateMetadataCollation", NULL);

		tracker_db_interface_execute_procedure (common, NULL, "SetCollationLocale", current_locale, NULL);
	}

	g_free (stored_locale);
}

void
tracker_db_manager_init (TrackerDBManagerFlags	flags,
			 gboolean	       *first_time,
			 gboolean	        shared_cache)
{
	GType		    etype;
	TrackerDBVersion    version;
	gchar		   *filename;
	const gchar	   *dir;
	gboolean	    need_reindex;
	guint		    i;

	if (first_time) {
		*first_time = FALSE;
	}

	if (initialized) {
		return;
	}

	need_reindex = FALSE;

	/* Since we don't reference this enum anywhere, we do
	 * it here to make sure it exists when we call
	 * g_type_class_peek(). This wouldn't be necessary if
	 * it was a param in a GObject for example.
	 *
	 * This does mean that we are leaking by 1 reference
	 * here and should clean it up, but it doesn't grow so
	 * this is acceptable.
	 */
	etype = tracker_db_get_type ();
	db_type_enum_class_pointer = g_type_class_ref (etype);

	/* Set up locations */
	g_message ("Setting database locations");

	services_dir = g_build_filename (SHAREDIR,
					 "tracker",
					 "services",
					 NULL);
	sql_dir = g_build_filename (SHAREDIR,
				    "tracker",
				    NULL);

	user_data_dir = g_build_filename (g_get_user_data_dir (),
					  "tracker",
					  "data",
					  NULL);

	data_dir = g_build_filename (g_get_user_cache_dir (),
				     "tracker",
				     NULL);

	filename = g_strdup_printf ("tracker-%s", g_get_user_name ());
	sys_tmp_dir = g_build_filename (g_get_tmp_dir (), filename, NULL);
	g_free (filename);

	/* Make sure the directories exist */
	g_message ("Checking database directories exist");

	g_mkdir_with_parents (data_dir, 00755);
	g_mkdir_with_parents (user_data_dir, 00755);
	g_mkdir_with_parents (sys_tmp_dir, 00755);

	g_message ("Checking database version");

	version = db_get_version ();

	if (version < TRACKER_DB_VERSION_NOW) {
		g_message ("  A reindex will be forced");
		need_reindex = TRUE;
	}

	if (need_reindex) {
		db_set_version ();	
	}

	g_message ("Checking database files exist");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		/* Fill absolute path for the database */
		dir = location_to_directory (dbs[i].location);
		dbs[i].abs_filename = g_build_filename (dir, dbs[i].file, NULL);

		if (flags & TRACKER_DB_MANAGER_LOW_MEMORY_MODE) {
			dbs[i].cache_size /= 2;
		}

		/* Check we have each database in place, if one is
		 * missing, we reindex, except the cache which we
		 * expect to be replaced on each startup.
		 */
		if (i == TRACKER_DB_CACHE) {
			continue;
		}

		/* No need to check for other files not existing (for
		 * reindex) if one is already missing.
		 */
		if (need_reindex) {
			continue;
		}

		if (!g_file_test (dbs[i].abs_filename, G_FILE_TEST_EXISTS)) {
			g_message ("Could not find database file:'%s'", dbs[i].abs_filename);
			g_message ("One or more database files are missing, a reindex will be forced");
			need_reindex = TRUE;
		}
	}

	/* If we are just initializing to remove the databases,
	 * return here. 
	 */
	if ((flags & TRACKER_DB_MANAGER_REMOVE_ALL) != 0) {
		initialized = TRUE;
		return;
	}

	/* Set general database options */
	if (shared_cache) {
		g_message ("Enabling database shared cache");
		tracker_db_interface_sqlite_enable_shared_cache ();
	}

	/* Add prepared queries */
	prepared_queries = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_free);

	load_prepared_queries ();

	/* Should we reindex? If so, just remove all databases files,
	 * NOT the paths, note, that these paths are also used for
	 * other things like the nfs lock file.
	 */
	if (flags & TRACKER_DB_MANAGER_FORCE_REINDEX || need_reindex) {
		if (first_time) {
			*first_time = TRUE;
		}

		/* We call an internal version of this function here
		 * because at the time 'initialized' = FALSE and that
		 * will cause errors and do nothing.
		 */
		g_message ("Cleaning up database files for reindex");
		db_manager_remove_all ();

		/* In cases where we re-init this module, make sure
		 * we have cleaned up the ontology before we load all
		 * new databases.
		 */
		tracker_ontology_shutdown ();

		/* Make sure we initialize all other modules we depend on */
		tracker_ontology_init ();

		/* Now create the databases and close them */
		g_message ("Creating database files, this may take a few moments...");

		for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
			dbs[i].iface = db_interface_create (i);
		}

		/* We don't close the dbs in the same loop as before
		 * becase some databases need other databases
		 * attached to be created correctly.
		 */
		for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
			g_object_unref (dbs[i].iface);
			dbs[i].iface = NULL;
		}
	} else {
		/* Make sure we remove and recreate the cache directory in tmp
		 * each time we start up, this is meant to be a per-run
		 * thing.
		 */
		if (flags & TRACKER_DB_MANAGER_REMOVE_CACHE) {
			g_message ("Removing cache database:'%s'",
				   dbs[TRACKER_DB_CACHE].abs_filename);
			g_unlink (dbs[TRACKER_DB_CACHE].abs_filename);
		}

		/* Make sure we initialize all other modules we depend on */
		tracker_ontology_init ();
	}

	/* Load databases */
	g_message ("Loading databases files...");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		dbs[i].iface = db_interface_create (i);
		dbs[i].mtime = tracker_file_get_mtime (dbs[i].abs_filename);
	}

	tracker_db_manager_ensure_locale ();

	initialized = TRUE;
}

void
tracker_db_manager_shutdown (void)
{
	guint i;

	if (!initialized) {
		return;
	}

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		if (dbs[i].abs_filename) {
			g_free (dbs[i].abs_filename);
			dbs[i].abs_filename = NULL;

			if (dbs[i].iface) {
				g_object_unref (dbs[i].iface);
				dbs[i].iface = NULL;
			}
		}
	}

	if (prepared_queries) {
		g_hash_table_unref (prepared_queries);
		prepared_queries = NULL;
	}

	g_free (data_dir);
	g_free (user_data_dir);
	g_free (sys_tmp_dir);
	g_free (services_dir);
	g_free (sql_dir);

	if (file_iface) {
		g_object_unref (file_iface);
		file_iface = NULL;
	}

	if (email_iface) {
		g_object_unref (email_iface);
		email_iface = NULL;
	}


	/* Since we don't reference this enum anywhere, we do
	 * it here to make sure it exists when we call
	 * g_type_class_peek(). This wouldn't be necessary if
	 * it was a param in a GObject for example.
	 *
	 * This does mean that we are leaking by 1 reference
	 * here and should clean it up, but it doesn't grow so
	 * this is acceptable.
	 */
	g_type_class_unref (db_type_enum_class_pointer);
	db_type_enum_class_pointer = NULL;

	/* Make sure we shutdown all other modules we depend on */
	tracker_ontology_shutdown ();

	initialized = FALSE;
}

void
tracker_db_manager_remove_all (void)
{
	g_return_if_fail (initialized != FALSE);
	
	db_manager_remove_all ();
}

void
tracker_db_manager_optimize (void)
{
	gboolean dbs_are_open = FALSE;
	guint    i;

	g_return_if_fail (initialized != FALSE);

	g_message ("Optimizing databases...");

	g_message ("  Checking DBs are not open");

	/* Check if any connections are open? */
	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		if (G_OBJECT (dbs[i].iface)->ref_count > 1) {
			g_message ("  DB:'%s' is still open with %d references!",
				   dbs[i].name,
				   G_OBJECT (dbs[i].iface)->ref_count);
				   
			dbs_are_open = TRUE;
		}
	}

	if (dbs_are_open) {
		g_message ("  Not optimizing DBs, some are still open with > 1 reference");
		return;
	}

	/* Optimize the file/email content databases */
	db_manager_analyze (TRACKER_DB_FILE_METADATA);
	db_manager_analyze (TRACKER_DB_EMAIL_METADATA);
}

const gchar *
tracker_db_manager_get_file (TrackerDB db)
{
	g_return_val_if_fail (initialized != FALSE, NULL);

	return dbs[db].abs_filename;
}

/**
 * tracker_db_manager_get_db_interfaces:
 * @num: amount of TrackerDB files wanted
 * @...: All the files that you want in the connection as TrackerDB items
 *
 * Request a database connection where the first requested file gets connected
 * to and the subsequent requsted files get attached to the connection.
 *
 * The caller must g_object_unref the result when finished using it.
 *
 * returns: (caller-owns): a database connection
 **/
TrackerDBInterface *
tracker_db_manager_get_db_interfaces (gint num, ...)
{
	gint		    n_args;
	va_list		    args;
	TrackerDBInterface *connection = NULL;

	g_return_val_if_fail (initialized != FALSE, NULL);

	va_start (args, num);
	for (n_args = 1; n_args <= num; n_args++) {
		TrackerDB db = va_arg (args, TrackerDB);

		if (!connection) {
			connection = tracker_db_interface_sqlite_new (dbs[db].abs_filename);
			tracker_db_interface_set_procedure_table (connection,
								  prepared_queries);

			db_set_params (connection,
				       dbs[db].cache_size,
				       dbs[db].page_size,
				       TRUE);

		} else {
			db_exec_no_reply (connection,
					  "ATTACH '%s' as '%s'",
					  dbs[db].abs_filename,
					  dbs[db].name);
		}

	}
	va_end (args);

	return connection;
}

TrackerDBInterface *
tracker_db_manager_get_db_interfaces_ro (gint num, ...)
{
	gint		    n_args;
	va_list		    args;
	TrackerDBInterface *connection = NULL;

	g_return_val_if_fail (initialized != FALSE, NULL);

	va_start (args, num);
	for (n_args = 1; n_args <= num; n_args++) {
		TrackerDB db = va_arg (args, TrackerDB);

		if (!connection) {
			connection = tracker_db_interface_sqlite_new_ro (dbs[db].abs_filename);
			tracker_db_interface_set_procedure_table (connection,
								  prepared_queries);
			db_set_params (connection,
				       dbs[db].cache_size,
				       dbs[db].page_size,
				       TRUE);
		} else {
			db_exec_no_reply (connection,
					  "ATTACH '%s' as '%s'",
					  dbs[db].abs_filename,
					  dbs[db].name);
		}

	}
	va_end (args);

	return connection;
}


/**
 * tracker_db_manager_get_db_interface:
 * @db: the database file wanted
 *
 * Request a database connection to the database file @db.
 *
 * The caller must NOT g_object_unref the result
 *
 * returns: (callee-owns): a database connection
 **/
TrackerDBInterface *
tracker_db_manager_get_db_interface (TrackerDB db)
{
	g_return_val_if_fail (initialized != FALSE, NULL);

	return dbs[db].iface;
}

/**
 * tracker_db_manager_get_db_interface_by_service:
 * @service: the server for which you'll use the database connection
 *
 * Request a database connection that can be used for @service. At this moment
 * service can either be "Files", "Emails", "Attachments".
 *
 * The caller must NOT g_object_unref the result
 *
 * returns: (callee-owns): a database connection
 **/
TrackerDBInterface *
tracker_db_manager_get_db_interface_by_service (const gchar *service)
{
	TrackerDBInterface	  *iface;
	TrackerDBType		   type;

	g_return_val_if_fail (initialized != FALSE, NULL);

	type = tracker_ontology_get_service_db_by_name (service);

	switch (type) {
	case TRACKER_DB_TYPE_EMAIL:
		if (!email_iface) {
			email_iface = tracker_db_manager_get_db_interfaces (4,
									    TRACKER_DB_COMMON,
									    TRACKER_DB_EMAIL_CONTENTS,
									    TRACKER_DB_EMAIL_METADATA,
									    TRACKER_DB_CACHE);
		}

		iface = email_iface;
		break;

	case TRACKER_DB_TYPE_UNKNOWN:
	case TRACKER_DB_TYPE_DATA:
	case TRACKER_DB_TYPE_INDEX:
	case TRACKER_DB_TYPE_COMMON:
	case TRACKER_DB_TYPE_CONTENT:
	case TRACKER_DB_TYPE_CACHE:
	case TRACKER_DB_TYPE_USER:
		g_warning ("Defaulting to Files DB. Strange DB Type for service '%s'", 
			   service);

	case TRACKER_DB_TYPE_FILES:
	default:
		if (!file_iface) {
			file_iface = tracker_db_manager_get_db_interfaces (4,
									   TRACKER_DB_COMMON,
									   TRACKER_DB_FILE_CONTENTS,
									   TRACKER_DB_FILE_METADATA,
									   TRACKER_DB_CACHE);
		}

		iface = file_iface;
		break;
	}

	return iface;
}

TrackerDBInterface *
tracker_db_manager_get_db_interface_by_type (const gchar	  *service,
					     TrackerDBContentType  content_type)
{
	TrackerDBType type;
	TrackerDB     db;

	g_return_val_if_fail (initialized != FALSE, NULL);
	g_return_val_if_fail (service != NULL, NULL);

	type = tracker_ontology_get_service_db_by_name (service);

	switch (type) {
	case TRACKER_DB_TYPE_EMAIL:
		if (content_type == TRACKER_DB_CONTENT_TYPE_METADATA) {
			db = TRACKER_DB_EMAIL_METADATA;
		} else {
			db = TRACKER_DB_EMAIL_CONTENTS;
		}
		break;

	case TRACKER_DB_TYPE_FILES:
		if (content_type == TRACKER_DB_CONTENT_TYPE_METADATA) {
			db = TRACKER_DB_FILE_METADATA;
		} else {
			db = TRACKER_DB_FILE_CONTENTS;
		}
		break;

	case TRACKER_DB_TYPE_UNKNOWN:
	case TRACKER_DB_TYPE_DATA:
	case TRACKER_DB_TYPE_INDEX:
	case TRACKER_DB_TYPE_COMMON:
	case TRACKER_DB_TYPE_CONTENT:
	case TRACKER_DB_TYPE_CACHE:
	case TRACKER_DB_TYPE_USER:
	default:
		g_warning ("Database type not supported");
		return NULL;
	}

	return tracker_db_manager_get_db_interface (db);
}

gboolean
tracker_db_manager_are_db_too_big (void)
{
	const gchar *filename_const;
	gboolean     too_big;

	filename_const = tracker_db_manager_get_file (TRACKER_DB_FILE_METADATA);
	too_big = tracker_file_get_size (filename_const) > TRACKER_DB_MAX_FILE_SIZE;

	if (too_big) {
		g_critical ("File metadata database is too big, discontinuing indexing");
		return TRUE;
	}

	filename_const = tracker_db_manager_get_file (TRACKER_DB_EMAIL_METADATA);
	too_big = tracker_file_get_size (filename_const) > TRACKER_DB_MAX_FILE_SIZE;

	if (too_big) {
		g_critical ("Email metadata database is too big, discontinuing indexing");
		return TRUE;
	}

	return FALSE;

}
