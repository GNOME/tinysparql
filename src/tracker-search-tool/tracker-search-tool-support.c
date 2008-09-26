/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GNOME Search Tool
 *
 *  File:  tracker_search-support.c
 *
 *  (C) 2002 the Free Software Foundation
 *
 *  Authors:	Dennis Cranston  <dennis_cranston@yahoo.com>
 *		George Lebl
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib/gi18n.h>
#include <glib/gdate.h>
#include <regex.h>
#include <gdk/gdkx.h>
#include <libart_lgpl/art_rgb.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomeui/gnome-thumbnail.h>

#include <gnome.h>

#include "tracker-search-tool.h"
#include "tracker-search-tool-callbacks.h"
#include "tracker-search-tool-support.h"

#define C_STANDARD_STRFTIME_CHARACTERS "aAbBcdHIjmMpSUwWxXyYZ"
#define C_STANDARD_NUMERIC_STRFTIME_CHARACTERS "dHIjmMSUwWyY"
#define SUS_EXTENDED_STRFTIME_MODIFIERS "EO"
#define BINARY_EXEC_MIME_TYPE	   "application/x-executable"
#define MAX_SYMLINKS_FOLLOWED	   32
#define GSEARCH_DATE_FORMAT_LOCALE "locale"
#define GSEARCH_DATE_FORMAT_ISO    "iso"

gboolean
tracker_is_empty_string (const gchar *s)
{
	return s == NULL || s[0] == '\0';
}

/* START OF THE GCONF FUNCTIONS */

static gboolean
tracker_search_gconf_handle_error (GError ** error)
{
	if (error != NULL) {
		if (*error != NULL) {
			g_warning (_("GConf error:\n  %s"), (*error)->message);
			g_error_free (*error);
			*error = NULL;
			return TRUE;
		}
	}
	return FALSE;
}

static GConfClient *
tracker_search_gconf_client_get_global (void)
{
	static GConfClient * global_gconf_client = NULL;

	/* Initialize gconf if needed */
	if (!gconf_is_initialized ()) {
		gchar *argv[] = { "tracker_search-preferences", NULL };
		GError *error = NULL;

		if (!gconf_init (1, argv, &error)) {
			if (tracker_search_gconf_handle_error (&error)) {
				return NULL;
			}
		}
	}

	if (global_gconf_client == NULL) {
		global_gconf_client = gconf_client_get_default ();
	}

	return global_gconf_client;
}

gchar *
tracker_search_gconf_get_string (const gchar * key)
{
	GConfClient * client;
	GError * error = NULL;
	gchar * result;

	g_return_val_if_fail (key != NULL, NULL);

	client = tracker_search_gconf_client_get_global ();
	g_return_val_if_fail (client != NULL, NULL);

	result = gconf_client_get_string (client, key, &error);

	if (tracker_search_gconf_handle_error (&error)) {
		result = g_strdup ("");
	}

	return result;
}

GSList *
tracker_search_gconf_get_list (const gchar * key,
			       GConfValueType list_type)
{
	GConfClient * client;
	GError * error = NULL;
	GSList * result;

	g_return_val_if_fail (key != NULL, FALSE);

	client = tracker_search_gconf_client_get_global ();
	g_return_val_if_fail (client != NULL, NULL);

	result = gconf_client_get_list (client, key, list_type, &error);

	if (tracker_search_gconf_handle_error (&error)) {
		result = NULL;
	}

	return result;
}

void
tracker_search_gconf_set_list (const gchar * key,
			       GSList * list,
			       GConfValueType list_type)
{
	GConfClient * client;
	GError * error = NULL;

	g_return_if_fail (key != NULL);

	client = tracker_search_gconf_client_get_global ();
	g_return_if_fail (client != NULL);

	gconf_client_set_list (client, key, list_type, list, &error);

	tracker_search_gconf_handle_error (&error);
}

gint
tracker_search_gconf_get_int (const gchar * key)
{
	GConfClient * client;
	GError * error = NULL;
	gint result;

	g_return_val_if_fail (key != NULL, FALSE);

	client = tracker_search_gconf_client_get_global ();
	g_return_val_if_fail (client != NULL, FALSE);

	result = gconf_client_get_int (client, key, &error);

	if (tracker_search_gconf_handle_error (&error)) {
		result = 0;
	}

	return result;
}

void
tracker_search_gconf_set_int (const gchar * key,
			      const gint value)
{
	GConfClient * client;
	GError * error = NULL;

	g_return_if_fail (key != NULL);

	client = tracker_search_gconf_client_get_global ();
	g_return_if_fail (client != NULL);

	gconf_client_set_int (client, key, value, &error);

	tracker_search_gconf_handle_error (&error);
}

gboolean
tracker_search_gconf_get_boolean (const gchar * key)
{
	GConfClient * client;
	GError * error = NULL;
	gboolean result;

	g_return_val_if_fail (key != NULL, FALSE);

	client = tracker_search_gconf_client_get_global ();
	g_return_val_if_fail (client != NULL, FALSE);

	result = gconf_client_get_bool (client, key, &error);

	if (tracker_search_gconf_handle_error (&error)) {
		result = FALSE;
	}

	return result;
}

void
tracker_search_gconf_set_boolean (const gchar * key,
				  const gboolean flag)
{
	GConfClient * client;
	GError * error = NULL;

	g_return_if_fail (key != NULL);

	client = tracker_search_gconf_client_get_global ();
	g_return_if_fail (client != NULL);

	gconf_client_set_bool (client, key, flag, &error);

	tracker_search_gconf_handle_error (&error);
}

void
tracker_search_gconf_add_dir (const gchar * dir)
{
	GConfClient * client;
	GError * error = NULL;

	g_return_if_fail (dir != NULL);

	client = tracker_search_gconf_client_get_global ();
	g_return_if_fail (client != NULL);

	gconf_client_add_dir (client,
			      dir,
			      GCONF_CLIENT_PRELOAD_RECURSIVE,
			      &error);

	tracker_search_gconf_handle_error (&error);
}

void
tracker_search_gconf_watch_key (const gchar * dir,
				const gchar * key,
				GConfClientNotifyFunc callback,
				gpointer user_data)
{
	GConfClient * client;
	GError * error = NULL;

	g_return_if_fail (key != NULL);
	g_return_if_fail (dir != NULL);

	client = tracker_search_gconf_client_get_global ();
	g_return_if_fail (client != NULL);

	gconf_client_add_dir (client,
			      dir,
			      GCONF_CLIENT_PRELOAD_NONE,
			      &error);

	tracker_search_gconf_handle_error (&error);

	gconf_client_notify_add (client,
				 key,
				 callback,
				 user_data,
				 NULL,
				 &error);

	tracker_search_gconf_handle_error (&error);
}

/* START OF GENERIC GNOME-SEARCH-TOOL FUNCTIONS */

gboolean
is_path_hidden (const gchar * path)
{
	gint results = FALSE;
	gchar * sub_str;
	gchar * hidden_path_substr = g_strconcat (G_DIR_SEPARATOR_S, ".", NULL);

	sub_str = g_strstr_len (path, strlen (path), hidden_path_substr);

	if (sub_str != NULL) {
		gchar * gnome_desktop_str;

		gnome_desktop_str = g_strconcat (G_DIR_SEPARATOR_S, ".gnome-desktop", G_DIR_SEPARATOR_S, NULL);

		/* exclude the .gnome-desktop folder */
		if (strncmp (sub_str, gnome_desktop_str, strlen (gnome_desktop_str)) == 0) {
			sub_str++;
			results = (g_strstr_len (sub_str, strlen (sub_str), hidden_path_substr) != NULL);
		}
		else {
			results = TRUE;
		}

		g_free (gnome_desktop_str);
	}

	g_free (hidden_path_substr);
	return results;
}

gboolean
is_quick_search_excluded_path (const gchar * path)
{
	GSList	   * exclude_path_list;
	GSList	   * tmp_list;
	gchar	   * dir;
	gboolean     results = FALSE;

	dir = g_strdup (path);

	/* Remove trailing G_DIR_SEPARATOR. */
	if ((strlen (dir) > 1) && (g_str_has_suffix (dir, G_DIR_SEPARATOR_S) == TRUE)) {
		dir[strlen (dir) - 1] = '\0';
	}

	/* Always exclude a path that is symbolic link. */
	if (g_file_test (dir, G_FILE_TEST_IS_SYMLINK)) {
		g_free (dir);

		return TRUE;
	}
	g_free (dir);

	/* Check path against the Quick-Search-Excluded-Paths list. */
	exclude_path_list = tracker_search_gconf_get_list ("/apps/tracker-search-tool/quick_search_excluded_paths",
							GCONF_VALUE_STRING);

	for (tmp_list = exclude_path_list; tmp_list; tmp_list = tmp_list->next) {

		gchar * dir;

		/* Skip empty or null values. */
		if (tracker_is_empty_string (tmp_list->data)) {
			continue;
		}

		dir = g_strdup (tmp_list->data);

		/* Wild-card comparisons. */
		if (g_strstr_len (dir, strlen (dir), "*") != NULL) {

			if (g_pattern_match_simple (dir, path) == TRUE) {

				results = TRUE;
				g_free (dir);
				break;
			}
		}
		/* Non-wild-card comparisons. */
		else {
			/* Add a trailing G_DIR_SEPARATOR. */
			if (g_str_has_suffix (dir, G_DIR_SEPARATOR_S) == FALSE) {

				gchar *tmp;

				tmp = dir;
				dir = g_strconcat (dir, G_DIR_SEPARATOR_S, NULL);
				g_free (tmp);
			}

			if (strcmp (path, dir) == 0) {

				results = TRUE;
				g_free (dir);
				break;
			}
		}
		g_free (dir);
	}

	for (tmp_list = exclude_path_list; tmp_list; tmp_list = tmp_list->next) {
		g_free (tmp_list->data);
	}
	g_slist_free (exclude_path_list);

	return results;
}

gboolean
is_second_scan_excluded_path (const gchar * path)
{
	GSList	   * exclude_path_list;
	GSList	   * tmp_list;
	gchar	   * dir;
	gboolean     results = FALSE;

	dir = g_strdup (path);

	/* Remove trailing G_DIR_SEPARATOR. */
	if ((strlen (dir) > 1) && (g_str_has_suffix (dir, G_DIR_SEPARATOR_S) == TRUE)) {
		dir[strlen (dir) - 1] = '\0';
	}

	/* Always exclude a path that is symbolic link. */
	if (g_file_test (dir, G_FILE_TEST_IS_SYMLINK)) {
		g_free (dir);

		return TRUE;
	}
	g_free (dir);

	/* Check path against the Quick-Search-Excluded-Paths list. */
	exclude_path_list = tracker_search_gconf_get_list ("/apps/tracker-search-tool/quick_search_second_scan_excluded_paths",
							GCONF_VALUE_STRING);

	for (tmp_list = exclude_path_list; tmp_list; tmp_list = tmp_list->next) {

		gchar * dir;

		/* Skip empty or null values. */
		if (tracker_is_empty_string (tmp_list->data)) {
			continue;
		}

		dir = g_strdup (tmp_list->data);

		/* Wild-card comparisons. */
		if (g_strstr_len (dir, strlen (dir), "*") != NULL) {

			if (g_pattern_match_simple (dir, path) == TRUE) {

				results = TRUE;
				g_free (dir);
				break;
			}
		}
		/* Non-wild-card comparisons. */
		else {
			/* Add a trailing G_DIR_SEPARATOR. */
			if (g_str_has_suffix (dir, G_DIR_SEPARATOR_S) == FALSE) {

				gchar *tmp;

				tmp = dir;
				dir = g_strconcat (dir, G_DIR_SEPARATOR_S, NULL);
				g_free (tmp);
			}

			if (strcmp (path, dir) == 0) {

				results = TRUE;
				g_free (dir);
				break;
			}
		}
		g_free (dir);
	}

	for (tmp_list = exclude_path_list; tmp_list; tmp_list = tmp_list->next) {
		g_free (tmp_list->data);
	}
	g_slist_free (exclude_path_list);

	return results;
}

gboolean
compare_regex (const gchar * regex,
	       const gchar * string)
{
	regex_t regexec_pattern;

	if (regex == NULL) {
		return TRUE;
	}

	if (!regcomp (&regexec_pattern, regex, REG_NOSUB)) {
		if (regexec (&regexec_pattern, string, 0, 0, 0) != REG_NOMATCH) {
			return TRUE;
		}
	}
	return FALSE;
}

gboolean
limit_string_to_x_lines (GString * string,
			 gint x)
{
	int i;
	int count = 0;
	for (i = 0; string->str[i] != '\0'; i++) {
		if (string->str[i] == '\n') {
			count++;
			if (count == x) {
				g_string_truncate (string, i);
				return TRUE;
			}
		}
	}
	return FALSE;
}

gchar *
tracker_string_replace (const gchar * haystack,
			gchar * needle,
			gchar * replacement)
{
	GString * str;
	gint pos, needle_len;

	g_return_val_if_fail (haystack && needle, NULL);

	needle_len = strlen (needle);

	str = g_string_new ("");

	for (pos = 0; haystack[pos]; pos++)
	{
		if (strncmp (&haystack[pos], needle, needle_len) == 0)
		{

			if (replacement) {
				str = g_string_append (str, replacement);
			}

			pos += needle_len - 1;

		} else {
			str = g_string_append_c (str, haystack[pos]);
		}
	}

	return g_string_free (str, FALSE);
}

static gint
count_of_char_in_string (const gchar * string,
			 const gchar c)
{
	int cnt = 0;
	for(; *string; string++) {
		if (*string == c) cnt++;
	}
	return cnt;
}

gchar *
escape_single_quotes (const gchar * string)
{
	GString * gs;

	if (string == NULL) {
		return NULL;
	}

	if (count_of_char_in_string (string, '\'') == 0) {
		return g_strdup(string);
	}
	gs = g_string_new ("");
	for(; *string; string++) {
		if (*string == '\'') {
			g_string_append(gs, "'\\''");
		}
		else {
			g_string_append_c(gs, *string);
		}
	}
	return g_string_free (gs, FALSE);
}

gchar *
backslash_special_characters (const gchar * string)
{
	GString * gs;

	if (string == NULL) {
		return NULL;
	}

	if ((count_of_char_in_string (string, '\\') == 0) &&
	    (count_of_char_in_string (string, '-') == 0)) {
		return g_strdup(string);
	}
	gs = g_string_new ("");
	for(; *string; string++) {
		if (*string == '\\') {
			g_string_append(gs, "\\\\");
		}
		else if (*string == '-') {
			g_string_append(gs, "\\-");
		}
		else {
			g_string_append_c(gs, *string);
		}
	}
	return g_string_free (gs, FALSE);
}

gchar *
remove_mnemonic_character (const gchar * string)
{
	GString * gs;
	gboolean first_mnemonic = TRUE;

	if (string == NULL) {
		return NULL;
	}

	gs = g_string_new ("");
	for(; *string; string++) {
		if ((first_mnemonic) && (*string == '_')) {
			first_mnemonic = FALSE;
			continue;
		}
		g_string_append_c(gs, *string);
	}
	return g_string_free (gs, FALSE);
}

gchar *
get_readable_date (const gchar * format_string,
		   const time_t file_time_raw)
{
	struct tm * file_time;
	gchar * format;
	GDate * today;
	GDate * file_date;
	guint32 file_date_age;
	gchar * readable_date;

	file_time = localtime (&file_time_raw);

	/* Base format of date column on nautilus date_format key */
	if (format_string != NULL) {
		if (strcmp(format_string, GSEARCH_DATE_FORMAT_LOCALE) == 0) {
			return tracker_search_strdup_strftime ("%c", file_time);
		} else if (strcmp (format_string, GSEARCH_DATE_FORMAT_ISO) == 0) {
			return tracker_search_strdup_strftime ("%Y-%m-%d %H:%M:%S", file_time);
		}
	}

	file_date = g_date_new_dmy (file_time->tm_mday,
			       file_time->tm_mon + 1,
			       file_time->tm_year + 1900);

	today = g_date_new ();
	g_date_set_time_t (today, time (NULL));

	file_date_age = g_date_get_julian (today) - g_date_get_julian (file_date);

	g_date_free (today);
	g_date_free (file_date);

	if (file_date_age == 0)	{
	/* Translators:  Below are the strings displayed in the 'Date Modified'
	   column of the list view.  The format of this string can vary depending
	   on age of a file.  Please modify the format of the timestamp to match
	   your locale.  For example, to display 24 hour time replace the '%-I'
	   with '%-H' and remove the '%p'.  (See bugzilla report #120434.) */
		format = g_strdup(_("today at %-I:%M %p"));
	} else if (file_date_age == 1) {
		format = g_strdup(_("yesterday at %-I:%M %p"));
	} else if (file_date_age < 7) {
		format = g_strdup(_("%A, %B %-d %Y at %-I:%M:%S %p"));
	} else {
		format = g_strdup(_("%A, %B %-d %Y at %-I:%M:%S %p"));
	}

	readable_date = tracker_search_strdup_strftime (format, file_time);
	g_free (format);

	return readable_date;
}

gchar *
tracker_search_strdup_strftime (const gchar * format,
				struct tm * time_pieces)
{
	/* This function is borrowed from eel's eel_strdup_strftime() */
	GString * string;
	const char * remainder, * percent;
	char code[4], buffer[512];
	char * piece, * result, * converted;
	size_t string_length;
	gboolean strip_leading_zeros, turn_leading_zeros_to_spaces;
	char modifier;
	int i;

	/* Format could be translated, and contain UTF-8 chars,
	 * so convert to locale encoding which strftime uses */
	converted = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
	g_return_val_if_fail (converted != NULL, NULL);

	string = g_string_new ("");
	remainder = converted;

	/* Walk from % character to % character. */
	for (;;) {
		percent = strchr (remainder, '%');
		if (percent == NULL) {
			g_string_append (string, remainder);
			break;
		}
		g_string_append_len (string, remainder,
				     percent - remainder);

		/* Handle the "%" character. */
		remainder = percent + 1;
		switch (*remainder) {
		case '-':
			strip_leading_zeros = TRUE;
			turn_leading_zeros_to_spaces = FALSE;
			remainder++;
			break;
		case '_':
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = TRUE;
			remainder++;
			break;
		case '%':
			g_string_append_c (string, '%');
			remainder++;
			continue;
		case '\0':
			g_warning ("Trailing %% passed to tracker_search_strdup_strftime");
			g_string_append_c (string, '%');
			continue;
		default:
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = FALSE;
			break;
		}

		modifier = 0;
		if (strchr (SUS_EXTENDED_STRFTIME_MODIFIERS, *remainder) != NULL) {
			modifier = *remainder;
			remainder++;

			if (*remainder == 0) {
				g_warning ("Unfinished %%%c modifier passed to tracker_search_strdup_strftime", modifier);
				break;
			}
		}

		if (strchr (C_STANDARD_STRFTIME_CHARACTERS, *remainder) == NULL) {
			g_warning ("tracker_search_strdup_strftime does not support "
				   "non-standard escape code %%%c",
				   *remainder);
		}

		/* Convert code to strftime format. We have a fixed
		 * limit here that each code can expand to a maximum
		 * of 512 bytes, which is probably OK. There's no
		 * limit on the total size of the result string.
		 */
		i = 0;
		code[i++] = '%';
		if (modifier != 0) {
#ifdef HAVE_STRFTIME_EXTENSION
			code[i++] = modifier;
#endif
		}
		code[i++] = *remainder;
		code[i++] = '\0';
		string_length = strftime (buffer, sizeof (buffer),
					  code, time_pieces);
		if (string_length == 0) {
			/* We could put a warning here, but there's no
			 * way to tell a successful conversion to
			 * empty string from a failure.
			 */
			buffer[0] = '\0';
		}

		/* Strip leading zeros if requested. */
		piece = buffer;
		if (strip_leading_zeros || turn_leading_zeros_to_spaces) {
			if (strchr (C_STANDARD_NUMERIC_STRFTIME_CHARACTERS, *remainder) == NULL) {
				g_warning ("tracker_search_strdup_strftime does not support "
					   "modifier for non-numeric escape code %%%c%c",
					   remainder[-1],
					   *remainder);
			}
			if (*piece == '0') {
				do {
					piece++;
				} while (*piece == '0');
				if (!g_ascii_isdigit (*piece)) {
				    piece--;
				}
			}
			if (turn_leading_zeros_to_spaces) {
				memset (buffer, ' ', piece - buffer);
				piece = buffer;
			}
		}
		remainder++;

		/* Add this piece. */
		g_string_append (string, piece);
	}

	/* Convert the string back into utf-8. */
	result = g_locale_to_utf8 (string->str, -1, NULL, NULL, NULL);

	g_string_free (string, TRUE);
	g_free (converted);

	return result;
}

gchar *
get_file_type_description (const gchar * file,
			   const char *mime,
			   GnomeVFSFileInfo * file_info)
{
	gchar * desc;

	if (file == NULL || mime == NULL) {
		return g_strdup (gnome_vfs_mime_get_description (GNOME_VFS_MIME_TYPE_UNKNOWN));
	}

	desc = g_strdup (gnome_vfs_mime_get_description (mime));

	if (file_info->symlink_name != NULL) {

		gchar * absolute_symlink = NULL;
		gchar * str = NULL;

		if (g_path_is_absolute (file_info->symlink_name) != TRUE) {
			gchar *dirname;

			dirname = g_path_get_dirname (file);
			absolute_symlink = g_strconcat (dirname, G_DIR_SEPARATOR_S, file_info->symlink_name, NULL);
			g_free (dirname);
		}
		else {
			absolute_symlink = g_strdup (file_info->symlink_name);
		}

		if (g_file_test (absolute_symlink, G_FILE_TEST_EXISTS) != TRUE) {
		       if ((g_ascii_strcasecmp (mime, "x-special/socket") != 0) &&
			   (g_ascii_strcasecmp (mime, "x-special/fifo") != 0)) {
				g_free (absolute_symlink);
				g_free (desc);
				return g_strdup (_("link (broken)"));
			}
		}

		str = g_strdup_printf (_("link to %s"), (desc != NULL) ? desc : mime);
		g_free (absolute_symlink);
		g_free (desc);
		return str;
	}
	return desc;
}


static GdkPixbuf *
tracker_search_load_thumbnail_frame (void)
{
	GdkPixbuf * pixbuf = NULL;
	gchar * image_path;

	image_path = tracker_search_pixmap_file ("thumbnail_frame.png");

	if (image_path != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);
	}
	g_free (image_path);
	return pixbuf;
}

static void
tracker_search_draw_frame_row (GdkPixbuf * frame_image,
			    gint target_width,
			    gint source_width,
			    gint source_v_position,
			    gint dest_v_position,
			    GdkPixbuf * result_pixbuf,
			    gint left_offset,
			    gint height)
{
	gint remaining_width;
	gint h_offset;
	gint slab_width;

	remaining_width = target_width;
	h_offset = 0;
	while (remaining_width > 0) {
		slab_width = remaining_width > source_width ? source_width : remaining_width;
		gdk_pixbuf_copy_area (frame_image, left_offset, source_v_position, slab_width,
				      height, result_pixbuf, left_offset + h_offset, dest_v_position);
		remaining_width -= slab_width;
		h_offset += slab_width;
	}
}

static void
tracker_search_draw_frame_column (GdkPixbuf * frame_image,
			       gint target_height,
			       gint source_height,
			       gint source_h_position,
			       gint dest_h_position,
			       GdkPixbuf * result_pixbuf,
			       gint top_offset,
			       gint width)
{
	gint remaining_height;
	gint v_offset;
	gint slab_height;

	remaining_height = target_height;
	v_offset = 0;
	while (remaining_height > 0) {
		slab_height = remaining_height > source_height ? source_height : remaining_height;
		gdk_pixbuf_copy_area (frame_image, source_h_position, top_offset, width, slab_height,
				      result_pixbuf, dest_h_position, top_offset + v_offset);
		remaining_height -= slab_height;
		v_offset += slab_height;
	}
}

static GdkPixbuf *
tracker_search_stretch_frame_image (GdkPixbuf *frame_image,
				 gint left_offset,
				 gint top_offset,
				 gint right_offset,
				 gint bottom_offset,
				 gint dest_width,
				 gint dest_height,
				 gboolean fill_flag)
{
	GdkPixbuf * result_pixbuf;
	guchar * pixels_ptr;
	gint frame_width, frame_height;
	gint y, row_stride;
	gint target_width, target_frame_width;
	gint target_height, target_frame_height;

	frame_width = gdk_pixbuf_get_width (frame_image);
	frame_height = gdk_pixbuf_get_height (frame_image);

	if (fill_flag) {
		result_pixbuf = gdk_pixbuf_scale_simple (frame_image, dest_width, dest_height, GDK_INTERP_NEAREST);
	} else {
		result_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, dest_width, dest_height);
	}
	row_stride = gdk_pixbuf_get_rowstride (result_pixbuf);
	pixels_ptr = gdk_pixbuf_get_pixels (result_pixbuf);

	/* clear the new pixbuf */
	if (fill_flag == FALSE) {
		for (y = 0; y < dest_height; y++) {
			art_rgb_run_alpha (pixels_ptr, 255, 255, 255, 255, dest_width);
			pixels_ptr += row_stride;
		}
	}

	target_width  = dest_width - left_offset - right_offset;
	target_frame_width = frame_width - left_offset - right_offset;

	target_height  = dest_height - top_offset - bottom_offset;
	target_frame_height = frame_height - top_offset - bottom_offset;

	/* Draw the left top corner  and top row */
	gdk_pixbuf_copy_area (frame_image, 0, 0, left_offset, top_offset, result_pixbuf, 0,  0);
	tracker_search_draw_frame_row (frame_image, target_width, target_frame_width, 0, 0,
				    result_pixbuf, left_offset, top_offset);

	/* Draw the right top corner and left column */
	gdk_pixbuf_copy_area (frame_image, frame_width - right_offset, 0, right_offset, top_offset,
			      result_pixbuf, dest_width - right_offset,  0);
	tracker_search_draw_frame_column (frame_image, target_height, target_frame_height, 0, 0,
				       result_pixbuf, top_offset, left_offset);

	/* Draw the bottom right corner and bottom row */
	gdk_pixbuf_copy_area (frame_image, frame_width - right_offset, frame_height - bottom_offset,
			      right_offset, bottom_offset, result_pixbuf, dest_width - right_offset,
			      dest_height - bottom_offset);
	tracker_search_draw_frame_row (frame_image, target_width, target_frame_width,
				    frame_height - bottom_offset, dest_height - bottom_offset,
				    result_pixbuf, left_offset, bottom_offset);

	/* Draw the bottom left corner and the right column */
	gdk_pixbuf_copy_area (frame_image, 0, frame_height - bottom_offset, left_offset, bottom_offset,
			      result_pixbuf, 0,  dest_height - bottom_offset);
	tracker_search_draw_frame_column (frame_image, target_height, target_frame_height,
				       frame_width - right_offset, dest_width - right_offset,
				       result_pixbuf, top_offset, right_offset);
	return result_pixbuf;
}

static GdkPixbuf *
tracker_search_embed_image_in_frame (GdkPixbuf * source_image,
				  GdkPixbuf * frame_image,
				  gint left_offset,
				  gint top_offset,
				  gint right_offset,
				  gint bottom_offset)
{
	GdkPixbuf * result_pixbuf;
	gint source_width, source_height;
	gint dest_width, dest_height;

	source_width = gdk_pixbuf_get_width (source_image);
	source_height = gdk_pixbuf_get_height (source_image);

	dest_width = source_width + left_offset + right_offset;
	dest_height = source_height + top_offset + bottom_offset;

	result_pixbuf = tracker_search_stretch_frame_image (frame_image, left_offset, top_offset, right_offset, bottom_offset,
							 dest_width, dest_height, FALSE);

	gdk_pixbuf_copy_area (source_image, 0, 0, source_width, source_height, result_pixbuf, left_offset, top_offset);

	return result_pixbuf;
}

static void
tracker_search_thumbnail_frame_image (GdkPixbuf ** pixbuf)
{
	GdkPixbuf * pixbuf_with_frame;
	GdkPixbuf * frame;

	frame = tracker_search_load_thumbnail_frame ();
	if (frame == NULL) {
		return;
	}

	pixbuf_with_frame = tracker_search_embed_image_in_frame (*pixbuf, frame, 3, 3, 6, 6);
	g_object_unref (*pixbuf);
	g_object_unref (frame);

	*pixbuf = pixbuf_with_frame;
}

static GdkPixbuf *
tracker_search_get_thumbnail_image (const gchar * file)
{
	GdkPixbuf * pixbuf = NULL;
	gchar * thumbnail_path;
	gchar * uri;

	uri = gnome_vfs_get_uri_from_local_path (file);
	thumbnail_path = gnome_thumbnail_path_for_uri (uri, GNOME_THUMBNAIL_SIZE_NORMAL);

	if (thumbnail_path != NULL) {
		if (g_file_test (thumbnail_path, G_FILE_TEST_EXISTS)) {

			GdkPixbuf * thumbnail_pixbuf = NULL;
			gfloat scale_factor_x = 1.0;
			gfloat scale_factor_y = 1.0;
			gint scale_x;
			gint scale_y;

			thumbnail_pixbuf = gdk_pixbuf_new_from_file (thumbnail_path, NULL);
			tracker_search_thumbnail_frame_image (&thumbnail_pixbuf);

			if (gdk_pixbuf_get_width (thumbnail_pixbuf) > ICON_SIZE) {
				scale_factor_x = (gfloat) ICON_SIZE / (gfloat) gdk_pixbuf_get_width (thumbnail_pixbuf);
			}
			if (gdk_pixbuf_get_height (thumbnail_pixbuf) > ICON_SIZE) {
				scale_factor_y = (gfloat) ICON_SIZE / (gfloat) gdk_pixbuf_get_height (thumbnail_pixbuf);
			}

			if (gdk_pixbuf_get_width (thumbnail_pixbuf) > gdk_pixbuf_get_height (thumbnail_pixbuf)) {
				scale_x = ICON_SIZE;
				scale_y = (gint) (gdk_pixbuf_get_height (thumbnail_pixbuf) * scale_factor_x);
			}
			else {
				scale_x = (gint) (gdk_pixbuf_get_width (thumbnail_pixbuf) * scale_factor_y);
				scale_y = ICON_SIZE;
			}

			pixbuf = gdk_pixbuf_scale_simple (thumbnail_pixbuf, scale_x, scale_y, GDK_INTERP_BILINEAR);
			g_object_unref (thumbnail_pixbuf);
		}
		g_free (thumbnail_path);
	}
	g_free (uri);
	return pixbuf;
}

static gchar *
tracker_search_icon_lookup (GSearchWindow * gsearch,
			    const gchar * file,
			    const gchar * mime,
			    GnomeVFSFileInfo * file_info,
			    gboolean enable_thumbnails)
{
	GnomeIconLookupFlags lookup_flags = GNOME_ICON_LOOKUP_FLAGS_NONE;
	gchar * icon_name = NULL;
	gchar * uri;

	uri = gnome_vfs_get_uri_from_local_path (file);

	if ((strncmp (mime, "image/", 6) != 0) ||
	    ((int)file_info->size < (int)gsearch->show_thumbnails_file_size_limit)) {
		if (gsearch->thumbnail_factory == NULL) {
			gsearch->thumbnail_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
		}
		lookup_flags = GNOME_ICON_LOOKUP_FLAGS_SHOW_SMALL_IMAGES_AS_THEMSELVES |
			       GNOME_ICON_LOOKUP_FLAGS_ALLOW_SVG_AS_THEMSELVES;
	}

	icon_name = gnome_icon_lookup (gtk_icon_theme_get_default (),
				       gsearch->thumbnail_factory,
				       uri,
				       NULL,
				       file_info,
				       mime,
				       lookup_flags,
				       NULL);
	g_free (uri);
	return icon_name;
}

GdkPixbuf *
get_file_pixbuf (GSearchWindow * gsearch,
		 const gchar * file,
		 const char *mime,
		 GnomeVFSFileInfo * file_info)
{
	GdkPixbuf * pixbuf = NULL;
	gchar * icon_name = NULL;

	if (file == NULL || mime == NULL) {
		icon_name = g_strdup (ICON_THEME_REGULAR_ICON);
	}

	else if ((g_file_test (file, G_FILE_TEST_IS_EXECUTABLE)) &&
		 (g_ascii_strcasecmp (mime, "application/x-executable-binary") == 0)) {
		icon_name = g_strdup (ICON_THEME_EXECUTABLE_ICON);
	}
	else if (g_ascii_strcasecmp (mime, "x-special/device-char") == 0) {
		icon_name = g_strdup (ICON_THEME_CHAR_DEVICE);
	}
	else if (g_ascii_strcasecmp (mime, "x-special/device-block") == 0) {
		icon_name = g_strdup (ICON_THEME_BLOCK_DEVICE);
	}
	else if (g_ascii_strcasecmp (mime, "x-special/socket") == 0) {
		icon_name = g_strdup (ICON_THEME_SOCKET);
	}
	else if (g_ascii_strcasecmp (mime, "x-special/fifo") == 0) {
		icon_name = g_strdup (ICON_THEME_FIFO);
	} else {

		/* check for thumbnail first */
		GdkPixbuf *thumbnail_pixbuf = tracker_search_get_thumbnail_image (file);

		if (thumbnail_pixbuf != NULL) {

			if ((gdk_pixbuf_get_width (thumbnail_pixbuf) > ICON_SIZE) ||
			    (gdk_pixbuf_get_height (thumbnail_pixbuf) > ICON_SIZE)) {

				gfloat scale_factor_x = 1.0;
				gfloat scale_factor_y = 1.0;
				gint scale_x;
				gint scale_y;

				if (gdk_pixbuf_get_width (thumbnail_pixbuf) > ICON_SIZE) {
					scale_factor_x = (gfloat) ICON_SIZE / (gfloat) gdk_pixbuf_get_width (thumbnail_pixbuf);
				}
				if (gdk_pixbuf_get_height (thumbnail_pixbuf) > ICON_SIZE) {
					scale_factor_y = (gfloat) ICON_SIZE / (gfloat) gdk_pixbuf_get_height (thumbnail_pixbuf);
				}

				if (gdk_pixbuf_get_width (thumbnail_pixbuf) > gdk_pixbuf_get_height (thumbnail_pixbuf)) {
					scale_x = ICON_SIZE;
					scale_y = (gint) (gdk_pixbuf_get_height (thumbnail_pixbuf) * scale_factor_x);
				}
				else {
					scale_x = (gint) (gdk_pixbuf_get_width (thumbnail_pixbuf) * scale_factor_y);
					scale_y = ICON_SIZE;
				}

				pixbuf = gdk_pixbuf_scale_simple (thumbnail_pixbuf, scale_x, scale_y, GDK_INTERP_BILINEAR);
				g_object_unref (thumbnail_pixbuf);

				return pixbuf;

			} else {
				return thumbnail_pixbuf;
			}

		}

		/* check if image can be generated from file */

		if ((strncmp (mime, "image/", 6) == 0)) {
			pixbuf = gdk_pixbuf_new_from_file_at_scale (file, ICON_SIZE, ICON_SIZE, TRUE, NULL);
		}

		if (pixbuf) {
			return pixbuf;
		} else {
			icon_name = tracker_search_icon_lookup (gsearch, file, mime, file_info, TRUE);
		}


	}

	pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), icon_name, ICON_SIZE, 0, NULL);

	if (!pixbuf) {
		gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), ICON_THEME_REGULAR_ICON, ICON_SIZE, 0, NULL);
	}


	g_free (icon_name);


	return pixbuf;
}

gboolean
open_file_with_xdg_open (GtkWidget * window,
			 const gchar * file)
{
	gboolean  result;
	gchar	  *quoted_filename = g_shell_quote (file);
	gchar	  *command = g_strconcat ("xdg-open ", quoted_filename, NULL);

	g_free (quoted_filename);
	result = g_spawn_command_line_async (command, NULL);
	g_free (command);

	return result;
}

gboolean
open_file_with_nautilus (GtkWidget * window,
			 const gchar * file)
{
	GnomeDesktopItem * ditem;
	GdkScreen * screen;
	GError *error = NULL;
	gchar * command;
	gchar * contents;
	gchar * escaped;

	escaped = g_shell_quote (file);

	command = g_strconcat ("nautilus ",
			       "--sm-disable ",
			       "--no-desktop ",
			       "--no-default-window ",
			       escaped,
			       NULL);

	contents = g_strdup_printf ("[Desktop Entry]\n"
				    "Name=Nautilus\n"
				    "Icon=file-manager\n"
				    "Exec=%s\n"
				    "Terminal=false\n"
				    "StartupNotify=true\n"
				    "Type=Application\n",
				    command);

	ditem = gnome_desktop_item_new_from_string (NULL,
						    contents,
						    strlen (contents),
						    GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS ,
						    NULL);

	if (ditem == NULL) {
		g_free (contents);
		g_free (command);
		g_free (escaped);
		return FALSE;
	}

	screen = gtk_widget_get_screen (window);

	gnome_desktop_item_set_launch_time (ditem,
					    gtk_get_current_event_time ());

	gnome_desktop_item_launch_on_screen (ditem, NULL,
					     GNOME_DESKTOP_ITEM_LAUNCH_ONLY_ONE,
					     screen, -1, &error);

	gnome_desktop_item_unref (ditem);
	g_free (contents);
	g_free (command);
	g_free (escaped);

	if (error) {
		g_error_free (error);
		return FALSE;
	}
	return TRUE;
}

gboolean
open_file_with_application (GtkWidget * window,
			    const gchar * file)
{
	GnomeVFSMimeApplication * application;
	const char * mime;

	mime = gnome_vfs_get_file_mime_type (file, NULL, FALSE);
	application = gnome_vfs_mime_get_default_application (mime);

	if (!g_file_test (file, G_FILE_TEST_IS_DIR)) {
		if (application) {
			const char *desktop_file;
			GnomeDesktopItem *ditem;
			GdkScreen *screen;
			GError *error = NULL;
			GList *uris = NULL;
			gboolean result;
			char *uri;

			desktop_file = gnome_vfs_mime_application_get_desktop_file_path (application);

			uri = gnome_vfs_get_uri_from_local_path (file);
			uris = g_list_append (uris, uri);

			if (!g_file_test (desktop_file, G_FILE_TEST_EXISTS)) {
				result = (gnome_vfs_mime_application_launch (application, uris) == GNOME_VFS_OK);
			}
			else {
				result = TRUE;
				ditem = gnome_desktop_item_new_from_file (desktop_file, 0, &error);
				if (error) {
					result = FALSE;
					g_error_free (error);
				}
				else {
					screen = gtk_widget_get_screen (window);
					gnome_desktop_item_set_launch_time (ditem, gtk_get_current_event_time ());
					gnome_desktop_item_launch_on_screen (ditem, uris,
						GNOME_DESKTOP_ITEM_LAUNCH_APPEND_PATHS, screen, -1, &error);
					if (error) {
						result = FALSE;
						g_error_free (error);
					}
				}
				gnome_desktop_item_unref (ditem);
			}
			gnome_vfs_mime_application_free (application);
			g_list_free (uris);
			g_free (uri);

			return result;
		}
	}
	return FALSE;
}

gboolean
launch_file (const gchar * file)
{
	const char * mime = gnome_vfs_get_file_mime_type (file, NULL, FALSE);
	gboolean result = FALSE;

	if ((g_file_test (file, G_FILE_TEST_IS_EXECUTABLE)) &&
	    (g_ascii_strcasecmp (mime, BINARY_EXEC_MIME_TYPE) == 0)) {
		result = g_spawn_command_line_async (file, NULL);
	}

	return result;
}

gchar *
tracker_search_get_unique_filename (const gchar * path,
				 const gchar * suffix)
{
	const gint num_of_words = 12;
	gchar	  * words[] = {
		    "foo",
		    "bar",
		    "blah",
		    "cranston",
		    "frobate",
		    "hadjaha",
		    "greasy",
		    "hammer",
		    "eek",
		    "larry",
		    "curly",
		    "moe",
		    NULL};
	gchar * retval = NULL;
	gboolean exists = TRUE;

	while (exists) {
		gchar * file;
		gint rnd;
		gint word;

		rnd = rand ();
		word = rand () % num_of_words;

		file = g_strdup_printf ("%s-%010x%s",
					    words [word],
					    (guint) rnd,
					    suffix);

		g_free (retval);
		retval = g_strconcat (path, G_DIR_SEPARATOR_S, file, NULL);
		exists = g_file_test (retval, G_FILE_TEST_EXISTS);
		g_free (file);
	}
	return retval;
}

GtkWidget *
tracker_search_button_new_with_stock_icon (const gchar * string,
					const gchar * stock_id)
{
	GtkWidget * align;
	GtkWidget * button;
	GtkWidget * hbox;
	GtkWidget * image;
	GtkWidget * label;

	button = gtk_button_new ();
	label = gtk_label_new_with_mnemonic (string);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (button));
	image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
	hbox = gtk_hbox_new (FALSE, 2);
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (button), align);
	gtk_container_add (GTK_CONTAINER (align), hbox);
	gtk_widget_show_all (align);

	return button;
}

GSList *
tracker_search_get_columns_order (GtkTreeView * treeview)
{
	GSList *order = NULL;
	GList * columns;
	GList * col;

	columns = gtk_tree_view_get_columns (treeview);

	for (col = columns; col; col = col->next) {
		gint id;

		id = gtk_tree_view_column_get_sort_column_id (col->data);
		order = g_slist_prepend (order, GINT_TO_POINTER (id));
	}
	g_list_free (columns);

	order = g_slist_reverse (order);
	return order;
}

static GtkTreeViewColumn *
get_column_with_sort_column_id (GtkTreeView * treeview,
				gint id)
{
	GtkTreeViewColumn * col = NULL;
	GList * columns;
	GList * it;

	columns = gtk_tree_view_get_columns (treeview);

	for (it = columns; it; it = it->next) {
		if (gtk_tree_view_column_get_sort_column_id (it->data) == id) {
			col = it->data;
			break;
		}
	}
	g_list_free (columns);
	return col;
}

void
tracker_search_set_columns_order (GtkTreeView * treeview)
{
	GtkTreeViewColumn * last = NULL;
	GSList * order;
	GSList * it;

	order = tracker_search_gconf_get_list ("/apps/tracker-search-tool/columns_order", GCONF_VALUE_INT);

	for (it = order; it; it = it->next) {

		GtkTreeViewColumn * cur;
		gint id;

		id = GPOINTER_TO_INT (it->data);

		if (id >= 0 && id < NUM_COLUMNS) {

			cur = get_column_with_sort_column_id (treeview, id);

			if (cur && cur != last) {
				gtk_tree_view_move_column_after (treeview, cur, last);
				last = cur;
			}
		}
	}
	g_slist_free (order);
}

gint
tracker_get_stored_separator_position (void)
{
	gint saved_pos;

	saved_pos = tracker_search_gconf_get_int ("/apps/tracker-search-tool/separator_position");

	if (saved_pos < 1) {
		return DEFAULT_SEPARATOR_POSITION;
	}

	return saved_pos;
}

void
tracker_set_stored_separator_position (gint pos)
{
	tracker_search_gconf_set_int ("/apps/tracker-search-tool/separator_position", pos);
}

void
tracker_search_get_stored_window_geometry (gint * width,
					   gint * height)
{
	gint saved_width;
	gint saved_height;

	if (width == NULL || height == NULL) {
		return;
	}

	saved_width = tracker_search_gconf_get_int ("/apps/tracker-search-tool/default_window_width");
	saved_height = tracker_search_gconf_get_int ("/apps/tracker-search-tool/default_window_height");

	if (saved_width < 1) {
		saved_width = DEFAULT_WINDOW_WIDTH;
	}

	if (saved_height < 1) {
		saved_height = DEFAULT_WINDOW_HEIGHT;
	}

	*width = saved_width;
	*height = saved_height;
}

void
tracker_set_atk_relationship(GtkWidget *obj1, int relation_type,
			     GtkWidget *obj2)
{
	AtkObject *atk_obj1, *atk_obj2, *targets[1];
	AtkRelationSet *atk_rel_set;
	AtkRelation *atk_rel;

	atk_obj1 = gtk_widget_get_accessible (GTK_WIDGET (obj1));
	atk_obj2 = gtk_widget_get_accessible (GTK_WIDGET (obj2));
	atk_rel_set = atk_object_ref_relation_set (atk_obj1);
	targets[0] = atk_obj2;
	atk_rel = atk_relation_new (targets, 1, relation_type);
	atk_relation_set_add (atk_rel_set, atk_rel);
	g_object_unref (G_OBJECT (atk_rel));
}
