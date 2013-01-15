/*
 * Copyright (C) 2011, ARQ Media <sam.thursfield@codethink.co.uk>
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
 * Author: Sam Thursfield <sam.thursfield@codethink.co.uk>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

#if defined(HAVE_LIBCUE)
#include <libcue/libcue.h>
#endif

#include <libtracker-common/tracker-file-utils.h>

#include "tracker-cue-sheet.h"

#if defined(HAVE_LIBCUE)

static TrackerToc *
tracker_toc_new (void)
{
	TrackerToc *toc;

	toc = g_slice_new (TrackerToc);
	toc->tag_list = gst_tag_list_new (NULL);
	toc->entry_list = NULL;

	return toc;
}

#endif /* HAVE_LIBCUE */

void
tracker_toc_free (TrackerToc *toc)
{
	TrackerTocEntry *entry;
	GList *n;

	if (!toc) {
		return;
	}

	for (n = toc->entry_list; n != NULL; n = n->next) {
		entry = n->data;
		gst_tag_list_free (entry->tag_list);
		g_slice_free (TrackerTocEntry, entry);
	}

	g_list_free (toc->entry_list);

	g_slice_free (TrackerToc, toc);
}

#if defined(HAVE_LIBCUE)

static void
add_cdtext_string_tag (Cdtext      *cd_text,
                       enum Pti     index,
                       GstTagList  *tag_list,
                       const gchar *tag)
{
	const gchar *text;

	text = cdtext_get (index, cd_text);

	if (text != NULL) {
		gst_tag_list_add (tag_list, GST_TAG_MERGE_REPLACE, tag, text, NULL);
	}
}

static void
add_cdtext_comment_date_tag (Rem         *cd_comments,
                             enum Cmt     index,
                             GstTagList  *tag_list,
                             const gchar *tag)
{
	const gchar *text;
	gint year;
	GDate *date;

	text = rem_get (index, cd_comments);

	if (text != NULL) {
		year = atoi (text);

		if (year >= 1860) {
			date = g_date_new_dmy (1, 1, year);
			gst_tag_list_add (tag_list, GST_TAG_MERGE_REPLACE, tag, date, NULL);
			g_date_free (date);
		}
	}
}

static void
add_cdtext_comment_double_tag (Rem         *cd_comments,
                               enum Cmt     index,
                               GstTagList  *tag_list,
                               const gchar *tag)
{
	const gchar *text;
	gdouble value;

	text = rem_get (index, cd_comments);

	if (text != NULL) {
		value = strtod (text, NULL);

		/* Shortcut: it just so happens that 0.0 is meaningless for the replay
		 * gain properties so we can get away with testing for errors this way.
		 */
		if (value != 0.0)
			gst_tag_list_add (tag_list, GST_TAG_MERGE_REPLACE, tag, value, NULL);
	}
}

static void
set_album_tags_from_cdtext (GstTagList *tag_list,
                            Cdtext     *cd_text,
                            Rem        *cd_comments)
{
	if (cd_text != NULL) {
		add_cdtext_string_tag (cd_text, PTI_TITLE, tag_list, GST_TAG_ALBUM);
		add_cdtext_string_tag (cd_text, PTI_PERFORMER, tag_list, GST_TAG_ALBUM_ARTIST);
	}

	if (cd_comments != NULL) {
		add_cdtext_comment_date_tag (cd_comments, REM_DATE, tag_list, GST_TAG_DATE);

		add_cdtext_comment_double_tag (cd_comments, REM_REPLAYGAIN_ALBUM_GAIN, tag_list, GST_TAG_ALBUM_GAIN);
		add_cdtext_comment_double_tag (cd_comments, REM_REPLAYGAIN_ALBUM_PEAK, tag_list, GST_TAG_ALBUM_PEAK);
	}
}

static void
set_track_tags_from_cdtext (GstTagList *tag_list,
                            Cdtext     *cd_text,
                            Rem        *cd_comments)
{
	if (cd_text != NULL) {
		add_cdtext_string_tag (cd_text, PTI_TITLE, tag_list, GST_TAG_TITLE);
		add_cdtext_string_tag (cd_text, PTI_PERFORMER, tag_list, GST_TAG_PERFORMER);
		add_cdtext_string_tag (cd_text, PTI_COMPOSER, tag_list, GST_TAG_COMPOSER);
	}

	if (cd_comments != NULL) {
		add_cdtext_comment_double_tag (cd_comments, REM_REPLAYGAIN_TRACK_GAIN, tag_list, GST_TAG_TRACK_GAIN);
		add_cdtext_comment_double_tag (cd_comments, REM_REPLAYGAIN_TRACK_PEAK, tag_list, GST_TAG_TRACK_PEAK);
	}
}

/* Some simple heuristics to fill in missing tag information. */
static void
process_toc_tags (TrackerToc *toc)
{
	GList *node;
	gint track_count;

	gchar *album_artist = NULL;

	if (gst_tag_list_get_tag_size (toc->tag_list, GST_TAG_TRACK_COUNT) == 0) {
		track_count = g_list_length (toc->entry_list);
		gst_tag_list_add (toc->tag_list,
		                  GST_TAG_MERGE_REPLACE,
		                  GST_TAG_TRACK_COUNT,
		                  track_count,
		                  NULL);
	}

	gst_tag_list_get_string (toc->tag_list, GST_TAG_ALBUM_ARTIST, &album_artist);

	for (node = toc->entry_list; node; node = node->next) {
		TrackerTocEntry *entry = node->data;

		if (album_artist != NULL) {
			if (gst_tag_list_get_tag_size (entry->tag_list, GST_TAG_ARTIST) == 0 &&
			    gst_tag_list_get_tag_size (entry->tag_list, GST_TAG_PERFORMER) == 0)
				gst_tag_list_add (entry->tag_list,
				                  GST_TAG_MERGE_REPLACE,
				                  GST_TAG_ARTIST,
				                  album_artist,
				                  NULL);
		}
	}

	g_free (album_artist);
}

/* This function runs in two modes: for external CUE sheets, it will check
 * the FILE field for each track and build a TrackerToc for all the tracks
 * contained in @file_name. If @file_name does not appear in the CUE sheet,
 * %NULL will be returned. For embedded CUE sheets, @file_name will be NULL
 * the whole TOC will be returned regardless of any FILE information.
 */
static TrackerToc *
parse_cue_sheet_for_file (const gchar *cue_sheet,
                          const gchar *file_name)
{
	TrackerToc *toc;
	TrackerTocEntry *toc_entry;
	Cd *cd;
	Track *track;
	gint i;

	toc = NULL;

	cd = cue_parse_string (cue_sheet);

	if (cd == NULL) {
		g_debug ("Unable to parse CUE sheet for %s.",
		         file_name ? file_name : "(embedded in FLAC)");
		return NULL;
	}

	for (i = 1; i <= cd_get_ntrack (cd); i++) {
		track = cd_get_track (cd, i);

		/* CUE sheets generally have the correct basename but wrong
		 * extension in the FILE field, so this is what we test for.
		 */
		if (file_name != NULL) {
			if (!tracker_filename_casecmp_without_extension (file_name,
			                                                 track_get_filename (track))) {
				continue;
			}
		}

		if (track_get_mode (track) != MODE_AUDIO)
			continue;

		if (toc == NULL) {
			toc = tracker_toc_new ();

			set_album_tags_from_cdtext (toc->tag_list,
			                            cd_get_cdtext (cd),
			                            cd_get_rem (cd));
		}

		toc_entry = g_slice_new (TrackerTocEntry);
		toc_entry->tag_list = gst_tag_list_new (NULL);
		toc_entry->start = track_get_start (track) / 75.0;
		toc_entry->duration = track_get_length (track) / 75.0;

		set_track_tags_from_cdtext (toc_entry->tag_list,
		                            track_get_cdtext (track),
		                            track_get_rem (track));

		gst_tag_list_add (toc_entry->tag_list,
		                  GST_TAG_MERGE_REPLACE,
		                  GST_TAG_TRACK_NUMBER,
		                  i,
		                  NULL);


		toc->entry_list = g_list_prepend (toc->entry_list, toc_entry);
	}

	cd_delete (cd);

	if (toc != NULL)
		toc->entry_list = g_list_reverse (toc->entry_list);

	return toc;
}

TrackerToc *
tracker_cue_sheet_parse (const gchar *cue_sheet)
{
	TrackerToc *result;

	result = parse_cue_sheet_for_file (cue_sheet, NULL);

	if (result)
		process_toc_tags (result);

	return result;
}

static GList *
find_local_cue_sheets (GFile *audio_file)
{
	GFile *container;
	GFile *cue_sheet;
	GFileEnumerator *e;
	GFileInfo *file_info;
	gchar *container_path;
	const gchar *file_name;
	const gchar *file_content_type;
	gchar *file_path;
	GList *result = NULL;
	GError *error = NULL;

	container = g_file_get_parent (audio_file);
	container_path = g_file_get_path (container);

	e = g_file_enumerate_children (container,
	                               "standard::*",
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL,
	                               &error);

	if (error != NULL) {
		g_debug ("Unable to enumerate directory: %s", error->message);
		g_object_unref (container);
		g_error_free (error);
		return NULL;
	}

	while ((file_info = g_file_enumerator_next_file (e, NULL, NULL))) {
		file_name = g_file_info_get_attribute_byte_string (file_info,
		                                                   G_FILE_ATTRIBUTE_STANDARD_NAME);

		file_content_type = g_file_info_get_content_type (file_info);

		if (file_name == NULL || file_content_type == NULL) {
			g_debug ("Unable to get info for file %s/%s",
			         container_path,
			         g_file_info_get_display_name (file_info));
		} else if (strcmp (file_content_type, "application/x-cue") == 0) {
			file_path = g_build_filename (container_path, file_name, NULL);
			cue_sheet = g_file_new_for_path (file_path);
			result = g_list_prepend (result, cue_sheet);
			g_free (file_path);
		}

		g_object_unref (file_info);
	}

	g_object_unref (e);
	g_object_unref (container);
	g_free (container_path);

	return result;
}

TrackerToc *
tracker_cue_sheet_parse_uri (const gchar *uri)
{
	GFile *audio_file;
	gchar *audio_file_name;
	GList *cue_sheet_list;
	TrackerToc *toc;
	GError *error = NULL;
	GList *n;

	audio_file = g_file_new_for_uri (uri);
	audio_file_name = g_file_get_basename (audio_file);

	cue_sheet_list = find_local_cue_sheets (audio_file);

	toc = NULL;

	for (n = cue_sheet_list; n != NULL; n = n->next) {
		GFile *cue_sheet_file;
		gchar *buffer;

		cue_sheet_file = n->data;

		g_file_load_contents (cue_sheet_file, NULL, &buffer, NULL, NULL, &error);

		if (error != NULL) {
			g_debug ("Unable to read cue sheet: %s", error->message);
			g_error_free (error);
			continue;
		}

		toc = parse_cue_sheet_for_file (buffer, audio_file_name);

		g_free (buffer);

		if (toc != NULL) {
			char *path = g_file_get_path (cue_sheet_file);
			g_debug ("Using external CUE sheet: %s", path);
			g_free (path);
			break;
		}
	}

	g_list_foreach (cue_sheet_list, (GFunc) g_object_unref, NULL);
	g_list_free (cue_sheet_list);

	g_object_unref (audio_file);
	g_free (audio_file_name);

	if (toc)
		process_toc_tags (toc);

	return toc;
}

#else  /* ! HAVE_LIBCUE */

TrackerToc *
tracker_cue_sheet_parse (const gchar *cue_sheet)
{
	return NULL;
}

TrackerToc *
tracker_cue_sheet_parse_uri (const gchar *uri)
{
	return NULL;
}

#endif /* ! HAVE_LIBCUE */
