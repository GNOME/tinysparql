/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 */

#include "config.h"

#include <glib/gstdio.h>

#include <sqlite3.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#include <tracker-common.h>

#include "tracker-cursor.h"
#include "tracker-private.h"

#include "core/tracker-fts-tokenizer.h"
#include "core/tracker-collation.h"
#include "core/tracker-db-interface-sqlite.h"
#include "core/tracker-db-manager.h"
#include "core/tracker-data-enum-types.h"
#include "core/tracker-uuid.h"
#include "core/tracker-vtab-service.h"
#include "core/tracker-vtab-triples.h"

/* Avoid casts everywhere. */
#define sqlite3_value_text(x) ((const gchar *) sqlite3_value_text(x))

typedef struct {
	GRegex *syntax_check;
	GRegex *replacement;
	GRegex *unescape;
} TrackerDBReplaceFuncChecks;

struct TrackerDBInterface {
	GObject parent_instance;

	gchar *filename;
	sqlite3 *db;

	/* Compiled regular expressions */
	TrackerDBReplaceFuncChecks replace_func_checks;

	/* Number of users (e.g. active cursors) */
	gint n_users;

	guint flags;
	GCancellable *cancellable;
	gboolean corrupted;

	TrackerDBStatementMru select_stmt_mru;

	GMutex mutex;

	/* User data */
	GObject *user_data;
};

struct TrackerDBInterfaceClass {
	GObjectClass parent_class;
};

struct TrackerDBCursor {
	TrackerSparqlCursor parent_instance;
	sqlite3_stmt *stmt;
	TrackerDBStatement *ref_stmt;
	gboolean finished;
	guint n_columns;
};

struct TrackerDBCursorClass {
	TrackerSparqlCursorClass parent_class;
};

struct TrackerDBStatement {
	GInitiallyUnowned parent_instance;
	TrackerDBInterface *db_interface;
	sqlite3_stmt *stmt;
	guint stmt_is_used : 1;
	guint stmt_is_borrowed : 1;
	TrackerDBStatement *next;
	TrackerDBStatement *prev;
	gconstpointer mru_key;
};

struct TrackerDBStatementClass {
	GObjectClass parent_class;
};

static void                tracker_db_interface_initable_iface_init (GInitableIface        *iface);
static TrackerDBStatement *tracker_db_statement_sqlite_new          (TrackerDBInterface    *db_interface,
                                                                     sqlite3_stmt          *sqlite_stmt);
static void                tracker_db_statement_sqlite_reset        (TrackerDBStatement    *stmt);
static TrackerDBCursor    *tracker_db_cursor_sqlite_new             (TrackerDBStatement    *ref_stmt,
                                                                     guint                  n_columns);
static gboolean            tracker_db_cursor_get_boolean            (TrackerSparqlCursor   *cursor,
                                                                     gint                   column);
static gboolean            db_cursor_iter_next                      (TrackerDBCursor       *cursor,
                                                                     GCancellable          *cancellable,
                                                                     GError               **error);

void tracker_db_cursor_rewind (TrackerSparqlCursor *cursor);

gboolean tracker_db_cursor_iter_next (TrackerSparqlCursor  *cursor,
                                      GCancellable         *cancellable,
                                      GError              **error);

gint tracker_db_cursor_get_n_columns (TrackerSparqlCursor *cursor);

const gchar* tracker_db_cursor_get_variable_name (TrackerSparqlCursor *cursor,
                                                  gint                 column);

TrackerSparqlValueType tracker_db_cursor_get_value_type (TrackerSparqlCursor *cursor,
                                                         gint                 column);

const gchar* tracker_db_cursor_get_string (TrackerSparqlCursor  *cursor,
                                           gint                  column,
                                           const gchar         **langtag,
                                           glong                *length);

gint64 tracker_db_cursor_get_int (TrackerSparqlCursor *cursor,
                                  gint                 column);

gdouble tracker_db_cursor_get_double (TrackerSparqlCursor *cursor,
                                      gint                 column);

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_FLAGS,
};

enum {
	TRACKER_DB_CURSOR_PROP_0,
	TRACKER_DB_CURSOR_PROP_N_COLUMNS
};

G_DEFINE_TYPE_WITH_CODE (TrackerDBInterface, tracker_db_interface, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                tracker_db_interface_initable_iface_init))

G_DEFINE_TYPE (TrackerDBStatement, tracker_db_statement, G_TYPE_INITIALLY_UNOWNED)

G_DEFINE_TYPE (TrackerDBCursor, tracker_db_cursor, TRACKER_SPARQL_TYPE_CURSOR)

static void
result_context_function_error (sqlite3_context *context,
			       const gchar     *sparql_function,
			       const gchar     *error_message)
{
	gchar *message;

	message = g_strdup_printf ("%s: %s", sparql_function, error_message);
	sqlite3_result_error (context, message, -1);

	g_free (message);
}

#define TRACKER_RETURN_IF_FAIL(cond,fn,msg) \
	if (!((cond))) { \
		result_context_function_error (context, (fn), (msg)); \
		return; \
	}

static void
function_sparql_string_join (sqlite3_context *context,
                             int              argc,
                             sqlite3_value   *argv[])
{
	const gchar *fn = "fn:string-join";
	GString *str = NULL;
	const gchar *separator;
	gsize len;
	gint i;

	/* fn:string-join (str1, str2, ..., separator) */

	TRACKER_RETURN_IF_FAIL (argc >= 1, fn, "Invalid number of parameters");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_type (argv[argc - 1]) == SQLITE_TEXT,
	                        fn, "Invalid separator");

	separator = (gchar *)sqlite3_value_text (argv[argc-1]);

	str = g_string_new ("");

	for (i = 0; i < argc - 1; i++) {
		if (sqlite3_value_type (argv[argc-1]) == SQLITE_TEXT) {
			const gchar *text = (gchar *)sqlite3_value_text (argv[i]);

			if (text != NULL) {
				if (str->len == 0) {
					g_string_append (str, text);
				} else {
					g_string_append_printf (str, "%s%s", separator, text);
				}
			}
		}
	}

	len = str->len;
	sqlite3_result_text (context,
	                     g_string_free (str, FALSE),
	                     len, g_free);
}

/* Create a title-type string from the filename for replacing missing ones */
static void
function_sparql_string_from_filename (sqlite3_context *context,
                                      int              argc,
                                      sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:string-from-filename";
	gchar  *name = NULL;
	gchar  *suffix = NULL;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	/* "/home/user/path/title_of_the_movie.movie" -> "title of the movie"
	 * Only for local files currently, do we need to change? */

	name = g_filename_display_basename ((gchar *)sqlite3_value_text (argv[0]));
	suffix = g_strrstr (name, ".");

	if (suffix) {
		*suffix = '\0';
	}

	g_strdelimit (name, "._", ' ');

	sqlite3_result_text (context, name, -1, g_free);
}

static void
function_sparql_uri_is_parent (sqlite3_context *context,
                               int              argc,
                               sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:uri-is-parent";
	const gchar *uri, *parent, *remaining;
	gboolean match = FALSE;
	gssize parent_len;

	TRACKER_RETURN_IF_FAIL (argc == 2, fn, "Invalid argument count");

	parent = (gchar *)sqlite3_value_text (argv[0]);
	uri = (gchar *)sqlite3_value_text (argv[1]);

	if (!parent || !uri) {
		sqlite3_result_int (context, FALSE);
		return;
	}

	parent_len = sqlite3_value_bytes (argv[0]);

	/* Check only one argument, it's going to
	 * be compared with the other anyway.
	 */

	if (!(parent_len >= 7 && (parent[4] == ':' && parent[5] == '/' && parent[6] == '/'))) {
		if (strstr (parent, "://") == NULL) {
			sqlite3_result_int (context, FALSE);
			return;
		}
	}

	/* Remove trailing '/', will
	 * be checked later on uri.
	 */
	while (parent_len > 0 && parent[parent_len - 1] == '/') {
		parent_len--;
	}

	if (strncmp (uri, parent, parent_len) == 0 && uri[parent_len] == '/') {
		const gchar *slash;

		while (uri[parent_len] == '/') {
			parent_len++;
		}

		remaining = &uri[parent_len];

		if (*remaining == '\0') {
			/* Exact match, not a child */
			match = FALSE;
		} else if ((slash = strchr (remaining, '/')) == NULL) {
			/* Remaining doesn't have uri
			 * separator, it's a direct child.
			 */
			match = TRUE;
		} else {
			/* Check it's not trailing slashes */
			while (*slash == '/') {
				slash++;
			}

			match = (*slash == '\0');
		}
	}

	sqlite3_result_int (context, match);
}

static gboolean
check_uri_is_descendant (const gchar *parent,
                         gssize       parent_len,
                         const gchar *uri)
{
	const gchar *remaining;
	gboolean match = FALSE;

	/* Check only one argument, it's going to
	 * be compared with the other anyway.
	 */

	if (!(parent_len >= 7 && (parent[4] == ':' && parent[5] == '/' && parent[6] == '/'))) {
		if (strstr (parent, "://") == NULL) {
			return FALSE;
		}
	}

	/* Remove trailing '/', will
	 * be checked later on uri.
	 */
	while (parent_len > 0 && parent[parent_len - 1] == '/') {
		parent_len--;
	}

	if (strncmp (uri, parent, parent_len) == 0 && uri[parent_len] == '/') {
		while (uri[parent_len] == '/') {
			parent_len++;
		}

		remaining = &uri[parent_len];

		if (remaining && *remaining) {
			match = TRUE;
		}
	}

	return match;
}

static void
function_sparql_uri_is_descendant (sqlite3_context *context,
                                   int              argc,
                                   sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:uri-is-descendant";
	const gchar *child;
	gboolean match = FALSE;
	gint i;

	/* fn:uri-is-descendant (parent1, parent2, ..., parentN, child) */

	TRACKER_RETURN_IF_FAIL (argc >= 2, fn, "Invalid argument count");

	for (i = 0; i < argc; i++) {
		TRACKER_RETURN_IF_FAIL (sqlite3_value_type (argv[i]) == SQLITE_TEXT ||
		                        sqlite3_value_type (argv[i]) == SQLITE_NULL,
		                        fn, "Invalid non-text argument");
	}

	child = (gchar *)sqlite3_value_text (argv[argc-1]);

	for (i = 0; i < argc - 1 && !match; i++) {
		if (sqlite3_value_type (argv[i]) == SQLITE_TEXT) {
			const gchar *parent = (gchar *)sqlite3_value_text (argv[i]);
			gssize parent_len = sqlite3_value_bytes (argv[i]);

			match = check_uri_is_descendant (parent, parent_len, child);
		}
	}

	sqlite3_result_int (context, match);
}

static void
function_sparql_timestamp (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlTimestamp helper";

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_type (argv[0]) == SQLITE_INTEGER ||
	                        sqlite3_value_type (argv[0]) == SQLITE_TEXT ||
	                        sqlite3_value_type (argv[0]) == SQLITE_NULL,
	                        fn, "Invalid argument type");

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		gdouble seconds;

		seconds = sqlite3_value_double (argv[0]);
		sqlite3_result_double (context, seconds);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		GError *error = NULL;
		GDateTime *datetime;
		GTimeZone *tz;
		const gchar *str;

		tz = sqlite3_get_auxdata (context, 1);

		if (!tz) {
			tz = g_time_zone_new_local ();
			sqlite3_set_auxdata (context, 1, tz, (void (*) (void*)) g_time_zone_unref);
		}

		str = sqlite3_value_text (argv[0]);
		datetime = tracker_date_new_from_iso8601 (tz, str, &error);
		if (error) {
			result_context_function_error (context, fn, "Failed time string conversion");
			g_error_free (error);
			return;
		}

		sqlite3_result_int64 (context,
				      g_date_time_to_unix (datetime) +
				      (g_date_time_get_utc_offset (datetime) / G_USEC_PER_SEC));
		g_date_time_unref (datetime);
	}
}

static void
function_sparql_time_sort (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlTimeSort helper";
	gint64 sort_key = 0;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER ||
	                        sqlite3_value_numeric_type (argv[0]) == SQLITE_FLOAT ||
	                        sqlite3_value_type (argv[0]) == SQLITE_TEXT ||
	                        sqlite3_value_type (argv[0]) == SQLITE_NULL,
	                        fn, "Invalid argument type");

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
		return;
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER ||
	           sqlite3_value_numeric_type (argv[0]) == SQLITE_FLOAT) {
		gdouble value;

		value = sqlite3_value_double (argv[0]);
		sort_key = (gint64) (value * G_USEC_PER_SEC);
		sqlite3_result_int64 (context, sort_key);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		GDateTime *datetime;
		GTimeZone *tz;
		const gchar *value;
		GError *error = NULL;

		tz = sqlite3_get_auxdata (context, 1);

		if (!tz) {
			tz = g_time_zone_new_local ();
			sqlite3_set_auxdata (context, 1, tz, (void (*) (void*)) g_time_zone_unref);
		}

		value = sqlite3_value_text (argv[0]);
		datetime = tracker_date_new_from_iso8601 (tz, value, &error);
		if (error) {
			result_context_function_error (context, fn, error->message);
			g_error_free (error);
			return;
		}

		sort_key = ((g_date_time_to_unix (datetime) * G_USEC_PER_SEC) +
			    g_date_time_get_microsecond (datetime));
		sqlite3_result_int64 (context, sort_key);
		g_date_time_unref (datetime);
	}
}

static void
function_sparql_time_zone_duration (sqlite3_context *context,
                                    int              argc,
                                    sqlite3_value   *argv[])
{
	const gchar *fn = "timezone-from-dateTime";

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER ||
	                        sqlite3_value_type (argv[0]) == SQLITE_TEXT ||
	                        sqlite3_value_type (argv[0]) == SQLITE_NULL,
	                        fn, "Invalid argument type");

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		sqlite3_result_int (context, 0);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		GError *error = NULL;
		GDateTime *datetime;
		GTimeZone *tz;
		const gchar *str;

		tz = sqlite3_get_auxdata (context, 1);

		if (!tz) {
			tz = g_time_zone_new_local ();
			sqlite3_set_auxdata (context, 1, tz, (void (*) (void*)) g_time_zone_unref);
		}

		str = sqlite3_value_text (argv[0]);
		datetime = tracker_date_new_from_iso8601 (tz, str, &error);
		if (error) {
			result_context_function_error (context, fn, "Invalid date");
			g_error_free (error);
			return;
		}

		sqlite3_result_int64 (context,
				      g_date_time_get_utc_offset (datetime) /
				      G_USEC_PER_SEC);
		g_date_time_unref (datetime);
	}
}

static void
function_sparql_time_zone_substr (sqlite3_context *context,
                                  int              argc,
                                  sqlite3_value   *argv[])
{
	TRACKER_RETURN_IF_FAIL (argc == 1, "TZ", "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER ||
	                        sqlite3_value_type (argv[0]) == SQLITE_TEXT ||
	                        sqlite3_value_type (argv[0]) == SQLITE_NULL,
	                        "TZ", "Invalid argument type");

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		sqlite3_result_text (context, "", -1, NULL);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		const gchar *str;
		gsize len;

		str = sqlite3_value_text (argv[0]);
		len = strlen (str);

		if (g_str_has_suffix (str, "Z")) {
			sqlite3_result_text (context, "Z", -1, NULL);
		} else if (len > strlen ("0000-00-00T00:00:00Z")) {
			const gchar *tz = "";

			/* [+-]HHMM */
			if (str[len - 5] == '+' || str[len - 5] == '-')
				tz = &str[len - 5];
			/* [+-]HH:MM */
			else if (str[len - 6] == '+' || str[len - 6] == '-')
				tz = &str[len - 6];

			sqlite3_result_text (context, g_strdup (tz), -1, g_free);
		} else {
			sqlite3_result_text (context, "", -1, NULL);
		}
	}
}

static gchar *
offset_to_duration (gint offset)
{
	GString *str = g_string_new (NULL);
	gint hours, minutes, seconds;

	if (offset > 0)
		g_string_append (str, "+PT");
	else
		g_string_append (str, "-PT");

	offset = ABS (offset);
	hours = offset / 3600;
	minutes = offset % 3600 / 60;
	seconds = offset % 60;

	if (hours > 0)
		g_string_append_printf (str, "%dH", hours);
	if (minutes > 0)
		g_string_append_printf (str, "%dM", minutes);
	if (seconds > 0)
		g_string_append_printf (str, "%dS", seconds);

	return g_string_free (str, FALSE);
}

static void
function_sparql_time_zone (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlTimezone helper";

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER ||
	                        sqlite3_value_type (argv[0]) == SQLITE_TEXT ||
	                        sqlite3_value_type (argv[0]) == SQLITE_NULL,
	                        fn, "Invalid argument type");

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		sqlite3_result_text (context, "PT0S", -1, NULL);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		GError *error = NULL;
		GDateTime *datetime;
		GTimeZone *tz;
		const gchar *str;
		gchar *duration;

		tz = sqlite3_get_auxdata (context, 1);

		if (!tz) {
			tz = g_time_zone_new_local ();
			sqlite3_set_auxdata (context, 1, tz, (void (*) (void*)) g_time_zone_unref);
		}

		str = sqlite3_value_text (argv[0]);
		datetime = tracker_date_new_from_iso8601 (tz, str, &error);
		if (error) {
			result_context_function_error (context, fn, "Invalid date");
			g_error_free (error);
			return;
		}

		duration = offset_to_duration (g_date_time_get_utc_offset (datetime) /
					       G_USEC_PER_SEC);
		sqlite3_result_text (context, duration, -1, g_free);
		g_date_time_unref (datetime);
	}
}

static void
function_sparql_cartesian_distance (sqlite3_context *context,
                                    int              argc,
                                    sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:cartesian-distance";

	gdouble lat1;
	gdouble lat2;
	gdouble lon1;
	gdouble lon2;

	gdouble R;
	gdouble a;
	gdouble b;
	gdouble c;
	gdouble d;

	TRACKER_RETURN_IF_FAIL (argc == 4, fn, "Invalid argument count");

	lat1 = sqlite3_value_double (argv[0])*M_PI/180;
	lat2 = sqlite3_value_double (argv[1])*M_PI/180;
	lon1 = sqlite3_value_double (argv[2])*M_PI/180;
	lon2 = sqlite3_value_double (argv[3])*M_PI/180;

	R = 6371000;
	a = M_PI/2 - lat1;
	b = M_PI/2 - lat2;
	c = sqrt(a*a + b*b - 2*a*b*cos(lon2 - lon1));
	d = R*c;

	sqlite3_result_double (context, d);
}

static void
function_sparql_haversine_distance (sqlite3_context *context,
                                    int              argc,
                                    sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:haversine-distance";

	gdouble lat1;
	gdouble lat2;
	gdouble lon1;
	gdouble lon2;

	gdouble R;
	gdouble dLat;
	gdouble dLon;
	gdouble a;
	gdouble c;
	gdouble d;

	TRACKER_RETURN_IF_FAIL (argc == 4, fn, "Invalid argument count");

	lat1 = sqlite3_value_double (argv[0])*M_PI/180;
	lat2 = sqlite3_value_double (argv[1])*M_PI/180;
	lon1 = sqlite3_value_double (argv[2])*M_PI/180;
	lon2 = sqlite3_value_double (argv[3])*M_PI/180;

	R = 6371000;
	dLat = (lat2-lat1);
	dLon = (lon2-lon1);
	a = sin(dLat/2) * sin(dLat/2) + cos(lat1) * cos(lat2) *  sin(dLon/2) * sin(dLon/2);
	c = 2 * atan2(sqrt(a), sqrt(1-a));
	d = R * c;

	sqlite3_result_double (context, d);
}

static void
function_sparql_regex (sqlite3_context *context,
                       int              argc,
                       sqlite3_value   *argv[])
{
	const gchar *fn = "fn:matches";
	gboolean ret;
	const gchar *text, *pattern, *flags = "";
	GRegexCompileFlags regex_flags;
	GRegex *regex;

	TRACKER_RETURN_IF_FAIL (argc == 2 || argc == 3, fn, "Invalid argument count");

	regex = sqlite3_get_auxdata (context, 1);

	text = (gchar *)sqlite3_value_text (argv[0]);

	if (argc == 3)
		flags = (gchar *)sqlite3_value_text (argv[2]);

	if (regex == NULL) {
		gchar *err_str;
		GError *error = NULL;

		pattern = (gchar *)sqlite3_value_text (argv[1]);

		regex_flags = 0;
		while (*flags) {
			switch (*flags) {
			case 's':
				regex_flags |= G_REGEX_DOTALL;
				break;
			case 'm':
				regex_flags |= G_REGEX_MULTILINE;
				break;
			case 'i':
				regex_flags |= G_REGEX_CASELESS;
				break;
			case 'x':
				regex_flags |= G_REGEX_EXTENDED;
				break;
			default:
				err_str = g_strdup_printf ("Invalid SPARQL regex flag '%c'", *flags);
				result_context_function_error (context, fn, err_str);
				g_free (err_str);
				return;
			}
			flags++;
		}

		regex = g_regex_new (pattern, regex_flags, 0, &error);

		if (error) {
			result_context_function_error (context, fn, error->message);
			g_clear_error (&error);
			return;
		}

		sqlite3_set_auxdata (context, 1, regex, (void (*) (void*)) g_regex_unref);
	}

	if (text != NULL) {
		ret = g_regex_match (regex, text, 0, NULL);
	} else {
		ret = FALSE;
	}

	sqlite3_result_int (context, ret);
}

static void
ensure_replace_checks (TrackerDBInterface *db_interface)
{
	if (db_interface->replace_func_checks.syntax_check != NULL)
		return;

	db_interface->replace_func_checks.syntax_check =
		g_regex_new ("(?<!\\\\)\\$\\D", G_REGEX_OPTIMIZE, 0, NULL);
	db_interface->replace_func_checks.replacement =
		g_regex_new("(?<!\\\\)\\$(\\d)", G_REGEX_OPTIMIZE, 0, NULL);
	db_interface->replace_func_checks.unescape =
		g_regex_new("\\\\\\$", G_REGEX_OPTIMIZE, 0, NULL);
}

static void
function_sparql_replace (sqlite3_context *context,
                         int              argc,
                         sqlite3_value   *argv[])
{
	const gchar *fn = "fn:replace";
	TrackerDBInterface *db_interface = sqlite3_user_data (context);
	TrackerDBReplaceFuncChecks *checks = &db_interface->replace_func_checks;
	const gchar *input, *pattern, *replacement, *flags = "";
	gchar *err_str, *output = NULL, *replaced = NULL, *unescaped = NULL;
	GError *error = NULL;
	GRegexCompileFlags regex_flags = 0;
	GRegex *regex, *replace_regex;
	gint capture_count, i;

	TRACKER_RETURN_IF_FAIL (argc == 3 || argc == 4, fn, "Invalid argument count");

	ensure_replace_checks (db_interface);

	if (argc == 4)
		flags = (gchar *)sqlite3_value_text (argv[3]);

	input = (gchar *)sqlite3_value_text (argv[0]);
	regex = sqlite3_get_auxdata (context, 1);
	replacement = (gchar *)sqlite3_value_text (argv[2]);

	if (regex == NULL) {
		pattern = (gchar *)sqlite3_value_text (argv[1]);

		for (i = 0; flags[i]; i++) {
			switch (flags[i]) {
			case 's':
				regex_flags |= G_REGEX_DOTALL;
				break;
			case 'm':
				regex_flags |= G_REGEX_MULTILINE;
				break;
			case 'i':
				regex_flags |= G_REGEX_CASELESS;
				break;
			case 'x':
				regex_flags |= G_REGEX_EXTENDED;
				break;
			default:
				err_str = g_strdup_printf ("Invalid SPARQL regex flag '%c'", flags[i]);
				result_context_function_error (context, fn, err_str);
				g_free (err_str);
				return;
			}
		}

		regex = g_regex_new (pattern, regex_flags, 0, &error);

		if (error) {
			result_context_function_error (context, fn, error->message);
			g_clear_error (&error);
			return;
		}

		/* According to the XPath 2.0 standard, an error shall be raised, if the given
		 * pattern matches a zero-length string.
		 */
		if (g_regex_match (regex, "", 0, NULL)) {
			err_str = g_strdup_printf ("The given pattern '%s' matches a zero-length string.",
			                           pattern);
			result_context_function_error (context, fn, err_str);
			g_regex_unref (regex);
			g_free (err_str);
			return;
		}

		sqlite3_set_auxdata (context, 1, regex, (GDestroyNotify) g_regex_unref);
	}

	/* According to the XPath 2.0 standard, an error shall be raised, if all dollar
	 * signs ($) of the given replacement string are not immediately followed by
	 * a digit 0-9 or not immediately preceded by a \.
	 */
	if (g_regex_match (checks->syntax_check, replacement, 0, NULL)) {
		err_str = g_strdup_printf ("The replacement string '%s' contains a \"$\" character "
		                           "that is not immediately followed by a digit 0-9 and "
		                           "not immediately preceded by a \"\\\".",
		                           replacement);
		result_context_function_error (context, fn, err_str);
		g_free (err_str);
		return;
	}

	/* According to the XPath 2.0 standard, the dollar sign ($) followed by a number
	 * indicates backreferences. GRegex uses the backslash (\) for this purpose.
	 * So the ($) backreferences in the given replacement string are replaced by (\)
	 * backreferences to support the standard.
	 */
	capture_count = g_regex_get_capture_count (regex);
	replace_regex = sqlite3_get_auxdata (context, 2);

	if (capture_count > 9 && !replace_regex) {
		gint i;
		GString *backref_range;
		gchar *regex_interpret;

		/* S ... capture_count, N ... the given decimal number.
		 * If N>S and N>9, The last digit of N is taken to be a literal character
		 * to be included "as is" in the replacement string, and the rules are
		 * reapplied using the number N formed by stripping off this last digit.
		 */
		backref_range = g_string_new ("(");
		for (i = 10; i <= capture_count; i++) {
			g_string_append_printf (backref_range, "%d|", i);
		}

		g_string_append (backref_range, "\\d)");
		regex_interpret = g_strdup_printf ("(?<!\\\\)\\$%s",
		                                   backref_range->str);

		replace_regex = g_regex_new (regex_interpret, 0, 0, NULL);

		g_string_free (backref_range, TRUE);
		g_free (regex_interpret);

		sqlite3_set_auxdata (context, 2, replace_regex, (GDestroyNotify) g_regex_unref);
	} else if (capture_count <= 9) {
		replace_regex = checks->replacement;
	}

	replaced = g_regex_replace (replace_regex,
	                            replacement, -1, 0, "\\\\g<\\1>", 0, &error);

	if (!error) {
		/* All '\$' pairs are replaced by '$' */
		unescaped = g_regex_replace (checks->unescape,
		                             replaced, -1, 0, "$", 0, &error);
	}

	if (!error) {
		output = g_regex_replace (regex, input, -1, 0, unescaped, 0, &error);
	}

	if (error) {
		result_context_function_error (context, fn, error->message);
		g_clear_error (&error);
		return;
	}

	sqlite3_result_text (context, output, -1, g_free);

	g_free (replaced);
	g_free (unescaped);
}

static void
function_sparql_lower_case (sqlite3_context *context,
                            int              argc,
                            sqlite3_value   *argv[])
{
	const gchar *fn = "fn:lower-case";
	const gunichar2 *zInput;
	gunichar2 *zOutput;
	int nInput;
	gsize nOutput;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	zInput = sqlite3_value_text16 (argv[0]);

	if (zInput) {
		nInput = sqlite3_value_bytes16 (argv[0]);
		zOutput = tracker_parser_tolower (zInput, nInput, &nOutput);
		sqlite3_result_text16 (context, zOutput, -1, free);
	} else {
		sqlite3_result_null (context);
	}
}

static void
function_sparql_upper_case (sqlite3_context *context,
                            int              argc,
                            sqlite3_value   *argv[])
{
	const gchar *fn = "fn:upper-case";
	const gunichar2 *zInput;
	gunichar2 *zOutput;
	int nInput;
	gsize nOutput;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	zInput = sqlite3_value_text16 (argv[0]);

	if (zInput) {
		nInput = sqlite3_value_bytes16 (argv[0]);
		zOutput = tracker_parser_toupper (zInput, nInput, &nOutput);
		sqlite3_result_text16 (context, zOutput, -1, free);
	} else {
		sqlite3_result_null (context);
	}
}

static void
function_sparql_case_fold (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:case-fold";
	const gunichar2 *zInput;
	gunichar2 *zOutput;
	int nInput;
	gsize nOutput;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	zInput = sqlite3_value_text16 (argv[0]);

	if (zInput) {
		nInput = sqlite3_value_bytes16 (argv[0]);
		zOutput = tracker_parser_casefold (zInput, nInput, &nOutput);
		sqlite3_result_text16 (context, zOutput, -1, free);
	} else {
		sqlite3_result_null (context);
	}
}

static void
function_sparql_normalize (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:normalize";
	const gchar *nfstr;
	const gunichar2 *zInput;
	gunichar2 *zOutput = NULL;
	GNormalizeMode mode;
	int nInput;
	gsize nOutput;

	TRACKER_RETURN_IF_FAIL (argc == 2, fn, "Invalid argument count");

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		sqlite3_result_null (context);
		return;
	}

	nInput = sqlite3_value_bytes16 (argv[0]);

	nfstr = (gchar *)sqlite3_value_text (argv[1]);
	if (g_ascii_strcasecmp (nfstr, "nfc") == 0)
		mode = G_NORMALIZE_NFC;
	else if (g_ascii_strcasecmp (nfstr, "nfd") == 0)
		mode = G_NORMALIZE_NFD;
	else if (g_ascii_strcasecmp (nfstr, "nfkc") == 0)
		mode = G_NORMALIZE_NFKC;
	else if (g_ascii_strcasecmp (nfstr, "nfkd") == 0)
		mode = G_NORMALIZE_NFKD;
	else {
		result_context_function_error (context, fn, "Invalid normalization specified");
		return;
	}

	zOutput = tracker_parser_normalize (zInput, mode, nInput, &nOutput);
	sqlite3_result_text16 (context, zOutput, nOutput * sizeof (gunichar2), free);
}

static void
function_sparql_unaccent (sqlite3_context *context,
                          int              argc,
                          sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:unaccent";
	const gunichar2 *zInput;
	gunichar2 *zOutput = NULL;
	int nInput;
	gsize nOutput;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	zInput = sqlite3_value_text16 (argv[0]);

	if (zInput) {
		nInput = sqlite3_value_bytes16 (argv[0]);
		zOutput = tracker_parser_unaccent (zInput, nInput, &nOutput);
		sqlite3_result_text16 (context, zOutput, nOutput * sizeof (gunichar2), free);
	} else {
		sqlite3_result_null (context);
	}
}

static void
function_sparql_strip_punctuation (sqlite3_context *context,
                                   int              argc,
                                   sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:strip-punctuation";
	gchar *input, *replacement = "", *output = NULL;
	GError *error = NULL;
	GRegex *regex;
	input = (gchar *)sqlite3_value_text (argv[0]);
	const gchar *pattern = "\\p{P}";

	regex = g_regex_new (pattern, 0, 0, &error);
	if (error)
	{
		result_context_function_error (context, fn, error->message);
		g_clear_error (&error);
		return;
	}

	output = g_regex_replace (regex, input, -1, 0, replacement, 0, &error);

	sqlite3_result_text (context, output, -1, g_free);

	g_regex_unref (regex);
}

static void
function_sparql_encode_for_uri (sqlite3_context *context,
                                int              argc,
                                sqlite3_value   *argv[])
{
	const gchar *fn = "fn:encode-for-uri";
	const gchar *str;
	gchar *encoded;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	str = (gchar *)sqlite3_value_text (argv[0]);
	encoded = g_uri_escape_string (str, NULL, FALSE);
	sqlite3_result_text (context, encoded, -1, g_free);
}

static void
function_sparql_uri (sqlite3_context *context,
                     int              argc,
                     sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:uri";
	const gchar *str;
	gchar *encoded;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	str = (gchar *)sqlite3_value_text (argv[0]);
	encoded = g_uri_escape_string (str,
	                               G_URI_RESERVED_CHARS_ALLOWED_IN_PATH,
	                               FALSE);
	sqlite3_result_text (context, encoded, -1, g_free);
}

static void
function_sparql_string_before (sqlite3_context *context,
                               int              argc,
                               sqlite3_value   *argv[])
{
	const gchar *fn = "fn:substring-before";
	const gchar *str, *substr, *loc;
	gint len;

	TRACKER_RETURN_IF_FAIL (argc == 2, fn, "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_type (argv[0]) == SQLITE_TEXT &&
	                        sqlite3_value_type (argv[1]) == SQLITE_TEXT,
	                        fn, "Invalid argument types");

	str = (gchar *)sqlite3_value_text (argv[0]);
	substr = (gchar *)sqlite3_value_text (argv[1]);
	len = strlen (substr);

	if (len == 0) {
		sqlite3_result_text (context, "", -1, NULL);
		return;
	}

	loc = strstr (str, substr);

	if (!loc) {
		sqlite3_result_text (context, "", -1, NULL);
		return;
	}

	sqlite3_result_text (context, str, loc - str, NULL);
}

static void
function_sparql_string_after (sqlite3_context *context,
                              int              argc,
                              sqlite3_value   *argv[])
{
	const gchar *fn = "fn:substring-after";
	const gchar *str, *substr, *loc;
	gint len;

	TRACKER_RETURN_IF_FAIL (argc == 2, fn, "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_type (argv[0]) == SQLITE_TEXT &&
	                        sqlite3_value_type (argv[1]) == SQLITE_TEXT,
	                        fn, "Invalid argument types");

	str = (gchar *)sqlite3_value_text (argv[0]);
	substr = (gchar *)sqlite3_value_text (argv[1]);
	len = strlen (substr);

	if (len == 0) {
		sqlite3_result_text (context, g_strdup (str), -1, g_free);
		return;
	}

	loc = strstr (str, substr);

	if (!loc) {
		sqlite3_result_text (context, "", -1, NULL);
		return;
	}

	sqlite3_result_text (context, loc + len, -1, NULL);
}

static void
function_sparql_ceil (sqlite3_context *context,
                      int              argc,
                      sqlite3_value   *argv[])
{
	const gchar *fn = "fn:numeric-ceil";
	gdouble value;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	value = sqlite3_value_double (argv[0]);
	sqlite3_result_double (context, ceil (value));
}

static void
function_sparql_floor (sqlite3_context *context,
                       int              argc,
                       sqlite3_value   *argv[])
{
	const gchar *fn = "fn:numeric-floor";
	gdouble value;

	TRACKER_RETURN_IF_FAIL (argc == 1, fn, "Invalid argument count");

	value = sqlite3_value_double (argv[0]);
	sqlite3_result_double (context, floor (value));
}

static void
function_sparql_data_type (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlDataType helper";
	TrackerPropertyType prop_type;
	const gchar *type = NULL;

	TRACKER_RETURN_IF_FAIL (argc == 1 || argc == 2, fn, "Invalid argument count");

	prop_type = sqlite3_value_int (argv[0]);

	switch (prop_type) {
	case TRACKER_PROPERTY_TYPE_UNKNOWN:
		break;
	case TRACKER_PROPERTY_TYPE_STRING:
	case TRACKER_PROPERTY_TYPE_LANGSTRING:
		if (argc > 1 && sqlite3_value_type (argv[1]) == SQLITE_BLOB)
			type = "http://www.w3.org/1999/02/22-rdf-syntax-ns#langString";
		else
			type = "http://www.w3.org/2001/XMLSchema#string";
		break;
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
		type = "http://www.w3.org/2001/XMLSchema#boolean";
		break;
	case TRACKER_PROPERTY_TYPE_INTEGER:
		type = "http://www.w3.org/2001/XMLSchema#integer";
		break;
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		type = "http://www.w3.org/2001/XMLSchema#double";
		break;
	case TRACKER_PROPERTY_TYPE_DATE:
		type = "http://www.w3.org/2001/XMLSchema#date";
		break;
	case TRACKER_PROPERTY_TYPE_DATETIME:
		type = "http://www.w3.org/2001/XMLSchema#dateType";
		break;
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		type = "http://www.w3.org/2000/01/rdf-schema#Resource";
		break;
	}

	if (type)
		sqlite3_result_text (context, type, -1, NULL);
	else
		sqlite3_result_null (context);
}

static void
function_sparql_rand (sqlite3_context *context,
                      int              argc,
                      sqlite3_value   *argv[])
{
	const gchar *fn = "rand";

	TRACKER_RETURN_IF_FAIL (argc == 0, fn, "Invalid argument count");

	sqlite3_result_double (context, g_random_double ());
}

static void
function_sparql_checksum (sqlite3_context *context,
                          int              argc,
                          sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlCheckSum helper";
	const gchar *str, *checksumstr;
	GChecksumType checksum;
	gchar *result;

	TRACKER_RETURN_IF_FAIL (argc == 2, fn, "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_type (argv[0]) == SQLITE_TEXT &&
	                        sqlite3_value_type (argv[1]) == SQLITE_TEXT,
	                        fn, "Invalid argument types");

	str = (const gchar *) sqlite3_value_text (argv[0]);
	checksumstr = (const gchar *) sqlite3_value_text (argv[1]);

	if (g_ascii_strcasecmp (checksumstr, "md5") == 0)
		checksum = G_CHECKSUM_MD5;
	else if (g_ascii_strcasecmp (checksumstr, "sha1") == 0)
		checksum = G_CHECKSUM_SHA1;
	else if (g_ascii_strcasecmp (checksumstr, "sha256") == 0)
		checksum = G_CHECKSUM_SHA256;
	else if (g_ascii_strcasecmp (checksumstr, "sha384") == 0)
		checksum = G_CHECKSUM_SHA384;
	else if (g_ascii_strcasecmp (checksumstr, "sha512") == 0)
		checksum = G_CHECKSUM_SHA512;
	else {
		result_context_function_error (context, fn, "Invalid checksum method specified");
		return;
	}

	result = g_compute_checksum_for_string (checksum, str, -1);
	sqlite3_result_text (context, result, -1, g_free);
}

static void
function_sparql_langmatches (sqlite3_context *context,
                             int              argc,
                             sqlite3_value   *argv[])
{
	const gchar *fn = "langMatches";
	const gchar *str, *langtag;
	gint type;

	TRACKER_RETURN_IF_FAIL (argc == 2, fn, "Invalid argument count");
	TRACKER_RETURN_IF_FAIL (sqlite3_value_type (argv[0]) == SQLITE_TEXT ||
	                        sqlite3_value_type (argv[0]) == SQLITE_BLOB ||
	                        sqlite3_value_type (argv[0]) == SQLITE_NULL,
	                        fn, "Invalid argument type");

	type = sqlite3_value_type (argv[0]);

	if (type == SQLITE_TEXT) {
		/* text arguments don't contain any language information */
		sqlite3_result_int (context, FALSE);
	} else if (type == SQLITE_BLOB) {
		gsize str_len, langtag_len, len;

		str = sqlite3_value_blob (argv[0]);
		len = sqlite3_value_bytes (argv[0]);
		langtag = sqlite3_value_text (argv[1]);
		str_len = strlen (str) + 1;
		langtag_len = strlen (langtag) + 1;

		if (str_len + langtag_len != len ||
		    g_strcmp0 (&str[str_len], langtag) != 0) {
			sqlite3_result_int (context, FALSE);
		} else {
			sqlite3_result_int (context, TRUE);
		}
	} else {
		sqlite3_result_null (context);
	}
}

static void
function_sparql_strlang (sqlite3_context *context,
                         int              argc,
                         sqlite3_value   *argv[])
{
	const gchar *fn = "strlang";
	const gchar *str, *langtag;
	GBytes *bytes;
	gpointer data;
	gsize len;

	TRACKER_RETURN_IF_FAIL (argc == 2, fn, "Invalid argument count");

	str = sqlite3_value_text (argv[0]);
	langtag = sqlite3_value_text (argv[1]);

	bytes = tracker_sparql_make_langstring (str, langtag);
	data = g_bytes_unref_to_data (bytes, &len);

	sqlite3_result_blob64 (context, data, len, g_free);
}

static inline int
stmt_step (sqlite3_stmt *stmt)
{
	int result;

	result = sqlite3_step (stmt);

	/* If the statement expired between preparing it and executing
	 * sqlite3_step(), we are supposed to get SQLITE_SCHEMA error in
	 * sqlite3_errcode(), BUT there seems to be a bug in sqlite and
	 * SQLITE_ABORT is being returned instead for that case. So, the
	 * only way to see if a given statement was expired is to use
	 * sqlite3_expired(stmt), which is marked as DEPRECATED in sqlite.
	 * If found that the statement is expired, we need to reset it
	 * and retry the sqlite3_step().
	 * NOTE, that this expiration may only happen between preparing
	 * the statement and step-ing it, NOT between steps. */
	if ((result == SQLITE_ABORT || result == SQLITE_SCHEMA) &&
	    sqlite3_expired (stmt)) {
		sqlite3_reset (stmt);
		result = sqlite3_step (stmt);
	}

	return result;
}

static void
stmt_destroy (void *stmt)
{
	sqlite3_finalize ((sqlite3_stmt *) stmt);
}

static void
generate_uuid (sqlite3_context *context,
               const gchar     *fn,
               const gchar     *uri_prefix)
{
	gchar *uuid = NULL;
	sqlite3_stmt *stmt;
	gboolean store_auxdata = FALSE;
	sqlite3 *db;
	gint result;

	stmt = sqlite3_get_auxdata (context, 1);

	if (stmt == NULL) {
		db = sqlite3_context_db_handle (context);

		result = sqlite3_prepare_v2 (db, "SELECT ID FROM Resource WHERE Uri=?",
		                             -1, &stmt, NULL);
		if (result != SQLITE_OK) {
			result_context_function_error (context, fn, sqlite3_errstr (result));
			return;
		}

		store_auxdata = TRUE;
	}

	do {
		g_clear_pointer (&uuid, g_free);
		uuid = tracker_generate_uuid (uri_prefix);

		sqlite3_reset (stmt);
		sqlite3_bind_text (stmt, 1, uuid, -1, SQLITE_TRANSIENT);
		result = stmt_step (stmt);
	} while (result == SQLITE_ROW);

	if (store_auxdata) {
		sqlite3_set_auxdata (context, 1, (void*) stmt, stmt_destroy);
	}

	if (result != SQLITE_DONE) {
		result_context_function_error (context, fn, sqlite3_errstr (result));
		g_free (uuid);
	} else {
		sqlite3_result_text (context, uuid, -1, g_free);
	}
}

static void
function_sparql_uuid (sqlite3_context *context,
                      int              argc,
                      sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlUUID helper";
	const gchar *prefix = NULL;

	TRACKER_RETURN_IF_FAIL (argc <= 1, fn, "Invalid argument count");

	if (argc == 1)
		prefix = sqlite3_value_text (argv[0]);

	generate_uuid (context, fn, prefix);
}

static void
function_sparql_bnode (sqlite3_context *context,
                       int              argc,
                       sqlite3_value   *argv[])
{
	const gchar *fn = "SparlBNODE helper";

	TRACKER_RETURN_IF_FAIL (argc <= 1, fn, "Invalid argument count");

	generate_uuid (context, fn, "urn:bnode");
}

static void
function_sparql_print_value (sqlite3_context *context,
                             int              argc,
                             sqlite3_value   *argv[])
{
	const gchar *fn = "PrintValue helper";
	TrackerPropertyType prop_type;

	TRACKER_RETURN_IF_FAIL (argc <= 2, fn, "Invalid argument count");

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
		return;
	}

	prop_type = sqlite3_value_int64 (argv[1]);

	switch (prop_type) {
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
			/* Raw rowids belong to blank nodes */
			if (sqlite3_value_int64 (argv[0]) == 0) {
				sqlite3_result_null (context);
			} else {
				sqlite3_result_text (context,
				                     g_strdup_printf ("urn:bnode:%" G_GINT64_FORMAT,
				                                      (gint64) sqlite3_value_int64 (argv[0])),
				                     -1, g_free);
			}
		} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
			sqlite3_result_value (context, argv[0]);
		} else {
			result_context_function_error (context, fn, "Invalid value type");
		}
		break;
	case TRACKER_PROPERTY_TYPE_DATE:
	case TRACKER_PROPERTY_TYPE_DATETIME:
		if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
			struct tm tm;
			gint64 timestamp;
			gchar buf[100];
			int retval;

			timestamp = sqlite3_value_int64 (argv[0]);

			if (gmtime_r ((time_t *) &timestamp, &tm) == NULL)
				result_context_function_error (context, fn, "Invalid unix timestamp");

			if (prop_type == TRACKER_PROPERTY_TYPE_DATETIME)
				retval = strftime ((gchar *) &buf, sizeof (buf), STRFTIME_YEAR_MODIFIER "-%m-%dT%TZ", &tm);
			else if (prop_type == TRACKER_PROPERTY_TYPE_DATE)
				retval = strftime ((gchar *) &buf, sizeof (buf), STRFTIME_YEAR_MODIFIER "-%m-%d", &tm);
			else
				g_assert_not_reached ();

			if (retval != 0)
				sqlite3_result_text (context, g_strdup (buf), -1, g_free);
			else
				result_context_function_error (context, fn, "Invalid unix timestamp");
		} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
			if (prop_type == TRACKER_PROPERTY_TYPE_DATETIME) {
				sqlite3_result_value (context, argv[0]);
			} else if (prop_type == TRACKER_PROPERTY_TYPE_DATE) {
				const gchar *value, *end;
				gsize len;

				value = sqlite3_value_text (argv[0]);
				/* Drop time data if we are given a xsd:dateTime as a xsd:date */
				end = strchr (value, 'T');
				if (end)
					len = end - value;
				else
					len = strlen (value);

				sqlite3_result_text (context,
				                     g_strndup (value, len),
				                     -1, g_free);
			} else {
				g_assert_not_reached ();
			}
		} else {
			result_context_function_error (context, fn, "Invalid value type");
		}
		break;
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
		if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER &&
		    (sqlite3_value_int64 (argv[0]) == 0 ||
		     sqlite3_value_int64 (argv[0]) == 1)) {
			sqlite3_result_text (context,
			                     sqlite3_value_int64 (argv[0]) == 1 ?
			                     "true" : "false",
			                     -1, NULL);
		} else {
			sqlite3_result_null (context);
		}
		break;
	case TRACKER_PROPERTY_TYPE_STRING:
	case TRACKER_PROPERTY_TYPE_LANGSTRING:
	case TRACKER_PROPERTY_TYPE_INTEGER:
	case TRACKER_PROPERTY_TYPE_DOUBLE:
	case TRACKER_PROPERTY_TYPE_UNKNOWN:
		sqlite3_result_value (context, argv[0]);
		break;
	default:
		result_context_function_error (context, fn, "Invalid property type");
	}
}

static void
function_sparql_update_value (sqlite3_context *context,
                              int              argc,
                              sqlite3_value   *argv[])
{
	gboolean old_value_is_null, new_value_is_null, values_are_equal;
	TrackerPropertyOp op;
	enum {
		ARG_PROPERTY_NAME,
		ARG_OP,
		ARG_OLD_VALUE,
		ARG_NEW_VALUE,
	};

	op = sqlite3_value_int64 (argv[ARG_OP]);
	old_value_is_null = sqlite3_value_type (argv[ARG_OLD_VALUE]) == SQLITE_NULL;
	new_value_is_null = sqlite3_value_type (argv[ARG_NEW_VALUE]) == SQLITE_NULL;

	if (!old_value_is_null && !new_value_is_null) {
		if (sqlite3_value_type (argv[ARG_OLD_VALUE]) ==
		    sqlite3_value_type (argv[ARG_NEW_VALUE])) {
			values_are_equal =
				g_strcmp0 (sqlite3_value_text (argv[ARG_OLD_VALUE]),
				           sqlite3_value_text (argv[ARG_NEW_VALUE])) == 0;
		} else {
			values_are_equal = FALSE;
		}
	} else {
		values_are_equal = old_value_is_null == new_value_is_null;
	}

	if (op == TRACKER_OP_INSERT) {
		if (old_value_is_null) {
			/* Replace */
			sqlite3_result_value (context, argv[ARG_NEW_VALUE]);
		} else if (values_are_equal) {
			/* Values match, pick either */
			sqlite3_result_value (context, argv[ARG_OLD_VALUE]);
		} else {
			gchar *err_str;

			/* Insert on a single-valued column that already has a value */
			err_str = g_strdup_printf ("Unable to insert multiple values on "
			                           "single valued property (old: %s, new: %s)",
			                           sqlite3_value_text (argv[ARG_OLD_VALUE]),
			                           sqlite3_value_text (argv[ARG_NEW_VALUE]));
			result_context_function_error (context,
			                               sqlite3_value_text (argv[ARG_PROPERTY_NAME]),
			                               err_str);
			g_free (err_str);
		}
	} else if (op == TRACKER_OP_INSERT_FAILABLE) {
		if (old_value_is_null) {
			/* Replace */
			sqlite3_result_value (context, argv[ARG_NEW_VALUE]);
		} else {
			/* Preserve old value, continue without error */
			sqlite3_result_value (context, argv[ARG_OLD_VALUE]);
		}
	} else if (op == TRACKER_OP_DELETE) {
		if (!new_value_is_null && values_are_equal) {
			/* Remove after matching */
			sqlite3_result_null (context);
		} else {
			/* Preserve old value */
			sqlite3_result_value (context, argv[ARG_OLD_VALUE]);
		}
	} else if (op == TRACKER_OP_RESET) {
		/* Just reset without checks */
		sqlite3_result_value (context, argv[ARG_NEW_VALUE]);
	}
}

static void
function_sparql_fts_tokenize (sqlite3_context *context,
                              int              argc,
                              sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlFtsTokenizer helper";
	gchar *text;
	const gchar *p;
	gboolean in_quote = FALSE;
	gboolean in_space = FALSE;
	gboolean started = FALSE;
	int n_output_quotes = 0;
	gunichar ch;
	GString *str;
	int len;
	gchar *result;

	TRACKER_RETURN_IF_FAIL (argc <= 1, fn, "Invalid argument count");

	text = g_strstrip (g_strdup (sqlite3_value_text (argv[0])));
	str = g_string_new (NULL);
	p = text;

	while ((ch = g_utf8_get_char (p)) != 0) {
		if (ch == '\"') {
			n_output_quotes++;
			in_quote = !in_quote;
		} else if ((ch == ' ') != !!in_space) {
			/* Ensure terms get independently quoted, unless
			 * they are within a explicitly quoted part of the text.
			 */
			if (!in_quote && started) {
				g_string_append_c (str, '"');
				n_output_quotes++;
			}

			in_space = ch == ' ';
		} else if (!started) {
			/* Not a quote, nor a space at the first char. Add the starting quote */
			g_string_append_c (str, '"');
			n_output_quotes++;
		}

		g_string_append_unichar (str, ch);
		started = TRUE;
		p = g_utf8_next_char (p);
	}

	if (n_output_quotes % 2 != 0)
		g_string_append_c (str, '"');

	len = str->len;
	result = g_string_free (str, FALSE);
	sqlite3_result_text (context, result, len, g_free);
	g_free (text);
}

static int
check_interrupt (void *user_data)
{
	TrackerDBInterface *db_interface = user_data;

	return g_cancellable_is_cancelled (db_interface->cancellable) ? 1 : 0;
}

static void
initialize_functions (TrackerDBInterface *db_interface)
{
	gsize i;
	struct {
		gchar *name;
		int n_args;
		int mods;
		void (*func) (sqlite3_context *, int, sqlite3_value**);
	} functions[] = {
		/* Geolocation */
		{ "SparqlHaversineDistance", 4, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_haversine_distance },
		{ "SparqlCartesianDistance", 4, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_cartesian_distance },
		/* Date/time */
		{ "SparqlTimestamp", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_timestamp },
		{ "SparqlTimeSort", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_time_sort },
		{ "SparqlTimezoneDuration", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_time_zone_duration },
		{ "SparqlTimezoneString", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_time_zone_substr },
		{ "SparqlTimezone", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_time_zone },
		/* Paths and filenames */
		{ "SparqlStringFromFilename", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_string_from_filename },
		{ "SparqlUriIsParent", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_uri_is_parent },
		{ "SparqlUriIsDescendant", -1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_uri_is_descendant },
		{ "SparqlEncodeForUri", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_encode_for_uri },
		{ "SparqlUri", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_uri },
		/* Strings */
		{ "SparqlRegex", -1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_regex },
		{ "SparqlStringJoin", -1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_string_join },
		{ "SparqlLowerCase", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_lower_case },
		{ "SparqlUpperCase", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_upper_case },
		{ "SparqlCaseFold", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_case_fold },
		{"SparqlStripPunctuation", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		 function_sparql_strip_punctuation },
		{ "SparqlNormalize", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_normalize },
		{ "SparqlUnaccent", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_unaccent },
		{ "SparqlStringBefore", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_string_before },
		{ "SparqlStringAfter", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_string_after },
		{ "SparqlReplace", -1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_replace },
		{ "SparqlChecksum", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_checksum },
		{ "SparqlLangMatches", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_langmatches },
		{ "SparqlStrLang", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_strlang },
		{ "SparqlFtsTokenize", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_fts_tokenize },
		/* Numbers */
		{ "SparqlCeil", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_ceil },
		{ "SparqlFloor", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_floor },
		{ "SparqlRand", 0, SQLITE_ANY, function_sparql_rand },
		/* Types */
		{ "SparqlDataType", -1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_data_type },
		/* UUID */
		{ "SparqlUUID", -1, SQLITE_ANY, function_sparql_uuid },
		{ "SparqlBNODE", -1, SQLITE_ANY | SQLITE_DETERMINISTIC, function_sparql_bnode },
		/* Helpers */
		{ "SparqlPrintValue", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_print_value },
		{ "SparqlUpdateValue", 4, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_update_value },
	};

	for (i = 0; i < G_N_ELEMENTS (functions); i++) {
		sqlite3_create_function (db_interface->db,
		                         functions[i].name, functions[i].n_args,
		                         functions[i].mods, db_interface,
		                         functions[i].func, NULL, NULL);
	}
}

static inline void
tracker_db_interface_lock (TrackerDBInterface *iface)
{
	g_mutex_lock (&iface->mutex);
}

static inline void
tracker_db_interface_unlock (TrackerDBInterface *iface)
{
	g_mutex_unlock (&iface->mutex);
}

static void
open_database (TrackerDBInterface  *db_interface,
               GError             **error)
{
	int mode;
	int result;

	g_assert (db_interface->filename != NULL);

	if ((db_interface->flags & TRACKER_DB_INTERFACE_READONLY) == 0) {
		mode = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	} else {
		mode = SQLITE_OPEN_READONLY;
	}

	if ((db_interface->flags & TRACKER_DB_INTERFACE_IN_MEMORY) != 0)
		mode |= SQLITE_OPEN_MEMORY | SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_URI;

	result = sqlite3_open_v2 (db_interface->filename, &db_interface->db, mode | SQLITE_OPEN_NOMUTEX, NULL);

	if (result != SQLITE_OK) {
		const gchar *str;

		str = sqlite3_errstr (result);
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_OPEN_ERROR,
		             "Could not open sqlite3 database:'%s': %s",
		             db_interface->filename ? db_interface->filename : "memory",
		             str);
		return;
	} else {
		TRACKER_NOTE (SQLITE,
		              g_message ("Opened sqlite3 database:'%s'",
		                         db_interface->filename? db_interface->filename : "memory"));
	}

	/* Set our unicode collation function */
	tracker_db_interface_sqlite_reset_collator (db_interface);

	sqlite3_progress_handler (db_interface->db, 100,
	                          check_interrupt, db_interface);

	initialize_functions (db_interface);

	sqlite3_extended_result_codes (db_interface->db, 0);
	sqlite3_busy_timeout (db_interface->db, 100000);

#ifndef SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION
#warning Using sqlite3_enable_load_extension instead of SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, this is unsafe
	sqlite3_enable_load_extension (db_interface->db, 1);
#else
	sqlite3_db_config (db_interface->db, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, NULL);
#endif
}

static gboolean
tracker_db_interface_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
	TrackerDBInterface *db_iface;
	GError *internal_error = NULL;

	db_iface = TRACKER_DB_INTERFACE (initable);

	open_database (db_iface, &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	return TRUE;
}

static void
tracker_db_interface_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_db_interface_initable_init;
}

static void
tracker_db_interface_sqlite_set_property (GObject       *object,
                                          guint          prop_id,
                                          const GValue  *value,
                                          GParamSpec    *pspec)
{
	TrackerDBInterface *db_iface;

	db_iface = TRACKER_DB_INTERFACE (object);

	switch (prop_id) {
	case PROP_FLAGS:
		db_iface->flags = g_value_get_flags (value);
		break;
	case PROP_FILENAME:
		db_iface->filename = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
close_database (TrackerDBInterface *db_interface)
{
	gint rc;

	tracker_db_statement_mru_finish (&db_interface->select_stmt_mru);

	if (db_interface->replace_func_checks.syntax_check)
		g_regex_unref (db_interface->replace_func_checks.syntax_check);
	if (db_interface->replace_func_checks.replacement)
		g_regex_unref (db_interface->replace_func_checks.replacement);
	if (db_interface->replace_func_checks.unescape)
		g_regex_unref (db_interface->replace_func_checks.unescape);

	if (db_interface->db) {
		rc = sqlite3_close (db_interface->db);
		if (rc != SQLITE_OK &&
		    !(db_interface->flags & TRACKER_DB_INTERFACE_READONLY)) {
			g_warning ("Database closed uncleanly: %s",
			           sqlite3_errstr (rc));
		}
	}
}

gboolean
tracker_db_interface_sqlite_fts_init (TrackerDBInterface     *db_interface,
                                      TrackerDBManagerFlags   fts_flags,
                                      GError                **error)
{
	return tracker_tokenizer_initialize (db_interface->db,
	                                     db_interface,
	                                     fts_flags,
	                                     TRACKER_DATA_MANAGER (db_interface->user_data),
	                                     error);
}

void
tracker_db_interface_sqlite_reset_collator (TrackerDBInterface *db_interface)
{
	TRACKER_NOTE (SQLITE, g_message ("Resetting collator in db interface"));

	/* This will overwrite any other collation set before, if any */
	if (sqlite3_create_collation_v2 (db_interface->db,
	                                 TRACKER_COLLATION_NAME,
	                                 SQLITE_UTF8,
	                                 tracker_collation_init (),
	                                 tracker_collation_utf8,
	                                 tracker_collation_shutdown) != SQLITE_OK)
	{
		g_critical ("Couldn't set collation function: %s",
		            sqlite3_errmsg (db_interface->db));
	}

	if (sqlite3_create_collation_v2 (db_interface->db,
	                                 TRACKER_TITLE_COLLATION_NAME,
	                                 SQLITE_UTF8,
	                                 tracker_collation_init (),
	                                 tracker_collation_utf8_title,
	                                 tracker_collation_shutdown) != SQLITE_OK) {
		g_critical ("Couldn't set title collation function: %s",
		            sqlite3_errmsg (db_interface->db));
	}
}

gboolean
tracker_db_interface_sqlite_wal_checkpoint (TrackerDBInterface  *interface,
                                            gboolean             blocking,
                                            GError             **error)
{
	int return_val;

	TRACKER_NOTE (SQLITE, g_message ("Checkpointing database (%s)...", blocking ? "blocking" : "non-blocking"));

	return_val = sqlite3_wal_checkpoint_v2 (interface->db, NULL,
	                                        blocking ? SQLITE_CHECKPOINT_FULL : SQLITE_CHECKPOINT_PASSIVE,
	                                        NULL, NULL);

	if (return_val != SQLITE_OK) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_QUERY_ERROR,
		             "%s", sqlite3_errstr (return_val));
		return FALSE;
	}

	TRACKER_NOTE (SQLITE, g_message ("Checkpointing complete"));
	return TRUE;
}

static void
tracker_db_interface_sqlite_finalize (GObject *object)
{
	TrackerDBInterface *db_interface;

	db_interface = TRACKER_DB_INTERFACE (object);

	close_database (db_interface);

	TRACKER_NOTE (SQLITE, g_message ("Closed sqlite3 database:'%s'", db_interface->filename));

	g_free (db_interface->filename);

	g_clear_object (&db_interface->user_data);

	G_OBJECT_CLASS (tracker_db_interface_parent_class)->finalize (object);
}

static void
tracker_db_interface_class_init (TrackerDBInterfaceClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->set_property = tracker_db_interface_sqlite_set_property;
	object_class->finalize = tracker_db_interface_sqlite_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_FILENAME,
	                                 g_param_spec_string ("filename",
	                                                      "DB filename",
	                                                      "DB filename",
	                                                      NULL,
	                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_FLAGS,
	                                 g_param_spec_flags ("flags",
	                                                     "Flags",
	                                                     "Interface flags",
	                                                     TRACKER_TYPE_DB_INTERFACE_FLAGS, 0,
	                                                     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
tracker_db_interface_init (TrackerDBInterface *db_interface)
{
	tracker_db_statement_mru_init (&db_interface->select_stmt_mru, 100,
	                               g_str_hash, g_str_equal, NULL);
}

void
tracker_db_interface_set_max_stmt_cache_size (TrackerDBInterface         *db_interface,
                                              TrackerDBStatementCacheType cache_type,
                                              guint                       max_size)
{
	TrackerDBStatementMru *stmt_mru;

	if (cache_type == TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT) {
		stmt_mru = &db_interface->select_stmt_mru;
	} else {
		return;
	}

	/* Must be larger than 2 to make sense (to have a tail and head) */
	if (max_size > 2) {
		stmt_mru->max = max_size;
	} else {
		stmt_mru->max = 3;
	}
}

static sqlite3_stmt *
tracker_db_interface_prepare_stmt (TrackerDBInterface  *db_interface,
                                   const gchar         *full_query,
                                   GError             **error)
{
	sqlite3_stmt *sqlite_stmt;
	int retval;

	retval = sqlite3_prepare_v2 (db_interface->db, full_query, -1, &sqlite_stmt, NULL);

	if (retval != SQLITE_OK) {
		sqlite_stmt = NULL;

		if (retval == SQLITE_INTERRUPT) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_INTERRUPTED,
			             "Interrupted");
		} else {
#ifdef G_ENABLE_DEBUG
			if (TRACKER_DEBUG_CHECK (SQL)) {
				g_message ("Failure to prepare statement for SQL '%s', error: %s",
					   full_query,
					   sqlite3_errmsg (db_interface->db));
			}
#endif

			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_QUERY_ERROR,
			             "%s",
			             sqlite3_errmsg (db_interface->db));
		}
	}

	return sqlite_stmt;
}

void
tracker_db_statement_mru_init (TrackerDBStatementMru *mru,
                               guint                  size,
                               GHashFunc              hash_func,
                               GEqualFunc             equal_func,
                               GDestroyNotify         key_destroy)
{
	mru->head = mru->tail = NULL;
	mru->max = size;
	mru->size = 0;
	mru->stmts = g_hash_table_new_full (hash_func, equal_func,
	                                    key_destroy, g_object_unref);
}

void
tracker_db_statement_mru_finish (TrackerDBStatementMru *stmt_mru)
{
	stmt_mru->head = stmt_mru->tail = NULL;
	stmt_mru->size = stmt_mru->max = 0;
	g_clear_pointer (&stmt_mru->stmts, g_hash_table_unref);
}

void
tracker_db_statement_mru_clear (TrackerDBStatementMru *stmt_mru)
{
	stmt_mru->head = stmt_mru->tail = NULL;
	stmt_mru->size = 0;
	g_hash_table_remove_all (stmt_mru->stmts);
}

TrackerDBStatement *
tracker_db_statement_mru_lookup (TrackerDBStatementMru *stmt_mru,
                                 gconstpointer          key)
{
	return g_hash_table_lookup (stmt_mru->stmts, key);
}

void
tracker_db_statement_mru_insert (TrackerDBStatementMru *stmt_mru,
                                 gpointer               key,
                                 TrackerDBStatement    *stmt)
{
	g_return_if_fail (stmt->mru_key == NULL);

	/* use replace instead of insert to make sure we store the key that
	 * belongs to the right sqlite statement to ensure the lifetime of the key
	 * matches the statement
	 */
	g_hash_table_replace (stmt_mru->stmts, key, g_object_ref_sink (stmt));

	/* So the ring looks a bit like this: *
	 *                                    *
	 *    .--tail  .--head                *
	 *    |        |                      *
	 *  [p-n] -> [p-n] -> [p-n] -> [p-n]  *
	 *    ^                          |    *
	 *    `- [n-p] <- [n-p] <--------'    *
	 *                                    */

	if (stmt_mru->size == 0) {
		stmt_mru->head = stmt;
		stmt_mru->tail = stmt;
	} else if (stmt_mru->size >= stmt_mru->max) {
		TrackerDBStatement *new_head;

		/* We reached max-size of the MRU stmt cache. Destroy current
		 * least recently used (stmt_mru.head) and fix the ring. For
		 * that we take out the current head, and close the ring.
		 * Then we assign head->next as new head.
		 */
		new_head = stmt_mru->head->next;
		stmt_mru->head->prev->next = new_head;
		new_head->prev = stmt_mru->head->prev;
		stmt_mru->head->next = stmt_mru->head->prev = NULL;
		g_hash_table_remove (stmt_mru->stmts, (gpointer) stmt_mru->head->mru_key);
		stmt_mru->size--;
		stmt_mru->head = new_head;
	}

	/* Set the current stmt (which is always new here) as the new tail
	 * (new most recent used). We insert current stmt between head and
	 * current tail, and we set tail to current stmt.
	 */
	stmt_mru->size++;
	stmt->next = stmt_mru->head;
	stmt_mru->head->prev = stmt;

	stmt_mru->tail->next = stmt;
	stmt->prev = stmt_mru->tail;
	stmt_mru->tail = stmt;

	stmt->mru_key = key;
}

void
tracker_db_statement_mru_update (TrackerDBStatementMru *stmt_mru,
                                 TrackerDBStatement    *stmt)
{
	g_return_if_fail (stmt->mru_key != NULL);

	tracker_db_statement_sqlite_reset (stmt);

	if (stmt == stmt_mru->head) {
		/* Current stmt is least recently used, shift head and tail
		 * of the ring to efficiently make it most recently used.
		 */
		stmt_mru->head = stmt_mru->head->next;
		stmt_mru->tail = stmt_mru->tail->next;
	} else if (stmt != stmt_mru->tail) {
		/* Current statement isn't most recently used, make it most
		 * recently used now (less efficient way than above).
		 */

		/* Take stmt out of the list and close the ring */
		stmt->prev->next = stmt->next;
		stmt->next->prev = stmt->prev;

		/* Put stmt as tail (most recent used) */
		stmt->next = stmt_mru->head;
		stmt_mru->head->prev = stmt;
		stmt->prev = stmt_mru->tail;
		stmt_mru->tail->next = stmt;
		stmt_mru->tail = stmt;
	}

	/* if (stmt == tail), it's already the most recently used in the
	 * ring, so in this case we do nothing of course */
}

TrackerDBStatement *
tracker_db_interface_create_statement (TrackerDBInterface           *db_interface,
                                       TrackerDBStatementCacheType   cache_type,
                                       GError                      **error,
                                       const gchar                  *query)
{
	TrackerDBStatement *stmt = NULL;
	TrackerDBStatementMru *mru = NULL;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (db_interface), NULL);

	tracker_db_interface_lock (db_interface);

	/* MRU holds a reference to the stmt */
	if (cache_type == TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT)
		mru = &db_interface->select_stmt_mru;

	if (mru)
		stmt = tracker_db_statement_mru_lookup (mru, query);

	if (stmt && stmt->stmt_is_borrowed) {
		/* prepared statement is owned somewhere else, create new uncached one */
		stmt = NULL;
		mru = NULL;
	}

	if (!stmt) {
		sqlite3_stmt *sqlite_stmt;

		sqlite_stmt = tracker_db_interface_prepare_stmt (db_interface,
		                                                 query,
		                                                 error);
		if (!sqlite_stmt) {
			tracker_db_interface_unlock (db_interface);
			return NULL;
		}

		stmt = tracker_db_statement_sqlite_new (db_interface,
		                                        sqlite_stmt);

		if (mru)
			tracker_db_statement_mru_insert (mru, (gpointer) sqlite3_sql (stmt->stmt), stmt);
	} else if (mru) {
		tracker_db_statement_mru_update (mru, stmt);
	}

	stmt->stmt_is_borrowed = cache_type != TRACKER_DB_STATEMENT_CACHE_TYPE_NONE;

	tracker_db_interface_unlock (db_interface);

	return g_object_ref_sink (stmt);
}

TrackerDBStatement *
tracker_db_interface_create_vstatement (TrackerDBInterface           *db_interface,
                                        TrackerDBStatementCacheType   cache_type,
                                        GError                      **error,
                                        const gchar                  *query,
                                        ...)
{
	TrackerDBStatement *stmt;
	va_list args;
	gchar *full_query;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (db_interface), NULL);

	va_start (args, query);
	full_query = g_strdup_vprintf (query, args);
	va_end (args);

	stmt = tracker_db_interface_create_statement (db_interface, cache_type,
	                                              error, full_query);
	g_free (full_query);

	return stmt;
}

static gboolean
execute_stmt (TrackerDBInterface  *interface,
              sqlite3_stmt        *stmt,
              GCancellable        *cancellable,
              GError             **error)
{
	gint result;

	result = SQLITE_OK;

	tracker_db_interface_ref_use (interface);

#ifdef G_ENABLE_DEBUG
        if (TRACKER_DEBUG_CHECK (SQL)) {
	        gchar *full_query;

	        full_query = sqlite3_expanded_sql (stmt);

	        if (full_query) {
		        g_message ("Executing update: '%s'", full_query);
		        sqlite3_free (full_query);
	        } else {
		        g_message ("Executing update: '%s'",
		                   sqlite3_sql (stmt));
	        }
        }
#endif

	while (result == SQLITE_OK  ||
	       result == SQLITE_ROW ||
	       result == SQLITE_LOCKED) {

		if (g_cancellable_is_cancelled (cancellable)) {
			result = SQLITE_INTERRUPT;
			sqlite3_reset (stmt);
		} else {
			/* only one statement can be active at the same time per interface */
			interface->cancellable = cancellable;
			result = stmt_step (stmt);

			interface->cancellable = NULL;
		}

		switch (result) {
		case SQLITE_ERROR:
			sqlite3_reset (stmt);
			break;
		case SQLITE_ROW:
			break;
		case SQLITE_LOCKED:
			/* In-memory databases use a shared cache where readers
			 * can temporarily block the writer thread, keep stepping
			 * in this case until the database is no longer blocked.
			 */
			if (sqlite3_extended_errcode (interface->db) == SQLITE_LOCKED_SHAREDCACHE)
				continue;
			else
				break;
		default:
			break;
		}
	}

	tracker_db_interface_unref_use (interface);

	if (result != SQLITE_DONE) {
		if (result == SQLITE_INTERRUPT) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_INTERRUPTED,
			             "Interrupted");
		} else if (result == SQLITE_CONSTRAINT) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_CONSTRAINT,
			             "Constraint would be broken: %s",
			             sqlite3_errmsg (interface->db));
		} else if (result == SQLITE_FULL) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_NO_SPACE,
			             "No space to write database");
		} else {
			int db_result;

			db_result = sqlite3_errcode (interface->db);

			if (db_result == SQLITE_NOTADB) {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_OPEN_ERROR,
				             "Not a database: %s",
				             sqlite3_errmsg (interface->db));
			} else if (db_result == SQLITE_CORRUPT) {
				interface->corrupted = TRUE;
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_CORRUPT,
				             "Database corrupt: %s",
				             sqlite3_errmsg (interface->db));
			} else if (db_result == SQLITE_IOERR) {
				int db_errno;

				db_errno = sqlite3_system_errno (interface->db);

				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_QUERY_ERROR,
				             "I/O error (errno: %s)",
				             g_strerror (db_errno));
			} else {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_QUERY_ERROR,
				             "%s",
				             sqlite3_errmsg (interface->db));
			}
		}
	}

	return result == SQLITE_DONE;
}

gboolean
tracker_db_interface_execute_vquery (TrackerDBInterface  *db_interface,
                                     GError             **error,
                                     const gchar         *query,
                                     va_list              args)
{
	gchar *full_query;
	sqlite3_stmt *stmt;
	gboolean retval = FALSE;

	tracker_db_interface_lock (db_interface);

	full_query = g_strdup_vprintf (query, args);
	stmt = tracker_db_interface_prepare_stmt (db_interface,
	                                          full_query,
	                                          error);
	g_free (full_query);
	if (stmt) {
		retval = execute_stmt (db_interface, stmt, NULL, error);
		sqlite3_finalize (stmt);
	}

	tracker_db_interface_unlock (db_interface);

	return retval;
}

TrackerDBInterface *
tracker_db_interface_sqlite_new (const gchar              *filename,
                                 TrackerDBInterfaceFlags   flags,
                                 GError                  **error)
{
	TrackerDBInterface *object;
	GError *internal_error = NULL;

	object = g_initable_new (TRACKER_TYPE_DB_INTERFACE,
	                         NULL,
	                         &internal_error,
	                         "filename", filename,
	                         "flags", flags,
	                         NULL);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	return object;
}

gint64
tracker_db_interface_sqlite_get_last_insert_id (TrackerDBInterface *interface)
{
	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (interface), 0);

	return (gint64) sqlite3_last_insert_rowid (interface->db);
}

static void
tracker_db_statement_finalize (GObject *object)
{
	TrackerDBStatement *stmt;

	stmt = TRACKER_DB_STATEMENT (object);

	/* A cursor was still open while we're being finalized, because a cursor
	 * holds its own reference, this means that somebody is unreffing a stmt
	 * too often. We mustn't sqlite3_finalize the priv->stmt in this case,
	 * though. It would crash&burn the cursor. */

	g_assert (!stmt->stmt_is_used);

	sqlite3_finalize (stmt->stmt);

	G_OBJECT_CLASS (tracker_db_statement_parent_class)->finalize (object);
}

static void
tracker_db_statement_class_init (TrackerDBStatementClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_db_statement_finalize;
}

static TrackerDBStatement *
tracker_db_statement_sqlite_new (TrackerDBInterface *db_interface,
                                 sqlite3_stmt       *sqlite_stmt)
{
	TrackerDBStatement *stmt;

	stmt = g_object_new (TRACKER_TYPE_DB_STATEMENT, NULL);

	stmt->db_interface = db_interface;
	stmt->stmt = sqlite_stmt;
	stmt->stmt_is_used = FALSE;

	return stmt;
}

static TrackerDBStatement *
tracker_db_statement_sqlite_grab (TrackerDBStatement *stmt)
{
	g_assert (!stmt->stmt_is_used);
	stmt->stmt_is_used = TRUE;
	return stmt;
}

static void
tracker_db_statement_sqlite_release (TrackerDBStatement *stmt)
{
	stmt->stmt_is_borrowed = FALSE;

	tracker_db_statement_sqlite_reset (stmt);

	stmt->stmt_is_used = FALSE;
}

static void
tracker_db_cursor_close (TrackerSparqlCursor *sparql_cursor)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;

	g_return_if_fail (TRACKER_IS_DB_CURSOR (cursor));

	if (cursor->ref_stmt == NULL) {
		/* already closed */
		return;
	}

	iface = cursor->ref_stmt->db_interface;

	g_object_ref (iface);

	tracker_db_interface_lock (iface);
	tracker_db_statement_sqlite_release (cursor->ref_stmt);
	g_clear_object (&cursor->ref_stmt);
	tracker_db_interface_unlock (iface);

	tracker_db_interface_unref_use (iface);

	g_object_unref (iface);
}

static void
tracker_db_cursor_finalize (GObject *object)
{
	TrackerSparqlCursor *cursor;

	cursor = TRACKER_SPARQL_CURSOR (object);

	tracker_db_cursor_close (cursor);

	G_OBJECT_CLASS (tracker_db_cursor_parent_class)->finalize (object);
}

static void
tracker_db_cursor_iter_next_thread (GTask        *task,
                                    gpointer      object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
	/* run in thread */

	TrackerDBCursor *cursor = object;
	GError *error = NULL;
	gboolean result;

	result = db_cursor_iter_next (cursor, cancellable, &error);
	if (error) {
		g_task_return_error (task, error);
	} else {
		g_task_return_boolean (task, result);
	}
}

static void
tracker_db_cursor_iter_next_async (TrackerSparqlCursor     *cursor,
                                   GCancellable            *cancellable,
                                   GAsyncReadyCallback      callback,
                                   gpointer                 user_data)
{
	GTask *task;

	task = g_task_new (G_OBJECT (cursor), cancellable, callback, user_data);
	g_task_run_in_thread (task, tracker_db_cursor_iter_next_thread);
	g_object_unref (task);
}

static gboolean
tracker_db_cursor_iter_next_finish (TrackerSparqlCursor  *cursor,
                                    GAsyncResult         *res,
                                    GError              **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_db_cursor_class_init (TrackerDBCursorClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	TrackerSparqlCursorClass *sparql_cursor_class = TRACKER_SPARQL_CURSOR_CLASS (class);

	object_class->finalize = tracker_db_cursor_finalize;

	sparql_cursor_class->get_value_type = tracker_db_cursor_get_value_type;
	sparql_cursor_class->get_variable_name = tracker_db_cursor_get_variable_name;
	sparql_cursor_class->get_n_columns = tracker_db_cursor_get_n_columns;
	sparql_cursor_class->get_string = tracker_db_cursor_get_string;
	sparql_cursor_class->next = tracker_db_cursor_iter_next;
	sparql_cursor_class->next_async = tracker_db_cursor_iter_next_async;
	sparql_cursor_class->next_finish = tracker_db_cursor_iter_next_finish;
	sparql_cursor_class->rewind = tracker_db_cursor_rewind;
	sparql_cursor_class->close = tracker_db_cursor_close;

	sparql_cursor_class->get_integer = tracker_db_cursor_get_int;
	sparql_cursor_class->get_double = tracker_db_cursor_get_double;
	sparql_cursor_class->get_boolean = tracker_db_cursor_get_boolean;
}

static TrackerDBCursor *
tracker_db_cursor_sqlite_new (TrackerDBStatement  *ref_stmt,
                              guint                n_columns)
{
	TrackerDBCursor *cursor;
	TrackerDBInterface *iface;

	iface = ref_stmt->db_interface;
	tracker_db_interface_ref_use (iface);

#ifdef G_ENABLE_DEBUG
        if (TRACKER_DEBUG_CHECK (SQL)) {
	        gchar *full_query;

	        full_query = sqlite3_expanded_sql (ref_stmt->stmt);

	        if (full_query) {
		        g_message ("Executing query: '%s'", full_query);
		        sqlite3_free (full_query);
	        } else {
		        g_message ("Executing query: '%s'",
		                   sqlite3_sql (ref_stmt->stmt));
	        }
        }
#endif

	cursor = g_object_new (TRACKER_TYPE_DB_CURSOR, NULL);

	cursor->finished = FALSE;
	cursor->n_columns = n_columns;

	cursor->stmt = ref_stmt->stmt;
	cursor->ref_stmt = g_object_ref (ref_stmt);
	tracker_db_statement_sqlite_grab (cursor->ref_stmt);

	return cursor;
}

void
tracker_db_statement_bind_double (TrackerDBStatement *stmt,
                                  int                 index,
                                  double              value)
{
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_used);

	tracker_db_interface_lock (stmt->db_interface);
	sqlite3_bind_double (stmt->stmt, index + 1, value);
	tracker_db_interface_unlock (stmt->db_interface);
}

void
tracker_db_statement_bind_int (TrackerDBStatement *stmt,
                               int                 index,
                               gint64              value)
{
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_used);

	tracker_db_interface_lock (stmt->db_interface);
	sqlite3_bind_int64 (stmt->stmt, index + 1, value);
	tracker_db_interface_unlock (stmt->db_interface);
}

void
tracker_db_statement_bind_null (TrackerDBStatement *stmt,
                                int                 index)
{
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_used);

	tracker_db_interface_lock (stmt->db_interface);
	sqlite3_bind_null (stmt->stmt, index + 1);
	tracker_db_interface_unlock (stmt->db_interface);
}

void
tracker_db_statement_bind_text (TrackerDBStatement *stmt,
                                int                 index,
                                const gchar        *value)
{
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_used);

	tracker_db_interface_lock (stmt->db_interface);
	sqlite3_bind_text (stmt->stmt, index + 1, value, -1, SQLITE_TRANSIENT);
	tracker_db_interface_unlock (stmt->db_interface);
}

void
tracker_db_statement_bind_bytes (TrackerDBStatement         *stmt,
                                 int                         index,
                                 GBytes                     *value)
{
	gconstpointer data;
	gsize len;

	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_used);

	data = g_bytes_get_data (value, &len);

	tracker_db_interface_lock (stmt->db_interface);
	sqlite3_bind_blob (stmt->stmt, index + 1, data, len, SQLITE_TRANSIENT);
	tracker_db_interface_unlock (stmt->db_interface);
}

void
tracker_db_statement_bind_value (TrackerDBStatement *stmt,
                                 int                 index,
                                 const GValue       *value)
{
	GType type;

	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_used);

	tracker_db_interface_lock (stmt->db_interface);

	type = G_VALUE_TYPE (value);

	if (type == G_TYPE_INT) {
		sqlite3_bind_int64 (stmt->stmt, index + 1, g_value_get_int (value));
	} else if (type == G_TYPE_INT64) {
		sqlite3_bind_int64 (stmt->stmt, index + 1, g_value_get_int64 (value));
	} else if (type == G_TYPE_DOUBLE) {
		sqlite3_bind_double (stmt->stmt, index + 1, g_value_get_double (value));
	} else if (type == G_TYPE_FLOAT) {
		sqlite3_bind_double (stmt->stmt, index + 1, g_value_get_float (value));
	} else if (type == G_TYPE_STRING) {
		sqlite3_bind_text (stmt->stmt, index + 1,
				   g_value_get_string (value), -1, SQLITE_TRANSIENT);
	} else if (type == G_TYPE_BOOLEAN) {
		sqlite3_bind_text (stmt->stmt, index + 1,
		                   g_value_get_boolean (value) ? "true" : "false",
		                   -1, 0);
	} else if (type == G_TYPE_BYTES) {
		GBytes *bytes;
		gconstpointer data;
		gsize len;

		bytes = g_value_get_boxed (value);
		data = g_bytes_get_data (bytes, &len);
		sqlite3_bind_blob (stmt->stmt, index + 1,
		                   data, len, SQLITE_TRANSIENT);
	} else if (type == G_TYPE_DATE_TIME) {
		GDateTime *datetime;
		gchar *str;

		datetime = g_value_get_boxed (value);
		str = tracker_date_format_iso8601 (datetime);
		sqlite3_bind_text (stmt->stmt, index + 1,
		                   str, -1, g_free);
	} else {
		GValue dest = G_VALUE_INIT;

		g_value_init (&dest, G_TYPE_STRING);

		if (g_value_transform (value, &dest)) {
			sqlite3_bind_text (stmt->stmt, index + 1,
					   g_value_get_string (&dest), -1, SQLITE_TRANSIENT);
			g_value_unset (&dest);
		} else {
			g_assert_not_reached ();
		}
	}

	tracker_db_interface_unlock (stmt->db_interface);
}

void
tracker_db_cursor_rewind (TrackerSparqlCursor *sparql_cursor)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;

	g_return_if_fail (TRACKER_IS_DB_CURSOR (cursor));

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	sqlite3_reset (cursor->stmt);
	cursor->finished = FALSE;

	tracker_db_interface_unlock (iface);
}

gboolean
tracker_db_cursor_iter_next (TrackerSparqlCursor  *cursor,
                             GCancellable         *cancellable,
                             GError              **error)
{
	return db_cursor_iter_next (TRACKER_DB_CURSOR (cursor), cancellable, error);
}


static gboolean
db_cursor_iter_next (TrackerDBCursor *cursor,
                     GCancellable    *cancellable,
                     GError         **error)
{
	TrackerDBStatement *stmt = cursor->ref_stmt;
	TrackerDBInterface *iface = stmt->db_interface;
	gboolean finished;

	tracker_db_interface_lock (iface);

	if (!cursor->finished) {
		guint result;

		if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
			sqlite3_reset (cursor->stmt);
			cursor->finished = TRUE;
		} else {
			/* only one statement can be active at the same time per interface */
			iface->cancellable = cancellable;
			result = stmt_step (cursor->stmt);
			iface->cancellable = NULL;

			if (result == SQLITE_INTERRUPT) {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_INTERRUPTED,
				             "Interrupted");
			} else if (result != SQLITE_ROW && result != SQLITE_DONE) {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_QUERY_ERROR,
				             "%s", sqlite3_errmsg (iface->db));
			}

			cursor->finished = (result != SQLITE_ROW);
		}
	}

	finished = cursor->finished;

	tracker_db_interface_unlock (iface);

	return !finished;
}

gint
tracker_db_cursor_get_n_columns (TrackerSparqlCursor *sparql_cursor)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;
	guint n_columns;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	if (cursor->n_columns == 0)
		n_columns = sqlite3_column_count (cursor->stmt);
	else
		n_columns = cursor->n_columns;

	tracker_db_interface_unlock (iface);

	return n_columns;
}

void
tracker_db_cursor_get_value (TrackerDBCursor *cursor,
                             guint            column,
                             GValue          *value)
{
	gint col_type;

	col_type = sqlite3_column_type (cursor->stmt, column);

	switch (col_type) {
	case SQLITE_TEXT:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, (gchar *) sqlite3_column_text (cursor->stmt, column));
		break;
	case SQLITE_INTEGER:
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, sqlite3_column_int64 (cursor->stmt, column));
		break;
	case SQLITE_FLOAT:
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, sqlite3_column_double (cursor->stmt, column));
		break;
	case SQLITE_NULL:
		/* just ignore NULLs */
		break;
	default:
		g_critical ("Unknown sqlite3 database column type:%d", col_type);
	}

}

gint64
tracker_db_cursor_get_int (TrackerSparqlCursor *sparql_cursor,
                           gint                 column)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;
	gint64 result;

	if (cursor->n_columns > 0 && column >= (gint) cursor->n_columns)
		return 0;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	result = (gint64) sqlite3_column_int64 (cursor->stmt, column);

	tracker_db_interface_unlock (iface);

	return result;
}

gdouble
tracker_db_cursor_get_double (TrackerSparqlCursor *sparql_cursor,
                              gint                 column)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;
	gdouble result;

	if (cursor->n_columns > 0 && column >= (gint) cursor->n_columns)
		return 0;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	result = (gdouble) sqlite3_column_double (cursor->stmt, column);

	tracker_db_interface_unlock (iface);

	return result;
}

static gboolean
tracker_db_cursor_get_boolean (TrackerSparqlCursor *sparql_cursor,
                               gint                 column)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;
	gboolean result;
	gint col_type;

	if (cursor->n_columns > 0 && column >= (gint) cursor->n_columns)
		return FALSE;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	col_type = sqlite3_column_type (cursor->stmt, column);

	if (col_type == SQLITE_INTEGER)
		result = sqlite3_column_int64 (cursor->stmt, column) != 0;
	else if (col_type == SQLITE_TEXT)
		result = g_strcmp0 ((const gchar*) sqlite3_column_text (cursor->stmt, column), "true") == 0;
	else
		result = FALSE;

	tracker_db_interface_unlock (iface);

	return result;
}

static gboolean
tracker_db_cursor_get_annotated_value_type (TrackerDBCursor        *cursor,
                                            guint                   column,
                                            int                     column_type,
                                            TrackerSparqlValueType *value_type)
{
	TrackerPropertyType property_type;
	gboolean is_null;

	if (cursor->n_columns == 0)
		return FALSE;

	/* The value type may be annotated in extra columns, one per
	 * user-visible column.
	 */
	property_type = sqlite3_column_int64 (cursor->stmt, column + cursor->n_columns);
	is_null = column_type == SQLITE_NULL;

	if (is_null) {
		*value_type = TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
		return TRUE;
	}

	switch (property_type) {
	case TRACKER_PROPERTY_TYPE_UNKNOWN:
		return FALSE;
	case TRACKER_PROPERTY_TYPE_STRING:
	case TRACKER_PROPERTY_TYPE_LANGSTRING:
		*value_type = TRACKER_SPARQL_VALUE_TYPE_STRING;
		return TRUE;
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
		*value_type = TRACKER_SPARQL_VALUE_TYPE_BOOLEAN;
		return TRUE;
	case TRACKER_PROPERTY_TYPE_INTEGER:
		*value_type = TRACKER_SPARQL_VALUE_TYPE_INTEGER;
		return TRUE;
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		*value_type = TRACKER_SPARQL_VALUE_TYPE_DOUBLE;
		return TRUE;
	case TRACKER_PROPERTY_TYPE_DATE:
	case TRACKER_PROPERTY_TYPE_DATETIME:
		*value_type = TRACKER_SPARQL_VALUE_TYPE_DATETIME;
		return TRUE;
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		if (g_str_has_prefix ((const gchar *) sqlite3_column_text (cursor->stmt, column),
		                      "urn:bnode:"))
			*value_type = TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		else
			*value_type = TRACKER_SPARQL_VALUE_TYPE_URI;

		return TRUE;
	};

	g_assert_not_reached ();
}

TrackerSparqlValueType
tracker_db_cursor_get_value_type (TrackerSparqlCursor *sparql_cursor,
                                  gint                 column)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;
	gint column_type;
	TrackerSparqlValueType value_type = TRACKER_SPARQL_VALUE_TYPE_UNBOUND;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	if (cursor->n_columns > 0 &&
	    column >= (int) cursor->n_columns)
		goto out;

	column_type = sqlite3_column_type (cursor->stmt, column);

	if (!tracker_db_cursor_get_annotated_value_type (cursor, column, column_type, &value_type)) {
		if (column_type == SQLITE_NULL) {
			value_type = TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
		}

		switch (column_type) {
		case SQLITE_INTEGER:
			value_type = TRACKER_SPARQL_VALUE_TYPE_INTEGER;
			break;
		case SQLITE_FLOAT:
			value_type = TRACKER_SPARQL_VALUE_TYPE_DOUBLE;
			break;
		default:
			value_type = TRACKER_SPARQL_VALUE_TYPE_STRING;
			break;
		}
	}

 out:
	tracker_db_interface_unlock (iface);

	return value_type;
}

const gchar*
tracker_db_cursor_get_variable_name (TrackerSparqlCursor *sparql_cursor,
                                     gint                 column)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;
	const gchar *result;

	if (cursor->n_columns > 0 && column >= (gint) cursor->n_columns)
		return NULL;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);
	result = sqlite3_column_name (cursor->stmt, column);
	tracker_db_interface_unlock (iface);

	if (!result)
		return NULL;

	/* Weed out our own internal variable prefixes */
	if (g_str_has_prefix (result, "v_"))
		return &result[2];

	return result;
}

const gchar*
tracker_db_cursor_get_string (TrackerSparqlCursor  *sparql_cursor,
                              gint                  column,
                              const gchar         **langtag,
                              glong                *length)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (sparql_cursor);
	TrackerDBInterface *iface;
	const gchar *result = NULL;
	sqlite3_value *val;
	int type;

	if (langtag)
		*langtag = NULL;
	if (length)
		*length = 0;

	if (cursor->n_columns > 0 && column >= (gint) cursor->n_columns)
		return NULL;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	val = sqlite3_column_value (cursor->stmt, column);
	type = sqlite3_value_type (val);

	if (type == SQLITE_BLOB) {
		gsize str_len, len;

		result = (const gchar *) sqlite3_value_blob (val);

		if (langtag || length) {
			str_len = strlen (result);

			if (length)
				*length = str_len;

			if (langtag) {
				len = sqlite3_value_bytes (val);
				if (str_len < len)
					*langtag = &result[str_len + 1];
			}
		}
	} else {
		/* Columns without language information */
		if (length) {
			*length = sqlite3_value_bytes (val);
			result = (const gchar *) sqlite3_value_text (val);
		} else {
			result = (const gchar *) sqlite3_column_text (cursor->stmt, column);
		}
	}

	tracker_db_interface_unlock (iface);

	return result;
}

gboolean
tracker_db_statement_execute (TrackerDBStatement  *stmt,
                              GError             **error)
{
	gboolean retval;

	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), FALSE);
	g_return_val_if_fail (!stmt->stmt_is_used, FALSE);

	retval = execute_stmt (stmt->db_interface, stmt->stmt, NULL, error);
	tracker_db_statement_sqlite_release (stmt);

	return retval;
}

gboolean
tracker_db_statement_next_integer (TrackerDBStatement  *stmt,
                                   gboolean            *first,
                                   gint64              *value,
                                   GError             **error)
{
	int result;

	if (*first) {
		tracker_db_interface_lock (stmt->db_interface);
		tracker_db_interface_ref_use (stmt->db_interface);
		tracker_db_statement_sqlite_grab (stmt);

#ifdef G_ENABLE_DEBUG
		if (TRACKER_DEBUG_CHECK (SQL)) {
			gchar *full_query;

			full_query = sqlite3_expanded_sql (stmt->stmt);

			if (full_query) {
				g_message ("Executing query: '%s'", full_query);
				sqlite3_free (full_query);
			} else {
				g_message ("Executing query: '%s'",
				           sqlite3_sql (stmt->stmt));
			}
		}
#endif
	}

	result = stmt_step (stmt->stmt);

	if (result == SQLITE_DONE) {
		goto end;
	} else if (result != SQLITE_ROW) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_QUERY_ERROR,
		             "%s", sqlite3_errmsg (stmt->db_interface->db));
		goto end;
	}

	*first = FALSE;
	if (value)
		*value = sqlite3_column_int (stmt->stmt, 0);

	return TRUE;

 end:
	tracker_db_statement_sqlite_release (stmt);
	tracker_db_interface_unref_use (stmt->db_interface);
	tracker_db_interface_unlock (stmt->db_interface);

	return FALSE;
}

gboolean
tracker_db_statement_next_string (TrackerDBStatement  *stmt,
                                  gboolean            *first,
                                  const char         **value,
                                  GError             **error)
{
	int result;

	if (*first) {
		tracker_db_interface_lock (stmt->db_interface);
		tracker_db_interface_ref_use (stmt->db_interface);
		tracker_db_statement_sqlite_grab (stmt);

#ifdef G_ENABLE_DEBUG
		if (TRACKER_DEBUG_CHECK (SQL)) {
			gchar *full_query;

			full_query = sqlite3_expanded_sql (stmt->stmt);

			if (full_query) {
				g_message ("Executing query: '%s'", full_query);
				sqlite3_free (full_query);
			} else {
				g_message ("Executing query: '%s'",
				           sqlite3_sql (stmt->stmt));
			}
		}
#endif
	}

	result = stmt_step (stmt->stmt);

	if (result == SQLITE_DONE) {
		goto end;
	} else if (result != SQLITE_ROW) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_QUERY_ERROR,
		             "%s", sqlite3_errmsg (stmt->db_interface->db));
		goto end;
	}

	*first = FALSE;
	if (value)
		*value = (const char*) sqlite3_column_text (stmt->stmt, 0);

	return TRUE;

 end:
	tracker_db_statement_sqlite_release (stmt);
	tracker_db_interface_unref_use (stmt->db_interface);
	tracker_db_interface_unlock (stmt->db_interface);

	return FALSE;
}

TrackerDBCursor *
tracker_db_statement_start_cursor (TrackerDBStatement  *stmt,
                                   GError             **error)
{
	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), NULL);
	g_return_val_if_fail (!stmt->stmt_is_used, NULL);

	return tracker_db_cursor_sqlite_new (stmt, 0);
}

TrackerDBCursor *
tracker_db_statement_start_sparql_cursor (TrackerDBStatement  *stmt,
                                          guint                n_columns,
                                          GError             **error)
{
	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), NULL);
	g_return_val_if_fail (!stmt->stmt_is_used, NULL);

	return tracker_db_cursor_sqlite_new (stmt, n_columns);
}

static void
tracker_db_statement_init (TrackerDBStatement *stmt)
{
}

static void
tracker_db_cursor_init (TrackerDBCursor *cursor)
{
}

static void
tracker_db_statement_sqlite_reset (TrackerDBStatement *stmt)
{
	sqlite3_reset (stmt->stmt);
	sqlite3_clear_bindings (stmt->stmt);
}


void
tracker_db_interface_set_user_data (TrackerDBInterface *db_interface,
                                    GObject            *user_data)
{
	g_set_object (&db_interface->user_data, user_data);
}

void
tracker_db_interface_ref_use (TrackerDBInterface *db_interface)
{
	g_atomic_int_inc (&db_interface->n_users);
}

gboolean
tracker_db_interface_unref_use (TrackerDBInterface *db_interface)
{
	return g_atomic_int_dec_and_test (&db_interface->n_users);
}

gboolean
tracker_db_interface_get_is_used (TrackerDBInterface *db_interface)
{
	return g_atomic_int_get (&db_interface->n_users) > 0;
}

void
tracker_db_interface_init_vtabs (TrackerDBInterface *db_interface)
{
	tracker_vtab_triples_init (db_interface->db, (gpointer) db_interface->user_data);
	tracker_vtab_service_init (db_interface->db, (gpointer) db_interface->user_data);
}

gboolean
tracker_db_interface_attach_database (TrackerDBInterface  *db_interface,
                                      GFile               *file,
                                      const gchar         *name,
                                      GError             **error)
{
	gchar *sql, *uri = NULL;
	sqlite3_stmt *stmt;
	gboolean retval;

	g_return_val_if_fail (file, FALSE);

	uri = g_file_get_path (file);
	sql = g_strdup_printf ("ATTACH DATABASE \"%s\" AS \"%s\"",
	                       uri, name);
	g_free (uri);

	stmt = tracker_db_interface_prepare_stmt (db_interface, sql, error);
	g_free (sql);
	if (!stmt)
		return FALSE;

	retval = execute_stmt (db_interface, stmt, NULL, error);
	sqlite3_finalize (stmt);
	return retval;
}

gssize
tracker_db_interface_sqlite_release_memory (TrackerDBInterface *db_interface)
{
	tracker_db_statement_mru_clear (&db_interface->select_stmt_mru);

	return (gssize) sqlite3_db_release_memory (db_interface->db);
}

gboolean
tracker_db_interface_found_corruption (TrackerDBInterface *db_interface)
{
	return db_interface->corrupted;
}
