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

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-debug.h>
#include <libtracker-common/tracker-locale.h>
#include <libtracker-common/tracker-parser.h>

#include <libtracker-sparql/tracker-cursor.h>
#include <libtracker-sparql/tracker-private.h>

#include "tracker-fts.h"


#ifdef HAVE_LIBUNISTRING
/* libunistring versions prior to 9.1.2 need this hack */
#define _UNUSED_PARAMETER_
#include <unistr.h>
#include <unicase.h>
#elif defined(HAVE_LIBICU)
#include <unicode/utypes.h>
#include <unicode/uregex.h>
#include <unicode/ustring.h>
#include <unicode/ucol.h>
#include <unicode/unorm2.h>
#endif

#include "tracker-collation.h"

#include "tracker-db-interface-sqlite.h"
#include "tracker-db-manager.h"
#include "tracker-data-enum-types.h"
#include "tracker-uuid.h"
#include "tracker-vtab-service.h"
#include "tracker-vtab-triples.h"

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
	gchar *shared_cache_key;
	sqlite3 *db;

	/* Compiled regular expressions */
	TrackerDBReplaceFuncChecks replace_func_checks;

	/* Number of users (e.g. active cursors) */
	gint n_users;

	guint flags;
	GCancellable *cancellable;

	TrackerDBStatementMru select_stmt_mru;
	TrackerDBStatementMru update_stmt_mru;

	/* Used if TRACKER_DB_INTERFACE_USE_MUTEX is set */
	GMutex mutex;

	/* User data */
	gpointer user_data;
	GDestroyNotify user_data_destroy_notify;
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
                                                                     guint                  column);
static gboolean            db_cursor_iter_next                      (TrackerDBCursor       *cursor,
                                                                     GCancellable          *cancellable,
                                                                     GError               **error);

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_FLAGS,
	PROP_SHARED_CACHE_KEY,
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

void
tracker_db_interface_sqlite_enable_shared_cache (void)
{
	sqlite3_enable_shared_cache (1);
}

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

static void
function_sparql_string_join (sqlite3_context *context,
                             int              argc,
                             sqlite3_value   *argv[])
{
	const gchar *fn = "fn:string-join";
	GString *str = NULL;
	const gchar *separator;
	gint i;

	/* fn:string-join (str1, str2, ..., separator) */

	if (sqlite3_value_type (argv[argc-1]) != SQLITE_TEXT) {
		result_context_function_error (context, fn, "Invalid separator");
		return;
	}

	separator = (gchar *)sqlite3_value_text (argv[argc-1]);

	for (i = 0;i < argc-1; i++) {
		if (sqlite3_value_type (argv[argc-1]) == SQLITE_TEXT) {
			const gchar *text = (gchar *)sqlite3_value_text (argv[i]);

			if (text != NULL) {
				if (!str) {
					str = g_string_new (text);
				} else {
					g_string_append_printf (str, "%s%s", separator, text);
				}
			}
		}
	}

	if (str) {
		sqlite3_result_text (context, str->str, str->len, g_free);
		g_string_free (str, FALSE);
	} else {
		sqlite3_result_null (context);
	}

	return;
}

/* Create a title-type string from the filename for replacing missing ones */
static void
function_sparql_string_from_filename (sqlite3_context *context,
                                      int              argc,
                                      sqlite3_value   *argv[])
{
	const gchar *fn = "fn:string-from-filename";
	gchar  *name = NULL;
	gchar  *suffix = NULL;

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	/* "/home/user/path/title_of_the_movie.movie" -> "title of the movie"
	 * Only for local files currently, do we need to change? */

	name = g_filename_display_basename ((gchar *)sqlite3_value_text (argv[0]));

	if (!name) {
		sqlite3_result_null (context);
		return;
	}

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
	guint parent_len;

	if (argc != 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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
	while (parent[parent_len - 1] == '/') {
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
                         guint        parent_len,
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
	while (parent[parent_len - 1] == '/') {
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

	if (argc < 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	for (i = 0; i < argc; i++) {
		if (sqlite3_value_type (argv[i]) == SQLITE_NULL) {
			sqlite3_result_int (context, FALSE);
			return;
		} else if (sqlite3_value_type (argv[i]) != SQLITE_TEXT) {
			result_context_function_error (context, fn, "Invalid non-text argument");
			return;
		}
	}

	child = (gchar *)sqlite3_value_text (argv[argc-1]);

	for (i = 0; i < argc - 1 && !match; i++) {
		if (sqlite3_value_type (argv[i]) == SQLITE_TEXT) {
			const gchar *parent = (gchar *)sqlite3_value_text (argv[i]);
			guint parent_len = sqlite3_value_bytes (argv[i]);

			if (!parent)
				continue;

			match = check_uri_is_descendant (parent, parent_len, child);
		}
	}

	sqlite3_result_int (context, match);
}

static void
function_sparql_format_time (sqlite3_context *context,
                             int              argc,
                             sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlFormatTime helper";

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
		return;
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		GDateTime *datetime;
		gint64 timestamp;

		timestamp = sqlite3_value_int64 (argv[0]);
		datetime = g_date_time_new_from_unix_utc (timestamp);

		if (datetime) {
			sqlite3_result_text (context,
					     tracker_date_format_iso8601 (datetime),
					     -1, g_free);
			g_date_time_unref (datetime);
		} else {
			sqlite3_result_null (context);
		}
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		const gchar *str;

		str = sqlite3_value_text (argv[0]);
		sqlite3_result_text (context, g_strdup (str), -1, g_free);
	} else {
		result_context_function_error (context, fn, "Invalid argument type");
	}
}

static void
function_sparql_timestamp (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlTimestamp helper";

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
		return;
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		gdouble seconds;

		seconds = sqlite3_value_double (argv[0]);
		sqlite3_result_double (context, seconds);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		GError *error = NULL;
		GDateTime *datetime;
		const gchar *str;

		str = sqlite3_value_text (argv[0]);
		datetime = tracker_date_new_from_iso8601 (str, &error);
		if (error) {
			result_context_function_error (context, fn, "Failed time string conversion");
			g_error_free (error);
			return;
		}

		sqlite3_result_int64 (context,
				      g_date_time_to_unix (datetime) +
				      (g_date_time_get_utc_offset (datetime) / G_USEC_PER_SEC));
		g_date_time_unref (datetime);
	} else {
		result_context_function_error (context, fn, "Invalid argument type");
	}
}

static void
function_sparql_time_sort (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlTimeSort helper";
	gint64 sort_key;

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
		return;
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER ||
	           sqlite3_value_numeric_type (argv[0]) == SQLITE_FLOAT) {
		gdouble value;

		value = sqlite3_value_double (argv[0]);
		sort_key = (gint64) (value * G_USEC_PER_SEC);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		GDateTime *datetime;
		const gchar *value;
		GError *error = NULL;

		value = sqlite3_value_text (argv[0]);
		datetime = tracker_date_new_from_iso8601 (value, &error);
		if (error) {
			result_context_function_error (context, fn, error->message);
			g_error_free (error);
			return;
		}

		sort_key = ((g_date_time_to_unix (datetime) * G_USEC_PER_SEC) +
			    g_date_time_get_microsecond (datetime));
		g_date_time_unref (datetime);
	} else {
		result_context_function_error (context, fn, "Invalid argument type");
		return;
	}

	sqlite3_result_int64 (context, sort_key);
}

static void
function_sparql_time_zone_duration (sqlite3_context *context,
                                    int              argc,
                                    sqlite3_value   *argv[])
{
	const gchar *fn = "timezone-from-dateTime";
	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
		return;
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		sqlite3_result_int (context, 0);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		GError *error = NULL;
		GDateTime *datetime;
		const gchar *str;

		str = sqlite3_value_text (argv[0]);
		datetime = tracker_date_new_from_iso8601 (str, &error);
		if (error) {
			result_context_function_error (context, fn, "Invalid date");
			g_error_free (error);
			return;
		}

		sqlite3_result_int64 (context,
				      g_date_time_get_utc_offset (datetime) /
				      G_USEC_PER_SEC);
		g_date_time_unref (datetime);
	} else {
		result_context_function_error (context, fn, "Invalid argument type");
	}
}

static void
function_sparql_time_zone_substr (sqlite3_context *context,
                                  int              argc,
                                  sqlite3_value   *argv[])
{
	if (argc != 1) {
		sqlite3_result_error (context, "Invalid argument count converting timezone to string", -1);
		return;
	}

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
		return;
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
	} else {
		sqlite3_result_error (context, "Invalid argument type converting timezone to string", -1);
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

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	if (sqlite3_value_type (argv[0]) == SQLITE_NULL) {
		sqlite3_result_null (context);
		return;
	} else if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		sqlite3_result_text (context, "PT0S", -1, NULL);
	} else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT) {
		GError *error = NULL;
		GDateTime *datetime;
		const gchar *str;
		gchar *duration;

		str = sqlite3_value_text (argv[0]);
		datetime = tracker_date_new_from_iso8601 (str, &error);
		if (error) {
			result_context_function_error (context, fn, "Invalid date");
			g_error_free (error);
			return;
		}

		duration = offset_to_duration (g_date_time_get_utc_offset (datetime) /
					       G_USEC_PER_SEC);
		sqlite3_result_text (context, g_strdup (duration), -1, g_free);
		g_date_time_unref (datetime);
	} else {
		result_context_function_error (context, fn, "Invalid argument type");
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

	if (argc != 4) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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

	if (argc != 4) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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

	if (argc != 2 && argc != 3) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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
	gboolean store_regex = FALSE, store_replace_regex = FALSE;
	const gchar *input, *pattern, *replacement, *flags;
	gchar *err_str, *output = NULL, *replaced = NULL, *unescaped = NULL;
	GError *error = NULL;
	GRegexCompileFlags regex_flags = 0;
	GRegex *regex, *replace_regex;
	gint capture_count, i;

	ensure_replace_checks (db_interface);

	if (argc == 3) {
		flags = "";
	} else if (argc == 4) {
		flags = (gchar *)sqlite3_value_text (argv[3]);
	} else {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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

		store_regex = TRUE;
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

		store_replace_regex = TRUE;
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

	if (store_replace_regex)
		sqlite3_set_auxdata (context, 2, replace_regex, (GDestroyNotify) g_regex_unref);
	if (store_regex)
		sqlite3_set_auxdata (context, 1, regex, (GDestroyNotify) g_regex_unref);

	g_free (replaced);
	g_free (unescaped);
}

#ifdef HAVE_LIBUNISTRING

static void
function_sparql_lower_case (sqlite3_context *context,
                            int              argc,
                            sqlite3_value   *argv[])
{
	const uint16_t *zInput;
	uint16_t *zOutput;
	size_t written = 0;
	int nInput;

	g_assert (argc == 1);

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	nInput = sqlite3_value_bytes16 (argv[0]);

	zOutput = u16_tolower (zInput, nInput/2, NULL, NULL, NULL, &written);

	sqlite3_result_text16 (context, zOutput, written * 2, free);
}

static void
function_sparql_upper_case (sqlite3_context *context,
                            int              argc,
                            sqlite3_value   *argv[])
{
	const uint16_t *zInput;
	uint16_t *zOutput;
	size_t written = 0;
	int nInput;

	g_assert (argc == 1);

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	nInput = sqlite3_value_bytes16 (argv[0]);

	zOutput = u16_toupper (zInput, nInput / 2, NULL, NULL, NULL, &written);

	sqlite3_result_text16 (context, zOutput, written * 2, free);
}

static void
function_sparql_case_fold (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const uint16_t *zInput;
	uint16_t *zOutput;
	size_t written = 0;
	int nInput;

	g_assert (argc == 1);

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	nInput = sqlite3_value_bytes16 (argv[0]);

	zOutput = u16_casefold (zInput, nInput/2, NULL, NULL, NULL, &written);

	sqlite3_result_text16 (context, zOutput, written * 2, free);
}

static void
function_sparql_normalize (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:normalize";
	const gchar *nfstr;
	const uint16_t *zInput;
	uint16_t *zOutput;
	size_t written = 0;
	int nInput;
	uninorm_t nf;

	if (argc != 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	nfstr = sqlite3_value_text (argv[1]);
	if (g_ascii_strcasecmp (nfstr, "nfc") == 0)
		nf = UNINORM_NFC;
	else if (g_ascii_strcasecmp (nfstr, "nfd") == 0)
		nf = UNINORM_NFD;
	else if (g_ascii_strcasecmp (nfstr, "nfkc") == 0)
		nf = UNINORM_NFKC;
	else if (g_ascii_strcasecmp (nfstr, "nfkd") == 0)
		nf = UNINORM_NFKD;
	else {
		result_context_function_error (context, fn, "Invalid normalization specified, options are 'nfc', 'nfd', 'nfkc' or 'nfkd'");
		return;
	}

	nInput = sqlite3_value_bytes16 (argv[0]);

	zOutput = u16_normalize (nf, zInput, nInput/2, NULL, &written);

	sqlite3_result_text16 (context, zOutput, written * 2, free);
}

static void
function_sparql_unaccent (sqlite3_context *context,
                          int              argc,
                          sqlite3_value   *argv[])
{
	const gchar *zInput;
	gchar *zOutput;
	gsize written = 0;
	int nInput;

	g_assert (argc == 1);

	zInput = sqlite3_value_text (argv[0]);

	if (!zInput) {
		return;
	}

	nInput = sqlite3_value_bytes (argv[0]);

	zOutput = u8_normalize (UNINORM_NFKD, zInput, nInput, NULL, &written);

	/* Unaccenting is done in place */
	tracker_parser_unaccent_nfkd_string (zOutput, &written);

	sqlite3_result_text (context, zOutput, written, free);
}

#elif defined(HAVE_LIBICU)

static void
function_sparql_lower_case (sqlite3_context *context,
                            int              argc,
                            sqlite3_value   *argv[])
{
	const gchar *fn = "fn:lower-case";
	const UChar *zInput;
	UChar *zOutput;
	int nInput;
	int nOutput;
	UErrorCode status = U_ZERO_ERROR;

	g_assert (argc == 1);

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	nInput = sqlite3_value_bytes16 (argv[0]);

	nOutput = nInput * 2 + 2;
	zOutput = sqlite3_malloc (nOutput);

	if (!zOutput) {
		return;
	}

	u_strToLower (zOutput, nOutput/2, zInput, nInput/2, NULL, &status);

	if (!U_SUCCESS (status)){
		char zBuf[128];
		sqlite3_snprintf (128, zBuf, "ICU error: u_strToLower(): %s", u_errorName (status));
		zBuf[127] = '\0';
		sqlite3_free (zOutput);
		result_context_function_error (context, fn, zBuf);
		return;
	}

	sqlite3_result_text16 (context, zOutput, -1, sqlite3_free);
}

static void
function_sparql_upper_case (sqlite3_context *context,
                            int              argc,
                            sqlite3_value   *argv[])
{
	const gchar *fn = "fn:upper-case";
	const UChar *zInput;
	UChar *zOutput;
	int nInput;
	int nOutput;
	UErrorCode status = U_ZERO_ERROR;

	g_assert (argc == 1);

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	nInput = sqlite3_value_bytes16 (argv[0]);

	nOutput = nInput * 2 + 2;
	zOutput = sqlite3_malloc (nOutput);

	if (!zOutput) {
		return;
	}

	u_strToUpper (zOutput, nOutput / 2, zInput, nInput / 2, NULL, &status);

	if (!U_SUCCESS (status)){
		char zBuf[128];
		sqlite3_snprintf (128, zBuf, "ICU error: u_strToUpper(): %s", u_errorName (status));
		zBuf[127] = '\0';
		sqlite3_free (zOutput);
		result_context_function_error (context, fn, zBuf);
		return;
	}

	sqlite3_result_text16 (context, zOutput, -1, sqlite3_free);
}

static void
function_sparql_case_fold (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:case-fold";
	const UChar *zInput;
	UChar *zOutput;
	int nInput;
	int nOutput;
	UErrorCode status = U_ZERO_ERROR;

	g_assert (argc == 1);

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	nInput = sqlite3_value_bytes16 (argv[0]);

	nOutput = nInput * 2 + 2;
	zOutput = sqlite3_malloc (nOutput);

	if (!zOutput) {
		return;
	}

	u_strFoldCase (zOutput, nOutput/2, zInput, nInput/2, U_FOLD_CASE_DEFAULT, &status);

	if (!U_SUCCESS (status)){
		char zBuf[128];
		sqlite3_snprintf (128, zBuf, "ICU error: u_strFoldCase: %s", u_errorName (status));
		zBuf[127] = '\0';
		sqlite3_free (zOutput);
		result_context_function_error (context, fn, zBuf);
		return;
	}

	sqlite3_result_text16 (context, zOutput, -1, sqlite3_free);
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
}

static gunichar2 *
normalize_string (const gunichar2    *string,
                  gsize               string_len, /* In gunichar2s */
                  const UNormalizer2 *normalizer,
                  gsize              *len_out,    /* In gunichar2s */
                  UErrorCode         *status)
{
	int nOutput;
	gunichar2 *zOutput;

	nOutput = (string_len * 2) + 1;
	zOutput = g_new0 (gunichar2, nOutput);

	nOutput = unorm2_normalize (normalizer, string, string_len, zOutput, nOutput, status);

	if (*status == U_BUFFER_OVERFLOW_ERROR) {
		/* Try again after allocating enough space for the normalization */
		*status = U_ZERO_ERROR;
		zOutput = g_renew (gunichar2, zOutput, nOutput);
		memset (zOutput, 0, nOutput * sizeof (gunichar2));
		nOutput = unorm2_normalize (normalizer, string, string_len, zOutput, nOutput, status);
	}

	if (!U_SUCCESS (*status)) {
		g_clear_pointer (&zOutput, g_free);
		nOutput = 0;
	}

	if (len_out)
		*len_out = nOutput;

	return zOutput;
}

static void
function_sparql_normalize (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:normalize";
	const gchar *nfstr;
	const uint16_t *zInput;
	uint16_t *zOutput = NULL;
	int nInput;
	gsize nOutput;
	const UNormalizer2 *normalizer;
	UErrorCode status = U_ZERO_ERROR;

	if (argc != 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	nfstr = (gchar *)sqlite3_value_text (argv[1]);
	if (g_ascii_strcasecmp (nfstr, "nfc") == 0)
		normalizer = unorm2_getNFCInstance (&status);
	else if (g_ascii_strcasecmp (nfstr, "nfd") == 0)
		normalizer = unorm2_getNFDInstance (&status);
	else if (g_ascii_strcasecmp (nfstr, "nfkc") == 0)
		normalizer = unorm2_getNFKCInstance (&status);
	else if (g_ascii_strcasecmp (nfstr, "nfkd") == 0)
		normalizer = unorm2_getNFKDInstance (&status);
	else {
		result_context_function_error (context, fn, "Invalid normalization specified");
		return;
	}

	if (U_SUCCESS (status)) {
		nInput = sqlite3_value_bytes16 (argv[0]);
		zOutput = normalize_string (zInput, nInput / 2, normalizer, &nOutput, &status);
	}

	if (!U_SUCCESS (status)) {
		char zBuf[128];
		sqlite3_snprintf (128, zBuf, "ICU error: unorm_normalize: %s", u_errorName (status));
		zBuf[127] = '\0';
		g_free (zOutput);
		result_context_function_error (context, fn, zBuf);
		return;
	}

	sqlite3_result_text16 (context, zOutput, nOutput * sizeof (gunichar2), g_free);
}

static void
function_sparql_unaccent (sqlite3_context *context,
                          int              argc,
                          sqlite3_value   *argv[])
{
	const gchar *fn = "tracker:unaccent";
	const uint16_t *zInput;
	uint16_t *zOutput = NULL;
	int nInput;
	gsize nOutput;
	const UNormalizer2 *normalizer;
	UErrorCode status = U_ZERO_ERROR;

	g_assert (argc == 1);

	zInput = sqlite3_value_text16 (argv[0]);

	if (!zInput) {
		return;
	}

	normalizer = unorm2_getNFKDInstance (&status);

	if (U_SUCCESS (status)) {
		nInput = sqlite3_value_bytes16 (argv[0]);
		zOutput = normalize_string (zInput, nInput / 2, normalizer, &nOutput, &status);
	}

	if (!U_SUCCESS (status)) {
		char zBuf[128];
		sqlite3_snprintf (128, zBuf, "ICU error: unorm_normalize: %s", u_errorName (status));
		zBuf[127] = '\0';
		g_free (zOutput);
		result_context_function_error (context, fn, zBuf);
		return;
	}

	/* Unaccenting is done in place */
	tracker_parser_unaccent_nfkd_string (zOutput, &nOutput);

	sqlite3_result_text16 (context, zOutput, nOutput * sizeof (gunichar2), g_free);
}

#endif

static void
function_sparql_encode_for_uri (sqlite3_context *context,
                                int              argc,
                                sqlite3_value   *argv[])
{
	const gchar *fn = "fn:encode-for-uri";
	const gchar *str;
	gchar *encoded;

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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

	if (argc != 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	if (sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
	    sqlite3_value_type (argv[1]) != SQLITE_TEXT) {
		result_context_function_error (context, fn, "Invalid argument types");
		return;
	}

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

	if (argc != 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	if (sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
	    sqlite3_value_type (argv[1]) != SQLITE_TEXT) {
		result_context_function_error (context, fn, "Invalid argument types");
		return;
	}

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

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	value = sqlite3_value_double (argv[0]);
	sqlite3_result_double (context, floor (value));
}

static void
function_sparql_data_type (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "SparqlDateType helper";
	TrackerPropertyType prop_type;
	const gchar *type = NULL;

	if (argc != 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	prop_type = sqlite3_value_int (argv[0]);

	switch (prop_type) {
	case TRACKER_PROPERTY_TYPE_UNKNOWN:
		break;
	case TRACKER_PROPERTY_TYPE_STRING:
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
	case TRACKER_PROPERTY_TYPE_LANGSTRING:
		type = "http://www.w3.org/1999/02/22-rdf-syntax-ns#langString";
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

	if (argc != 0) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

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

	if (argc != 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	str = (gchar *)sqlite3_value_text (argv[0]);
	checksumstr = (gchar *)sqlite3_value_text (argv[1]);

	if (!str || !checksumstr) {
		result_context_function_error (context, fn, "Invalid arguments");
		return;
	}

	if (g_ascii_strcasecmp (checksumstr, "md5") == 0)
		checksum = G_CHECKSUM_MD5;
	else if (g_ascii_strcasecmp (checksumstr, "sha1") == 0)
		checksum = G_CHECKSUM_SHA1;
	else if (g_ascii_strcasecmp (checksumstr, "sha256") == 0)
		checksum = G_CHECKSUM_SHA256;
#if GLIB_CHECK_VERSION (2, 51, 0)
	else if (g_ascii_strcasecmp (checksumstr, "sha384") == 0)
		checksum = G_CHECKSUM_SHA384;
#endif
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

	if (argc != 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	type = sqlite3_value_type (argv[0]);

	if (type == SQLITE_TEXT) {
		/* text arguments don't contain any language information */
		sqlite3_result_int (context, FALSE);
	} else if (type == SQLITE_BLOB) {
		gsize str_len, len;

		str = sqlite3_value_blob (argv[0]);
		len = sqlite3_value_bytes (argv[0]);
		langtag = sqlite3_value_text (argv[1]);
		str_len = strlen (str) + 1;

		if (str_len + strlen (langtag) != len ||
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
	GString *langstr;

	if (argc != 2) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	str = sqlite3_value_text (argv[0]);
	langtag = sqlite3_value_text (argv[1]);

	langstr = g_string_new (str);
	g_string_append_c (langstr, '\0');
	g_string_append (langstr, langtag);

	sqlite3_result_blob64 (context, langstr->str,
	                       langstr->len, g_free);
	g_string_free (langstr, FALSE);
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
	const gchar *prefix;

	if (argc > 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	prefix = sqlite3_value_text (argv[0]);
	generate_uuid (context, fn, prefix);
}

static void
function_sparql_bnode (sqlite3_context *context,
                       int              argc,
                       sqlite3_value   *argv[])
{
	const gchar *fn = "SparlBNODE helper";

	if (argc > 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	generate_uuid (context, fn, "urn:bnode");
}

static void
function_sparql_print_iri (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	const gchar *fn = "PrintIRI helper";

	if (argc > 1) {
		result_context_function_error (context, fn, "Invalid argument count");
		return;
	}

	if (sqlite3_value_numeric_type (argv[0]) == SQLITE_INTEGER) {
		sqlite3_stmt *stmt;
		gboolean store_auxdata = FALSE;
		sqlite3 *db;
		gint result;

		stmt = sqlite3_get_auxdata (context, 1);

		if (stmt == NULL) {
			store_auxdata = TRUE;
			db = sqlite3_context_db_handle (context);

			result = sqlite3_prepare_v2 (db, "SELECT Uri FROM Resource WHERE ID = ?",
			                             -1, &stmt, NULL);
			if (result != SQLITE_OK) {
				result_context_function_error (context, fn, sqlite3_errstr (result));
				return;
			}
		}

		sqlite3_reset (stmt);
		sqlite3_bind_value (stmt, 1, argv[0]);
		result = stmt_step (stmt);

		if (result == SQLITE_DONE) {
			sqlite3_result_null (context);
		} else if (result == SQLITE_ROW) {
			const gchar *value;

			value = (const gchar *) sqlite3_column_text (stmt, 0);

			if (value && *value) {
				sqlite3_result_text (context, g_strdup (value), -1, g_free);
			} else {
				sqlite3_result_text (context,
				                     g_strdup_printf ("urn:bnode:%" G_GINT64_FORMAT,
				                                      (gint64) sqlite3_value_int64 (argv[0])),
				                     -1, g_free);
			}
		} else {
			result_context_function_error (context, fn, sqlite3_errstr (result));
		}

		if (store_auxdata) {
			sqlite3_set_auxdata (context, 1, (void*) stmt, stmt_destroy);
		}
	} else {
		sqlite3_result_value (context, argv[0]);
	}
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
		{ "SparqlFormatTime", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_format_time },
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
		{ "SparqlPrintIRI", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_print_iri },
		/* Numbers */
		{ "SparqlCeil", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_ceil },
		{ "SparqlFloor", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_floor },
		{ "SparqlRand", 0, SQLITE_ANY, function_sparql_rand },
		/* Types */
		{ "SparqlDataType", 1, SQLITE_ANY | SQLITE_DETERMINISTIC,
		  function_sparql_data_type },
		/* UUID */
		{ "SparqlUUID", 1, SQLITE_ANY, function_sparql_uuid },
		{ "SparqlBNODE", -1, SQLITE_ANY | SQLITE_DETERMINISTIC, function_sparql_bnode },
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
	if (iface->flags & TRACKER_DB_INTERFACE_USE_MUTEX)
		g_mutex_lock (&iface->mutex);
}

static inline void
tracker_db_interface_unlock (TrackerDBInterface *iface)
{
	if (iface->flags & TRACKER_DB_INTERFACE_USE_MUTEX)
		g_mutex_unlock (&iface->mutex);
}

static void
open_database (TrackerDBInterface  *db_interface,
               GError             **error)
{
	int mode;
	int result;
	gchar *uri;

	g_assert (db_interface->filename != NULL || db_interface->shared_cache_key != NULL);

	if ((db_interface->flags & TRACKER_DB_INTERFACE_READONLY) == 0) {
		mode = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	} else {
		mode = SQLITE_OPEN_READONLY;
	}

	if ((db_interface->flags & TRACKER_DB_INTERFACE_IN_MEMORY) != 0) {
		mode |= SQLITE_OPEN_MEMORY | SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_URI;
		uri = g_strdup_printf ("file:%s", db_interface->shared_cache_key);
	} else {
		uri = g_strdup (db_interface->filename);
	}

	result = sqlite3_open_v2 (uri, &db_interface->db, mode | SQLITE_OPEN_NOMUTEX, NULL);
	g_free (uri);

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
	case PROP_SHARED_CACHE_KEY:
		db_iface->shared_cache_key = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_db_interface_sqlite_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
	TrackerDBInterface *db_iface;

	db_iface = TRACKER_DB_INTERFACE (object);

	switch (prop_id) {
	case PROP_FLAGS:
		g_value_set_flags (value, db_iface->flags);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, db_iface->filename);
		break;
	case PROP_SHARED_CACHE_KEY:
		g_value_set_string (value, db_iface->shared_cache_key);
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
	tracker_db_statement_mru_finish (&db_interface->update_stmt_mru);

	if (db_interface->replace_func_checks.syntax_check)
		g_regex_unref (db_interface->replace_func_checks.syntax_check);
	if (db_interface->replace_func_checks.replacement)
		g_regex_unref (db_interface->replace_func_checks.replacement);
	if (db_interface->replace_func_checks.unescape)
		g_regex_unref (db_interface->replace_func_checks.unescape);

	if (db_interface->db) {
		rc = sqlite3_close (db_interface->db);
		if (rc != SQLITE_OK) {
			g_warning ("Database closed uncleanly: %s",
				   sqlite3_errstr (rc));
		}
	}
}

gboolean
tracker_db_interface_sqlite_fts_init (TrackerDBInterface  *db_interface,
                                      const gchar         *database,
                                      GHashTable          *properties,
                                      GHashTable          *multivalued,
                                      gboolean             create,
                                      GError             **error)
{
	GError *inner_error = NULL;

	if (!tracker_fts_init_db (db_interface->db, db_interface,
	                          db_interface->flags, properties, error))
		return FALSE;

	if (create &&
	    !tracker_fts_create_table (db_interface->db, database, "fts5",
	                               properties, multivalued,
	                               &inner_error)) {
		g_propagate_prefixed_error (error,
		                            inner_error,
		                            "FTS tables creation failed: ");
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_db_interface_sqlite_fts_delete_table (TrackerDBInterface  *db_interface,
                                              const gchar         *database,
                                              GError             **error)
{
	return tracker_fts_delete_table (db_interface->db, database, "fts5", error);
}

gboolean
tracker_db_interface_sqlite_fts_alter_table (TrackerDBInterface  *db_interface,
                                             const gchar         *database,
                                             GHashTable          *properties,
                                             GHashTable          *multivalued,
                                             GError             **error)
{
	return tracker_fts_alter_table (db_interface->db, database, "fts5",
	                                properties, multivalued, error);
}

static gchar *
tracker_db_interface_sqlite_fts_create_update_query (TrackerDBInterface  *db_interface,
                                                     const gchar         *database,
                                                     const gchar        **properties)
{
        GString *props_str;
        gchar *query;
        gint i;

        props_str = g_string_new (NULL);

        for (i = 0; properties[i] != NULL; i++) {
		if (i != 0)
			g_string_append_c (props_str, ',');

                g_string_append_printf (props_str, "\"%s\"", properties[i]);
        }

        query = g_strdup_printf ("INSERT INTO \"%s\".fts5 (ROWID, %s) "
                                 "SELECT ROWID, %s FROM \"%s\".fts_view WHERE ROWID = ? AND COALESCE(%s, NULL) IS NOT NULL",
                                 database,
                                 props_str->str,
                                 props_str->str,
                                 database,
                                 props_str->str);
        g_string_free (props_str, TRUE);

        return query;
}

gboolean
tracker_db_interface_sqlite_fts_update_text (TrackerDBInterface  *db_interface,
                                             const gchar         *database,
                                             TrackerRowid         id,
                                             const gchar        **properties,
                                             GError             **error)
{
	TrackerDBStatement *stmt;
	GError *inner_error = NULL;
	gchar *query;

	query = tracker_db_interface_sqlite_fts_create_update_query (db_interface,
	                                                             database,
	                                                             properties);
	stmt = tracker_db_interface_create_statement (db_interface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
	                                              error,
	                                              query);
	g_free (query);

        if (!stmt)
                return FALSE;

        tracker_db_statement_bind_int (stmt, 0, id);
        tracker_db_statement_execute (stmt, &inner_error);
        g_object_unref (stmt);

        if (inner_error) {
	        g_propagate_prefixed_error (error, inner_error, "Could not insert FTS text: ");
                return FALSE;
        }

        return TRUE;
}

static gchar *
tracker_db_interface_sqlite_fts_create_delete_query (TrackerDBInterface  *db_interface,
                                                     const gchar         *database,
                                                     const gchar        **properties)
{
        GString *props_str;
        gchar *query;
        gint i;

        props_str = g_string_new (NULL);

        for (i = 0; properties[i] != NULL; i++) {
		if (i != 0)
			g_string_append_c (props_str, ',');

                g_string_append_printf (props_str, "\"%s\"", properties[i]);
        }

        query = g_strdup_printf ("INSERT INTO \"%s\".fts5 (fts5, ROWID, %s) "
                                 "SELECT 'delete', ROWID, %s FROM \"%s\".fts_view WHERE ROWID = ? AND COALESCE(%s, NULL) IS NOT NULL",
                                 database,
                                 props_str->str,
                                 props_str->str,
                                 database,
                                 props_str->str);
        g_string_free (props_str, TRUE);

        return query;
}

gboolean
tracker_db_interface_sqlite_fts_delete_text (TrackerDBInterface  *db_interface,
                                             const gchar         *database,
                                             TrackerRowid         rowid,
                                             const gchar        **properties,
                                             GError             **error)
{
	TrackerDBStatement *stmt;
	GError *inner_error = NULL;
	gchar *query;

	query = tracker_db_interface_sqlite_fts_create_delete_query (db_interface,
	                                                             database,
	                                                             properties);
	stmt = tracker_db_interface_create_statement (db_interface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
	                                              error,
	                                              query);
	g_free (query);

	if (!stmt)
		return FALSE;

	tracker_db_statement_bind_int (stmt, 0, rowid);
	tracker_db_statement_execute (stmt, &inner_error);
	g_object_unref (stmt);

	if (inner_error) {
	        g_propagate_prefixed_error (error, inner_error, "Could not delete FTS text: ");
                return FALSE;
	}

	return TRUE;
}

gboolean
tracker_db_interface_sqlite_fts_rebuild_tokens (TrackerDBInterface  *interface,
                                                const gchar         *database,
                                                GError             **error)
{
	return tracker_fts_rebuild_tokens (interface->db, database, "fts5", error);
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
	g_free (db_interface->shared_cache_key);

	if (db_interface->user_data && db_interface->user_data_destroy_notify)
		db_interface->user_data_destroy_notify (db_interface->user_data);

	G_OBJECT_CLASS (tracker_db_interface_parent_class)->finalize (object);
}

static void
tracker_db_interface_class_init (TrackerDBInterfaceClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->set_property = tracker_db_interface_sqlite_set_property;
	object_class->get_property = tracker_db_interface_sqlite_get_property;
	object_class->finalize = tracker_db_interface_sqlite_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_FILENAME,
	                                 g_param_spec_string ("filename",
	                                                      "DB filename",
	                                                      "DB filename",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_FLAGS,
	                                 g_param_spec_flags ("flags",
	                                                     "Flags",
	                                                     "Interface flags",
	                                                     TRACKER_TYPE_DB_INTERFACE_FLAGS, 0,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_SHARED_CACHE_KEY,
	                                 g_param_spec_string ("shared-cache-key",
	                                                      "Shared cache key",
	                                                      "Shared cache key",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
tracker_db_interface_init (TrackerDBInterface *db_interface)
{
	tracker_db_statement_mru_init (&db_interface->select_stmt_mru, 100,
	                               g_str_hash, g_str_equal, NULL);
	tracker_db_statement_mru_init (&db_interface->update_stmt_mru, 100,
	                               g_str_hash, g_str_equal, NULL);
}

void
tracker_db_interface_set_max_stmt_cache_size (TrackerDBInterface         *db_interface,
                                              TrackerDBStatementCacheType cache_type,
                                              guint                       max_size)
{
	TrackerDBStatementMru *stmt_mru;

	if (cache_type == TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE) {
		stmt_mru = &db_interface->update_stmt_mru;
	} else if (cache_type == TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT) {
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
	else if (cache_type == TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE)
		mru = &db_interface->update_stmt_mru;

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
        if (TRACKER_DEBUG_CHECK (SQL_STATEMENTS)) {
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
		/* This is rather fatal */
		if (errno != ENOSPC &&
		    (sqlite3_errcode (interface->db) == SQLITE_IOERR ||
		     sqlite3_errcode (interface->db) == SQLITE_CORRUPT ||
		     sqlite3_errcode (interface->db) == SQLITE_NOTADB)) {

			g_critical ("SQLite error: %s (errno: %s)",
			            sqlite3_errmsg (interface->db),
			            g_strerror (errno));
			return FALSE;
		}

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
		} else {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             errno != ENOSPC ? TRACKER_DB_QUERY_ERROR : TRACKER_DB_NO_SPACE,
			             "%s%s%s%s",
			             sqlite3_errmsg (interface->db),
			             errno != 0 ? " (strerror of errno (not necessarily related): " : "",
			             errno != 0 ? g_strerror (errno) : "",
			             errno != 0 ? ")" : "");
		}
	}

	return result == SQLITE_DONE;
}

void
tracker_db_interface_execute_vquery (TrackerDBInterface  *db_interface,
                                     GError             **error,
                                     const gchar         *query,
                                     va_list              args)
{
	gchar *full_query;
	sqlite3_stmt *stmt;

	tracker_db_interface_lock (db_interface);

	full_query = g_strdup_vprintf (query, args);
	stmt = tracker_db_interface_prepare_stmt (db_interface,
	                                          full_query,
	                                          error);
	g_free (full_query);
	if (stmt) {
		execute_stmt (db_interface, stmt, NULL, error);
		sqlite3_finalize (stmt);
	}

	tracker_db_interface_unlock (db_interface);
}

TrackerDBInterface *
tracker_db_interface_sqlite_new (const gchar              *filename,
                                 const gchar              *shared_cache_key,
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
	                         "shared-cache-key", shared_cache_key,
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
	g_object_ref (stmt->db_interface);
	return g_object_ref (stmt);
}

static void
tracker_db_statement_sqlite_release (TrackerDBStatement *stmt)
{
	TrackerDBInterface *iface = stmt->db_interface;

	stmt->stmt_is_borrowed = FALSE;

	tracker_db_statement_sqlite_reset (stmt);

	if (stmt->stmt_is_used) {
		stmt->stmt_is_used = FALSE;
		g_object_unref (stmt);
		g_object_unref (iface);
	}
}

static void
tracker_db_cursor_close (TrackerDBCursor *cursor)
{
	TrackerDBInterface *iface;

	g_return_if_fail (TRACKER_IS_DB_CURSOR (cursor));

	if (cursor->ref_stmt == NULL) {
		/* already closed */
		return;
	}

	iface = cursor->ref_stmt->db_interface;

	g_object_ref (iface);

	tracker_db_interface_lock (iface);
	g_clear_pointer (&cursor->ref_stmt, tracker_db_statement_sqlite_release);
	tracker_db_interface_unlock (iface);

	tracker_db_interface_unref_use (iface);

	g_object_unref (iface);
}

static void
tracker_db_cursor_finalize (GObject *object)
{
	TrackerDBCursor *cursor;

	cursor = TRACKER_DB_CURSOR (object);

	tracker_db_cursor_close (cursor);

	G_OBJECT_CLASS (tracker_db_cursor_parent_class)->finalize (object);
}

static void
tracker_db_cursor_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	TrackerDBCursor *cursor = TRACKER_DB_CURSOR (object);
	switch (prop_id) {
	case TRACKER_DB_CURSOR_PROP_N_COLUMNS:
		g_value_set_int (value, tracker_db_cursor_get_n_columns (cursor));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
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
tracker_db_cursor_iter_next_async (TrackerDBCursor     *cursor,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
	GTask *task;

	task = g_task_new (G_OBJECT (cursor), cancellable, callback, user_data);
	g_task_run_in_thread (task, tracker_db_cursor_iter_next_thread);
	g_object_unref (task);
}

static gboolean
tracker_db_cursor_iter_next_finish (TrackerDBCursor  *cursor,
                                    GAsyncResult     *res,
                                    GError          **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_db_cursor_class_init (TrackerDBCursorClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	TrackerSparqlCursorClass *sparql_cursor_class = TRACKER_SPARQL_CURSOR_CLASS (class);

	object_class->finalize = tracker_db_cursor_finalize;
	object_class->get_property = tracker_db_cursor_get_property;

	sparql_cursor_class->get_value_type = (TrackerSparqlValueType (*) (TrackerSparqlCursor *, gint)) tracker_db_cursor_get_value_type;
	sparql_cursor_class->get_variable_name = (const gchar * (*) (TrackerSparqlCursor *, gint)) tracker_db_cursor_get_variable_name;
	sparql_cursor_class->get_n_columns = (gint (*) (TrackerSparqlCursor *)) tracker_db_cursor_get_n_columns;
	sparql_cursor_class->get_string = (const gchar * (*) (TrackerSparqlCursor *, gint, glong*)) tracker_db_cursor_get_string;
	sparql_cursor_class->next = (gboolean (*) (TrackerSparqlCursor *, GCancellable *, GError **)) tracker_db_cursor_iter_next;
	sparql_cursor_class->next_async = (void (*) (TrackerSparqlCursor *, GCancellable *, GAsyncReadyCallback, gpointer)) tracker_db_cursor_iter_next_async;
	sparql_cursor_class->next_finish = (gboolean (*) (TrackerSparqlCursor *, GAsyncResult *, GError **)) tracker_db_cursor_iter_next_finish;
	sparql_cursor_class->rewind = (void (*) (TrackerSparqlCursor *)) tracker_db_cursor_rewind;
	sparql_cursor_class->close = (void (*) (TrackerSparqlCursor *)) tracker_db_cursor_close;

	sparql_cursor_class->get_integer = (gint64 (*) (TrackerSparqlCursor *, gint)) tracker_db_cursor_get_int;
	sparql_cursor_class->get_double = (gdouble (*) (TrackerSparqlCursor *, gint)) tracker_db_cursor_get_double;
	sparql_cursor_class->get_boolean = (gboolean (*) (TrackerSparqlCursor *, gint)) tracker_db_cursor_get_boolean;

	g_object_class_override_property (object_class, TRACKER_DB_CURSOR_PROP_N_COLUMNS, "n-columns");
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
        if (TRACKER_DEBUG_CHECK (SQL_STATEMENTS)) {
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
	cursor->ref_stmt = tracker_db_statement_sqlite_grab (ref_stmt);

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
	sqlite3_bind_blob (stmt->stmt, index + 1, data, len - 1, SQLITE_TRANSIENT);
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
	} else if (type == G_TYPE_BYTES) {
		GBytes *bytes;
		gconstpointer data;
		gsize len;

		bytes = g_value_get_boxed (value);
		data = g_bytes_get_data (bytes, &len);
		sqlite3_bind_text (stmt->stmt, index + 1,
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
tracker_db_cursor_rewind (TrackerDBCursor *cursor)
{
	TrackerDBInterface *iface;

	g_return_if_fail (TRACKER_IS_DB_CURSOR (cursor));

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	sqlite3_reset (cursor->stmt);
	cursor->finished = FALSE;

	tracker_db_interface_unlock (iface);
}

gboolean
tracker_db_cursor_iter_next (TrackerDBCursor *cursor,
                             GCancellable    *cancellable,
                             GError         **error)
{
	if (!cursor) {
		return FALSE;
	}

	return db_cursor_iter_next (cursor, cancellable, error);
}


static gboolean
db_cursor_iter_next (TrackerDBCursor *cursor,
                     GCancellable    *cancellable,
                     GError         **error)
{
	TrackerDBStatement *stmt = cursor->ref_stmt;
	TrackerDBInterface *iface = stmt->db_interface;

	if (!cursor->finished) {
		guint result;

		tracker_db_interface_lock (iface);

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

		tracker_db_interface_unlock (iface);
	}

	return (!cursor->finished);
}

guint
tracker_db_cursor_get_n_columns (TrackerDBCursor *cursor)
{
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
tracker_db_cursor_get_int (TrackerDBCursor *cursor,
                           guint            column)
{
	TrackerDBInterface *iface;
	gint64 result;

	if (cursor->n_columns > 0 && column >= cursor->n_columns)
		return 0;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	result = (gint64) sqlite3_column_int64 (cursor->stmt, column);

	tracker_db_interface_unlock (iface);

	return result;
}

gdouble
tracker_db_cursor_get_double (TrackerDBCursor *cursor,
                              guint            column)
{
	TrackerDBInterface *iface;
	gdouble result;

	if (cursor->n_columns > 0 && column >= cursor->n_columns)
		return 0;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	result = (gdouble) sqlite3_column_double (cursor->stmt, column);

	tracker_db_interface_unlock (iface);

	return result;
}

static gboolean
tracker_db_cursor_get_boolean (TrackerSparqlCursor *sparql_cursor,
                               guint                column)
{
	TrackerDBCursor *cursor = (TrackerDBCursor *) sparql_cursor;
	return (g_strcmp0 (tracker_db_cursor_get_string (cursor, column, NULL), "true") == 0);
}

static gboolean
tracker_db_cursor_get_annotated_value_type (TrackerDBCursor        *cursor,
                                            guint                   column,
                                            TrackerSparqlValueType *value_type)
{
	TrackerDBInterface *iface;
	TrackerPropertyType property_type;
	gboolean is_null;

	if (cursor->n_columns == 0)
		return FALSE;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	/* The value type may be annotated in extra columns, one per
	 * user-visible column.
	 */
	property_type = sqlite3_column_int64 (cursor->stmt, column + cursor->n_columns);
	is_null = sqlite3_column_type (cursor->stmt, column) == SQLITE_NULL;

	tracker_db_interface_unlock (iface);

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
		if (g_str_has_prefix (tracker_db_cursor_get_string (cursor, column, NULL),
		                      "urn:bnode:"))
			*value_type = TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		else
			*value_type = TRACKER_SPARQL_VALUE_TYPE_URI;

		return TRUE;
	};

	g_assert_not_reached ();
}

TrackerSparqlValueType
tracker_db_cursor_get_value_type (TrackerDBCursor *cursor,
                                  guint            column)
{
	TrackerDBInterface *iface;
	gint column_type;
	guint n_columns = tracker_db_cursor_get_n_columns (cursor);
	TrackerSparqlValueType value_type;

	g_return_val_if_fail (column < n_columns, TRACKER_SPARQL_VALUE_TYPE_UNBOUND);

	if (tracker_db_cursor_get_annotated_value_type (cursor, column, &value_type))
		return value_type;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	column_type = sqlite3_column_type (cursor->stmt, column);

	tracker_db_interface_unlock (iface);

	if (column_type == SQLITE_NULL) {
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
        }

	switch (column_type) {
	case SQLITE_INTEGER:
		return TRACKER_SPARQL_VALUE_TYPE_INTEGER;
	case SQLITE_FLOAT:
		return TRACKER_SPARQL_VALUE_TYPE_DOUBLE;
	default:
		return TRACKER_SPARQL_VALUE_TYPE_STRING;
	}
}

const gchar*
tracker_db_cursor_get_variable_name (TrackerDBCursor *cursor,
                                     guint            column)
{
	TrackerDBInterface *iface;
	const gchar *result;

	if (cursor->n_columns > 0 && column >= cursor->n_columns)
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
tracker_db_cursor_get_string (TrackerDBCursor *cursor,
                              guint            column,
                              glong           *length)
{
	TrackerDBInterface *iface;
	const gchar *result;

	if (cursor->n_columns > 0 && column >= cursor->n_columns)
		return NULL;

	iface = cursor->ref_stmt->db_interface;

	tracker_db_interface_lock (iface);

	if (length) {
		sqlite3_value *val = sqlite3_column_value (cursor->stmt, column);

		*length = sqlite3_value_bytes (val);
		result = (const gchar *) sqlite3_value_text (val);
	} else {
		result = (const gchar *) sqlite3_column_text (cursor->stmt, column);
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

GArray *
tracker_db_statement_get_values (TrackerDBStatement   *stmt,
                                 TrackerPropertyType   type,
                                 GError              **error)
{
	gint result = SQLITE_OK;
	GArray *values;

	tracker_db_interface_lock (stmt->db_interface);
	tracker_db_interface_ref_use (stmt->db_interface);
	tracker_db_statement_sqlite_grab (stmt);

#ifdef G_ENABLE_DEBUG
        if (TRACKER_DEBUG_CHECK (SQL_STATEMENTS)) {
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

	values = g_array_new (FALSE, TRUE, sizeof (GValue));
	g_array_set_clear_func (values, (GDestroyNotify) g_value_unset);

	while (TRUE) {
		GError *inner_error = NULL;
		GDateTime *datetime;
		GValue gvalue = G_VALUE_INIT;

		result = stmt_step (stmt->stmt);

		if (result == SQLITE_DONE) {
			break;
		} else if (result != SQLITE_ROW) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_QUERY_ERROR,
			             "%s", sqlite3_errmsg (stmt->db_interface->db));
			g_clear_pointer (&values, g_array_unref);
			break;
		}

		if (sqlite3_column_type (stmt->stmt, 0) == SQLITE_NULL)
			continue;

		switch (type) {
		case TRACKER_PROPERTY_TYPE_UNKNOWN:
		case TRACKER_PROPERTY_TYPE_STRING:
			g_value_init (&gvalue, G_TYPE_STRING);
			g_value_set_string (&gvalue, (gchar *) sqlite3_column_text (stmt->stmt, 0));
			break;
		case TRACKER_PROPERTY_TYPE_LANGSTRING: {
			sqlite3_value *val = sqlite3_column_value (stmt->stmt, 0);
			gchar *text;

			text = g_strdup ((const gchar *) sqlite3_value_text (val));

			g_value_init (&gvalue, G_TYPE_BYTES);
			g_value_take_boxed (&gvalue,
			                    g_bytes_new_with_free_func (text,
			                                                sqlite3_value_bytes (val),
			                                                g_free, text));
			break;
		}
		case TRACKER_PROPERTY_TYPE_DOUBLE:
			g_value_init (&gvalue, G_TYPE_DOUBLE);
			g_value_set_double (&gvalue, sqlite3_column_double (stmt->stmt, 0));
			break;
		case TRACKER_PROPERTY_TYPE_BOOLEAN:
		case TRACKER_PROPERTY_TYPE_INTEGER:
		case TRACKER_PROPERTY_TYPE_RESOURCE:
			g_value_init (&gvalue, G_TYPE_INT64);
			g_value_set_int64 (&gvalue, sqlite3_column_int64 (stmt->stmt, 0));
			break;
		case TRACKER_PROPERTY_TYPE_DATE:
		case TRACKER_PROPERTY_TYPE_DATETIME:
			if (sqlite3_column_type (stmt->stmt, 0) == SQLITE_INTEGER) {
				datetime = g_date_time_new_from_unix_utc (sqlite3_column_int64 (stmt->stmt, 0));
			} else {
				datetime = tracker_date_new_from_iso8601 ((const gchar *) sqlite3_column_text (stmt->stmt, 0),
				                                          &inner_error);
				if (!datetime)
					break;
			}

			g_value_init (&gvalue, G_TYPE_DATE_TIME);
			g_value_take_boxed (&gvalue, datetime);
			break;
		}

		if (inner_error) {
			g_propagate_error (error, inner_error);
			g_clear_pointer (&values, g_array_unref);
			break;
		}

		g_array_append_val (values, gvalue);
	}

	tracker_db_statement_sqlite_release (stmt);
	tracker_db_interface_unref_use (stmt->db_interface);
	tracker_db_interface_unlock (stmt->db_interface);

	return values;
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
                                    gpointer            user_data,
                                    GDestroyNotify      destroy_notify)
{
	if (db_interface->user_data && db_interface->user_data_destroy_notify)
		db_interface->user_data_destroy_notify (db_interface->user_data);

	db_interface->user_data = user_data;
	db_interface->user_data_destroy_notify = destroy_notify;
}

gpointer
tracker_db_interface_get_user_data (TrackerDBInterface *db_interface)
{
	return db_interface->user_data;
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

gboolean
tracker_db_interface_init_vtabs (TrackerDBInterface *db_interface,
                                 gpointer            vtab_data)
{
	tracker_vtab_triples_init (db_interface->db, vtab_data);
	tracker_vtab_service_init (db_interface->db, vtab_data);
	return TRUE;
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

	g_return_val_if_fail (file || db_interface->shared_cache_key, FALSE);

	if (file) {
		uri = g_file_get_path (file);
	} else if (db_interface->shared_cache_key &&
	           (db_interface->flags & TRACKER_DB_INTERFACE_IN_MEMORY) != 0) {
		gchar *md5;

		md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, name, -1);
		uri = g_strdup_printf ("file:%s-%s?mode=memory&cache=shared",
		                       db_interface->shared_cache_key, md5);
		g_free (md5);
	}

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

gboolean
tracker_db_interface_detach_database (TrackerDBInterface  *db_interface,
                                      const gchar         *name,
                                      GError             **error)
{
	sqlite3_stmt *stmt;
	gboolean retval;
	gchar *sql;

	sql = g_strdup_printf ("DETACH DATABASE \"%s\"", name);
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
	tracker_db_statement_mru_clear (&db_interface->update_stmt_mru);

	return (gssize) sqlite3_db_release_memory (db_interface->db);
}
