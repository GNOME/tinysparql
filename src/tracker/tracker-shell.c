/*
 * Copyright (C) 2021, Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

/* FIXME: The editor is generally pretty oblivious to unicode input */

#include "config.h"

#include <sys/param.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>

#include <termios.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-common/tracker-common.h>

#include "tracker-sparql.h"
#include "tracker-color.h"

enum {
	DIRECTION_UP,
	DIRECTION_DOWN,
	DIRECTION_LEFT,
	DIRECTION_RIGHT,
};

typedef struct
{
	GPtrArray *lines; /* Array of GString */
	guint line, col;
} EditorState;

static struct termios original_termios = { 0 };
static int ctrl_c_counter = 0;
static EditorState *staging = NULL; /* The last new buffer */
static EditorState *state = NULL;
static GList *history = NULL;

#define CTRL_KEYCOMBO(c) ((c) & 0x1F)
#define KEY_ENTER 13
#define KEY_BACKSPACE 127
#define ESCAPE_SEQUENCE 27

#define LINE_PADDING 3 /* Prompt, plus possible ellipsis (or spaces) on both sides */
#define ROW_PADDING 1

#define COL_MAX_LEN 100

static gchar *database_path;
static gchar *dbus_service;
static gchar *remote_service;
static const gchar *subtitle;

static GOptionEntry entries[] = {
	{ "database", 'd', 0, G_OPTION_ARG_FILENAME, &database_path,
	  N_("Location of the database"),
	  N_("FILE")
	},
	{ "dbus-service", 'b', 0, G_OPTION_ARG_STRING, &dbus_service,
	  N_("Connects to a DBus service"),
	  N_("DBus service name")
	},
	{ "remote-service", 'r', 0, G_OPTION_ARG_STRING, &remote_service,
	  N_("Connects to a remote service"),
	  N_("Remote service URI")
	},
	{ NULL }
};

static inline char
read_char (void)
{
	char c;

	if (read (STDIN_FILENO, &c, 1) != 1)
		return 0;

	return c;
}

static GString *
string_array_add_line (GPtrArray *array,
                       gint       idx)
{
	GString *str;

	str = g_string_new ("");
	g_ptr_array_insert (array, idx, str);

	return str;
}

static EditorState *
editor_state_new (void)
{
	EditorState *state = g_new0 (EditorState, 1);

	state->lines = g_ptr_array_new ();
	string_array_add_line (state->lines, -1);

	return state;
}

static void
editor_state_free (EditorState *state)
{
	guint i;

	for (i = 0; i < state->lines->len; i++)
		g_string_free (g_ptr_array_index (state->lines, i), TRUE);

	g_ptr_array_unref (state->lines);
	g_free (state);
}

static gchar *
editor_state_to_string (EditorState *state)
{
	GString *str = g_string_new (NULL);
	guint i;

	for (i = 0; i < state->lines->len; i++) {
		GString *line = g_ptr_array_index (state->lines, i);

		g_string_append (str, line->str);
		g_string_append_c (str, '\n');
	}

	return g_string_free (str, FALSE);
}

static void
editor_state_insert_char (EditorState *state,
                          gunichar     ch)
{
	GString *str;

	str = g_ptr_array_index (state->lines, state->line);
	g_assert (str != NULL);
	g_assert (state->col <= str->len);

	g_string_insert_unichar (str, state->col, ch);
	state->col++;
}

static void
editor_state_handle_enter (EditorState *state)
{
	GString *str, *new;

	str = g_ptr_array_index (state->lines, state->line);
	g_assert (str != NULL);
	g_assert (state->col <= str->len);

	if (state->col == str->len) {
		string_array_add_line (state->lines, state->line + 1);
	} else {
		new = string_array_add_line (state->lines, state->line + 1);
		g_string_append (new, &str->str[state->col]);
		g_string_erase (str, state->col, -1);
	}

	state->line++;
	state->col = 0;
}

static void
editor_state_handle_delete (EditorState *state)
{
	GString *str, *prev;

	str = g_ptr_array_index (state->lines, state->line);
	g_assert (str != NULL);
	g_assert (state->col <= str->len);

	if (state->col == 0) {
		if (state->line > 0) {
			/* Merge with previous line */
			prev = g_ptr_array_index (state->lines, state->line - 1);
			g_ptr_array_remove_index (state->lines, state->line);
			state->col = prev->len;
			state->line--;

			g_string_append (prev, str->str);
			g_string_free (str, TRUE);
		}
	} else {
		g_string_erase (str, state->col - 1, 1);
		state->col--;
	}
}

static void
editor_state_handle_delete_forward (EditorState *state)
{
	GString *str, *next;

	str = g_ptr_array_index (state->lines, state->line);
	g_assert (str != NULL);
	g_assert (state->col <= str->len);

	if (state->col == str->len) {
		if (state->line < state->lines->len - 1) {
			/* Merge with following line */
			next = g_ptr_array_index (state->lines, state->line + 1);
			g_ptr_array_remove_index (state->lines, state->line + 1);

			g_string_append (str, next->str);
			g_string_free (next, TRUE);
		}
	} else {
		g_string_erase (str, state->col, 1);
	}
}

static void
editor_state_handle_move (EditorState *state,
                          int          direction)
{
	GString *str;

	switch (direction) {
	case DIRECTION_LEFT:
		if (state->col == 0) {
			if (state->line > 0) {
				state->line--;
				str = g_ptr_array_index (state->lines, state->line);
				state->col = str->len;
			}
		} else {
			state->col--;
		}
		break;
	case DIRECTION_RIGHT:
		str = g_ptr_array_index (state->lines, state->line);
		if (state->col >= str->len) {
			if (state->line < state->lines->len - 1) {
				state->line++;
				state->col = 0;
			}
		} else {
			state->col++;
		}
		break;
	case DIRECTION_UP:
		if (state->line > 0) {
			state->line--;
			str = g_ptr_array_index (state->lines, state->line);
			state->col = MIN (state->col, str->len);
		}
		break;
	case DIRECTION_DOWN:
		if (state->line < state->lines->len - 1) {
			state->line++;
			str = g_ptr_array_index (state->lines, state->line);
			state->col = MIN (state->col, str->len);
		}
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
find_next_word_position (EditorState *state,
                         int          direction,
                         guint       *line,
                         guint       *col)
{
	gint inc = 0, cur_line, cur_col, next_line, next_col;
	GString *str;
	gboolean found_nonspace = FALSE;
	gchar ch;

	if (direction == DIRECTION_LEFT)
		inc = -1;
	else if (direction == DIRECTION_RIGHT)
		inc = 1;
	else
		g_assert_not_reached ();

	cur_line = next_line = state->line;
	cur_col = next_col = state->col;

	do {
		str = g_ptr_array_index (state->lines, cur_line);

		if (cur_col + inc < 0) {
			if (cur_line == 0)
				break;
			next_line = cur_line - 1;
			str = g_ptr_array_index (state->lines, next_line);
			next_col = str->len;
		} else if (cur_col + inc > (int) str->len) {
			if (cur_line == (int) state->lines->len - 1)
				break;
			next_line = cur_line + 1;
			str = g_ptr_array_index (state->lines, next_line);
			next_col = 0;
		} else {
			next_line = cur_line;
			next_col = cur_col + inc;
		}

		ch = str->str[next_col];

		if (!g_ascii_isspace (ch) || str->len == 0)
			found_nonspace = TRUE;
		else if (found_nonspace)
			break;

		cur_line = next_line;
		cur_col = next_col;
	} while (next_line >= 0 && next_line < (int) state->lines->len);

	if (next_line != (int) state->line ||
	    next_col != (int) state->col) {
		if (inc > 0) {
			*line = next_line;
			*col = next_col;
		} else {
			*line = cur_line;
			*col = cur_col;
		}

		return TRUE;
	}

	return FALSE;
}

static void
editor_state_handle_delete_word (EditorState *state)
{
	GString *str, *prev;
	guint line, col, i;

	if (!find_next_word_position (state, DIRECTION_LEFT, &line, &col))
		return;

	str = g_ptr_array_index (state->lines, state->line);

	if (line == state->line) {
		g_assert (col < state->col);
		g_string_erase (str, col, state->col - col);
		state->col = col;
	} else {
		gint len;

		g_assert (line < state->line);

		prev = g_ptr_array_index (state->lines, line);

		/* Delete end of previous line and beginning of last */
		g_string_erase (prev, col, -1);
		g_string_erase (str, 0, state->col);

		if (str->len == 0)
			len = state->line - line;
		else
			len = state->line - line - 1;

		/* Delete intermediate lines */
		for (i = line + 1; i < state->line; i++) {
			g_string_free (g_ptr_array_index (state->lines, i),
			               TRUE);
		}

		if (len > 0)
			g_ptr_array_remove_range (state->lines, line + 1, len);

		state->line = line;
		state->col = col;
	}
}

static void
editor_state_handle_move_word (EditorState *state,
                               int          direction)
{
	guint line, col;

	if (direction == DIRECTION_LEFT || direction == DIRECTION_RIGHT) {
		if (find_next_word_position (state, direction, &line, &col)) {
			state->line = line;
			state->col = col;
		}
	} else {
		g_assert_not_reached ();
	}
}

static void
editor_state_handle_home (EditorState *state)
{
	state->col = 0;
}

static void
editor_state_handle_end (EditorState *state)
{
	GString *str;

	str = g_ptr_array_index (state->lines, state->line);
	state->col = str->len;
}

static void
print_line (EditorState *state,
            guint        line,
            guint        first_col,
            guint        cols)
{
	GString *str;

	str = g_ptr_array_index (state->lines, line);

	g_print ("»");

	if (first_col != 0)
		g_print ("…");
	else
		g_print (" ");

	if (str->len - first_col < cols - LINE_PADDING) {
		g_print ("%s", &str->str[first_col]);
	} else {
		gchar *truncated;

		truncated = g_strndup (&str->str[first_col], cols - LINE_PADDING);
		g_print ("%s", truncated);
		g_free (truncated);
	}

	if (first_col + cols < str->len)
		g_print ("…");
}

static guint
calculate_dimensions (guint len,
                      guint available_size,
                      guint pos)
{
	gint first_elem;

	if (len <= available_size ||
	    pos < available_size / 2)
		first_elem = 0;
	else if (pos > len - (available_size / 2))
		first_elem = len - available_size;
	else
		first_elem = pos - (available_size / 2) - (available_size % 2);

	first_elem = MAX (0, first_elem);

	return first_elem;
}

static void
get_viewport (EditorState *state,
              guint        rows,
              guint        cols,
              guint       *first_line,
              guint       *first_column)
{
	GString *str;

	str = g_ptr_array_index (state->lines, state->line);
	*first_line = calculate_dimensions (state->lines->len, rows - ROW_PADDING, state->line);
	*first_column = calculate_dimensions (str->len, cols - LINE_PADDING, state->col);
}

static void
editor_state_print (EditorState *state)
{
	guint rows, cols, i, first_col, first_line;

	/* Hide cursor */
	g_print ("\x1b[?25l");

	/* Move to first line/col */
	g_print ("\x1b[%d;%dH", 1, 1);

	tracker_term_dimensions (&cols, &rows);
	get_viewport (state, rows, cols, &first_line, &first_col);

	for (i = 0; i < rows; i++) {
		guint line = i + first_line;

		/* Clear line */
		g_print ("\x1b[K");

		if (i == rows - 1) {
			g_print ("Ctrl-Q to quit. Ctrl-X to execute SPARQL. PgUp/PgDown to navigate history");
			break;
		}

		if (line < state->lines->len)
			print_line (state, line, first_col, cols);

		g_print ("\r\n");
	}

	/* Set cursor in position */
	g_print ("\x1b[%d;%dH",
	         state->line + 1 - first_line,
	         state->col + 3 - first_col);

	/* Show cursor again */
	g_print ("\x1b[?25h");
}

static void
editor_state_find_offset (EditorState *state,
                          guint64      offset)
{
	guint i;

	for (i = 0; i < state->lines->len; i++) {
		GString *str = g_ptr_array_index (state->lines, i);

		if (offset < str->len) {
			state->line = i;
			state->col = offset;
		}

		offset -= str->len;
	}
}

static void
editor_history_move_up (void)
{
	GList *item;

	item = g_list_find (history, state);
	if (!item) {
		staging = state;
		state = history->data;
	} else if (item->next) {
		state = item->next->data;
	}
}

static void
editor_history_move_down (void)
{
	GList *item;

	item = g_list_find (history, state);
	if (!item)
		return;

	if (item->prev) {
		state = item->prev->data;
	} else if (item) {
		state = staging;
		staging = NULL;
	}
}

static void
editor_state_handle_key (EditorState *state,
                         gunichar     key)
{
	if (key == CTRL_KEYCOMBO ('h') || key == KEY_BACKSPACE) {
		editor_state_handle_delete (state);
	} else if (key == CTRL_KEYCOMBO ('w')) {
		editor_state_handle_delete_word (state);
	} else if (key == KEY_ENTER) {
		editor_state_handle_enter (state);
	} else if (key == ESCAPE_SEQUENCE) {
		gchar ch;

		switch ((ch = read_char ())) {
		case '[':
			switch ((ch = read_char ())) {
			case 'A':
				/* Up arrow */
				editor_state_handle_move (state, DIRECTION_UP);
				break;
			case 'B':
				/* Down arrow */
				editor_state_handle_move (state, DIRECTION_DOWN);
				break;
			case 'C':
				/* Right arrow */
				editor_state_handle_move (state, DIRECTION_RIGHT);
				break;
			case 'D':
				/* Left arrow */
				editor_state_handle_move (state, DIRECTION_LEFT);
				break;
			case '1':
				if (read_char () == ';' &&
				    read_char () == '5') {
					switch ((ch = read_char ())) {
					case 'C':
						/* Ctrl + Right */
						editor_state_handle_move_word (state, DIRECTION_RIGHT);
						break;
					case 'D':
						/* Ctrl + Left */
						editor_state_handle_move_word (state, DIRECTION_LEFT);
						break;
					default:
						g_debug ("Escape sequence: '[1;5%c'", ch);
						break;
					}
				}
				break;
			case '3':
				if (read_char () == '~')
					editor_state_handle_delete_forward (state);
				break;
			case '5':
				if (read_char () == '~')
					editor_history_move_up ();
				break;
			case '6':
				if (read_char () == '~')
					editor_history_move_down ();
				break;
			case 'H':
				editor_state_handle_home (state);
				break;
			case 'F':
				editor_state_handle_end (state);
				break;
			default:
				g_debug ("Escape sequence: '[%c'", ch);
				read_char ();
				break;
			}
			break;
		default:
			g_debug ("Escape sequence: '%c'", ch);
			break;
		}
	} else if (!g_ascii_iscntrl (key)) {
		/* Visible characters */
		editor_state_insert_char (state, key);
	}
}

static void
disable_raw_mode (void)
{
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &original_termios);
}

static void
enable_raw_mode (void)
{
	struct termios termios;

	tcgetattr (STDIN_FILENO, &original_termios);
	termios = original_termios;
	termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	termios.c_iflag &= ~(ICRNL | IXON | INPCK | ISTRIP);
	termios.c_oflag &= ~(OPOST);
	termios.c_cflag |= CS8;
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &termios);
}

static void
pad_string (GString     *str,
            guint        expected_len,
            const gchar *ch)
{
	guint i, len;

	len = g_utf8_strlen (str->str, -1);

	for (i = len; i < expected_len; i++)
		g_string_append (str, ch);
}

typedef struct {
	gchar *prefix;
	const gchar *shorthand;
} ShorthandLookup;

static void
find_shorthand (gpointer key,
                gpointer value,
                gpointer user_data)
{
	ShorthandLookup *data = user_data;

	if (g_strcmp0 ((const gchar *) value, data->prefix))
		data->shorthand = key;
}

static gchar *
get_uri_shorthand (TrackerNamespaceManager *namespaces,
                   const gchar             *uri)
{
	ShorthandLookup data;
	const gchar *loc;

	loc = strstr (uri, "#");
	if (!loc)
		return NULL;

	loc++;
	data.prefix = g_strndup (uri, loc - uri);
	tracker_namespace_manager_foreach (namespaces, find_shorthand, &data);
	g_free (data.prefix);

	if (!data.shorthand)
		return NULL;

	return g_strdup_printf ("%s:%s", data.shorthand, loc);
}

static gchar *
limit_string_length (const gchar *str)
{
	if (g_utf8_strlen (str, -1) < COL_MAX_LEN)
		return g_strdup (str);

	return g_utf8_substring (str, 0, COL_MAX_LEN);
}

static gchar *
format_column_output (TrackerSparqlCursor *cursor,
                      gint                 col)
{
	TrackerSparqlConnection *conn;
	TrackerNamespaceManager *namespaces;
	const gchar *col_str;
	gchar *shortened = NULL, *limited;

	col_str = tracker_sparql_cursor_get_string (cursor, col, NULL);
	if (!col_str)
		return NULL;

	conn = tracker_sparql_cursor_get_connection (cursor);
	namespaces = tracker_sparql_connection_get_namespace_manager (conn);

	if (namespaces)
		shortened = get_uri_shorthand (namespaces, col_str);

	limited = limit_string_length (shortened ? shortened : col_str);
	g_free (shortened);

	return limited;
}

static void
print_cursor (TrackerSparqlCursor *cursor)
{
	GPtrArray *lines;
	gint i, n_columns = 0;
	guint j, padding, next_padding;
	gchar *col_str;
	GString *str;

	lines = g_ptr_array_new ();

	/* First, set up the lines buffer */

	/* Add additional lines for headers */
	for (i = 0; i < 3; i++)
		string_array_add_line (lines, -1);

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		n_columns = tracker_sparql_cursor_get_n_columns (cursor);
		string_array_add_line (lines, -1);
	}

	if (n_columns == 0)
		return;

	/* Add last line for table decoration */
	string_array_add_line (lines, -1);
	next_padding = 0;

	/* Second. For every column, iterate the cursor and pad the strings properly */
	for (i = 0; i < n_columns; i++) {
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		tracker_sparql_cursor_rewind (cursor);
		G_GNUC_END_IGNORE_DEPRECATIONS
		padding = next_padding;
		next_padding = 0;
		j = 0;

		for (j = 0; j < lines->len; j++) {
			str = g_ptr_array_index (lines, j);
			pad_string (str, padding,
			            (j == 0 || j == 2 || j == lines->len - 1) ?
			            "─" : " ");

			if (j == 0 && i == 0)
				g_string_append (str, "┌");
			else if (j == 2 && i == 0)
				g_string_append (str, "├");
			else if (j == lines->len - 1 && i == 0)
				g_string_append (str, "└");
			else if (j == 0)
				g_string_append (str, "┬");
			else if (j == 2)
				g_string_append (str, "┼");
			else if (j == lines->len - 1)
				g_string_append (str, "┴");
			else
				g_string_append (str, "│");

			if (j == 1) {
				g_string_append (str, tracker_sparql_cursor_get_variable_name (cursor, i));
			} else if (j > 2 && j != lines->len - 1) {
				tracker_sparql_cursor_next (cursor, NULL, NULL);
				col_str = format_column_output (cursor, i);

				if (col_str)
					g_string_append (str, col_str);
				else
					g_string_append_c (str, ' ');
				g_free (col_str);
			}

			next_padding = MAX (next_padding, g_utf8_strlen (str->str, -1));
		}
	}

	/* Last, add last padding/decoration to strings, print and free */
	for (j = 0; j < lines->len; j++) {
		str = g_ptr_array_index (lines, j);

		pad_string (str, next_padding,
		            (j == 0 || j == 2 || j == lines->len - 1) ?
		            "─" : " ");

		if (j == 0)
			g_string_append (str, "┐");
		else if (j == 2)
			g_string_append (str, "┤");
		else if (j == lines->len - 1)
			g_string_append (str, "┘");
		else
			g_string_append (str, "│");

		g_print ("%s\r\n", str->str);
		g_string_free (str, TRUE);
	}

	g_ptr_array_unref (lines);
}

static TrackerSparqlConnection *
create_connection (GError **error)
{
	if (database_path && !dbus_service && !remote_service) {
		GFile *file;

		file = g_file_new_for_commandline_arg (database_path);
		subtitle = g_file_peek_path (file);
		return tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
		                                      file, NULL, NULL, error);
	} else if (dbus_service && !database_path && !remote_service) {
		GDBusConnection *dbus_conn;

		dbus_conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
		if (!dbus_conn)
			return NULL;

		subtitle = dbus_service;
		return tracker_sparql_connection_bus_new (dbus_service, NULL, dbus_conn, error);
	} else if (remote_service && !database_path && !dbus_service) {
		subtitle = remote_service;
		return tracker_sparql_connection_remote_new (remote_service);
	} else {
		TrackerSparqlConnection *conn;
		GFile *ontology;

		ontology = tracker_sparql_get_ontology_nepomuk ();
		conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
		                                      NULL,
		                                      ontology,
		                                      NULL,
		                                      NULL);
		g_object_unref (ontology);

		return conn;
	}
}

int
main (int argc, char *argv[])
{
	TrackerSparqlConnection *conn;
	GOptionContext *context;
	GError *error = NULL;
	gunichar c;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tracker shell";

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	g_option_context_free (context);


	/* Check we are not redirected */
	if (!tracker_term_is_tty ()) {
		g_printerr ("Output must be a TTY");
		return EXIT_FAILURE;
	}

	conn = create_connection (&error);
	if (!conn) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	/* Change terminal title */
	g_print ("\033]0;SPARQL Shell%s%s\007",
	         subtitle ? ": " : "",
	         subtitle ? subtitle : "");

	g_assert (conn != NULL);

	enable_raw_mode ();
	atexit (disable_raw_mode);

	state = editor_state_new ();
	editor_state_print (state);

	while (TRUE) {
		c = read_char ();

		if (c == CTRL_KEYCOMBO ('q')) {
			break;
		} else if (c == CTRL_KEYCOMBO ('c')) {
			ctrl_c_counter++;
			if (ctrl_c_counter == 3)
				break;

			continue;
		}

		ctrl_c_counter = 0;

		if (c == CTRL_KEYCOMBO ('x')) {
			TrackerSparqlCursor *cursor;
			gchar *sparql;
			GError *error = NULL;

			/* Execute the SPARQL query */

			/* Erase screen */
			g_print ("\x1b[2J\r");

			/* Move to first line/col */
			g_print ("\x1b[%d;%dH", 1, 1);
			g_print ("\r");

			sparql = editor_state_to_string (state);
			cursor = tracker_sparql_connection_query (conn, sparql,
			                                          NULL, &error);
			g_free (sparql);

			if (cursor) {
				/* Temporarily disable raw mode to redirect cursor to pager */
				disable_raw_mode ();
				tracker_term_pipe_to_pager (FALSE);
				print_cursor (cursor);
				tracker_term_pager_close ();
				enable_raw_mode ();

				g_object_unref (cursor);

				/* Move current state last in history */
				history = g_list_remove (history, state);
				history = g_list_prepend (history, state);

				g_clear_pointer (&staging, editor_state_free);
				state = editor_state_new ();
				editor_state_print (state);
			} else {
				gchar *error_str;
				gchar **error_lines;

				/* Look for our own syntax errors, and update editor
				 * cursor based on the error location.
				 */
				if (g_str_has_prefix (error->message, "Parser error at byte")) {
					guint64 offset;

					if (sscanf (error->message,
					            "Parser error at byte %" G_GUINT64_FORMAT,
					            &offset) == 1) {
						editor_state_find_offset (state, offset);
					}
				}

				error_lines = g_strsplit (error->message, "\n", -1);
				error_str = g_strjoinv ("\r\n", error_lines);
				g_print ("%s\r\n", error_str);
				g_error_free (error);
				g_free (error_str);
				g_strfreev (error_lines);

				/* Temporarily hide the cursor while showing the error */
				g_print ("\x1b[?25l");
			}

			continue;
		}

		editor_state_handle_key (state, c);
		editor_state_print (state);
	}

	if (!g_list_find (history, state))
		editor_state_free (state);

	g_clear_pointer (&staging, editor_state_free);
	g_list_free_full (history, (GDestroyNotify) editor_state_free);

	tracker_sparql_connection_close (conn);

	/* Erase screen */
	g_print ("\x1b[2J\r");

	/* Move to first line/col */
	g_print ("\x1b[%d;%dH", 1, 1);
	g_print ("\r");

	return EXIT_SUCCESS;
}
