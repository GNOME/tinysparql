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
 *
 * Author: Martyn Russell <martyn@lanedo.com>
 */

#include "config.h"

#include <libxml/HTMLparser.h>

#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-common/tracker-locale.h>

#include "tracker-miner-userguides.h"

/* FIXME: Should we rename this to just -locale not -applications-locale ? */
#include "tracker-miner-applications-locale.h"

// FIXME: get this value from tracker conf
#define MAX_EXTRACT_SIZE 1024 * 1024 // 1 MiB
#define MAX_TITLE_LENGTH 1000

typedef struct {
	gchar *uri;
	GString *title;
	gboolean in_text;
	gboolean in_title;
	GString *plain_text;
	gssize max_length;
} ParserContext;

typedef struct {
	TrackerMinerFS *miner;
	GFile *file;
	TrackerSparqlBuilder *sparql;
	GCancellable *cancellable;
	gchar *type;
} ProcessUserguideData;

static void     miner_userguides_initable_iface_init     (GInitableIface       *iface);
static gboolean miner_userguides_initable_init           (GInitable            *initable,
                                                          GCancellable         *cancellable,
                                                          GError              **error);
static gboolean miner_userguides_check_file              (TrackerMinerFS       *fs,
                                                          GFile                *file);
static gboolean miner_userguides_check_directory         (TrackerMinerFS       *fs,
                                                          GFile                *file);
static gboolean miner_userguides_process_file            (TrackerMinerFS       *fs,
                                                          GFile                *file,
                                                          TrackerSparqlBuilder *sparql,
                                                          GCancellable         *cancellable);
static gboolean miner_userguides_process_file_attributes (TrackerMinerFS       *fs,
                                                          GFile                *file,
                                                          TrackerSparqlBuilder *sparql,
                                                          GCancellable         *cancellable);
static gboolean miner_userguides_monitor_directory       (TrackerMinerFS       *fs,
                                                          GFile                *file);
static void     miner_userguides_finalize                (GObject              *object);
static void     parser_get_file_content                  (const gchar          *uri,
                                                          gssize                max_extract_size,
                                                          gchar               **content,
                                                          gchar               **title);

static GQuark miner_userguides_error_quark = 0;

static GInitableIface* miner_userguides_initable_parent_iface;

G_DEFINE_TYPE_WITH_CODE (TrackerMinerUserguides, tracker_miner_userguides, TRACKER_TYPE_MINER_FS,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                miner_userguides_initable_iface_init));

static void
tracker_miner_userguides_class_init (TrackerMinerUserguidesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerFSClass *miner_fs_class = TRACKER_MINER_FS_CLASS (klass);

	object_class->finalize = miner_userguides_finalize;

	miner_fs_class->check_file = miner_userguides_check_file;
	miner_fs_class->check_directory = miner_userguides_check_directory;
	miner_fs_class->monitor_directory = miner_userguides_monitor_directory;
	miner_fs_class->process_file = miner_userguides_process_file;
	miner_fs_class->process_file_attributes = miner_userguides_process_file_attributes;

	miner_userguides_error_quark = g_quark_from_static_string ("TrackerMinerUserguides");
}

static void
tracker_miner_userguides_init (TrackerMinerUserguides *ma)
{
}

static void
miner_userguides_initable_iface_init (GInitableIface *iface)
{
	miner_userguides_initable_parent_iface = g_type_interface_peek_parent (iface);
	iface->init = miner_userguides_initable_init;
}

static void
miner_userguides_basedir_add (TrackerMinerFS *fs,
                              const gchar    *basedir)
{
	GFile *file;
	gchar *path;

	/* Add $dir/userguide/contents */
	path = g_build_filename (basedir, "userguide", "contents", NULL);
	file = g_file_new_for_path (path);
	g_message ("  Adding:'%s'", path);
	tracker_miner_fs_directory_add (fs, file, TRUE);
	g_object_unref (file);
	g_free (path);
}

static void
miner_userguides_add_directories (TrackerMinerFS *fs)
{
	const gchar * const *xdg_dirs;
	gint i;

	g_message ("Setting up userguides to iterate from XDG system directories");

	/* Add all XDG system and local dirs */
	xdg_dirs = g_get_system_data_dirs ();

	for (i = 0; xdg_dirs[i]; i++) {
		miner_userguides_basedir_add (fs, xdg_dirs[i]);
	}
}

static void
tracker_locale_notify_cb (TrackerLocaleID id,
                          gpointer        user_data)
{
	TrackerMiner *miner = user_data;

	if (tracker_miner_userguides_detect_locale_changed (miner)) {
		tracker_miner_fs_set_mtime_checking (TRACKER_MINER_FS (miner), TRUE);

		miner_userguides_add_directories (TRACKER_MINER_FS (miner));
	}
}

static void
miner_finished_cb (TrackerMinerFS *fs,
                   gdouble         seconds_elapsed,
                   guint           total_directories_found,
                   guint           total_directories_ignored,
                   guint           total_files_found,
                   guint           total_files_ignored,
                   gpointer        user_data)
{
	/* Update locale file if necessary */
	if (tracker_miner_applications_locale_changed ()) {
		tracker_miner_applications_locale_set_current ();
	}
}

static gboolean
miner_userguides_initable_init (GInitable     *initable,
                                GCancellable  *cancellable,
                                GError       **error)
{
	TrackerMinerFS *fs;
	TrackerMinerUserguides *app;
	GError *inner_error = NULL;

	fs = TRACKER_MINER_FS (initable);
	app = TRACKER_MINER_USERGUIDES (initable);

	/* Chain up parent's initable callback before calling child's one */
	if (!miner_userguides_initable_parent_iface->init (initable, cancellable, &inner_error)) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	g_signal_connect (fs, "finished",
	                  G_CALLBACK (miner_finished_cb),
	                  NULL);

	miner_userguides_add_directories (fs);

	app->locale_notification_id = tracker_locale_notify_add (TRACKER_LOCALE_LANGUAGE,
	                                                         tracker_locale_notify_cb,
	                                                         app,
	                                                         NULL);

	return TRUE;
}

static void
miner_userguides_finalize (GObject *object)
{
	TrackerMinerUserguides *app;

	app = TRACKER_MINER_USERGUIDES (object);

	tracker_locale_notify_remove (app->locale_notification_id);

	G_OBJECT_CLASS (tracker_miner_userguides_parent_class)->finalize (object);
}

static gboolean
miner_userguides_check_file (TrackerMinerFS *fs,
                             GFile          *file)
{
	gboolean retval = FALSE;
	gchar *basename;

	basename = g_file_get_basename (file);

	/* FIXME: What do we ignore and what don't we? */
	if (g_str_has_suffix (basename, ".html")) {
		retval = TRUE;
	}

	/* FIXME: Do we check the mime type is 'application/x-userguide-html' */

	g_free (basename);

	return retval;
}

static gboolean
miner_userguides_check_directory (TrackerMinerFS *fs,
                                  GFile          *file)
{
	gboolean retval = TRUE;
	gchar *basename;

	/* We want to inspect all the passed dirs and their children except one:
	 * $prefix/userguide/contents/images/
	 */
	basename = g_file_get_basename (file);

	/* FIXME: Perhaps this is too broad? */
	if (strcmp (basename, "images") == 0) {
		g_message ("  Ignoring:'%s'", basename);
		retval = FALSE;
	}

	g_free (basename);

	return retval;
}

static gboolean
miner_userguides_monitor_directory (TrackerMinerFS *fs,
                                    GFile          *file)
{
	/* We want to monitor all the passed dirs and their children */
	return TRUE;
}

static const gchar *
get_file_urn (TrackerMinerFS *miner,
              GFile          *file,
              gboolean       *is_iri)
{
	const gchar *urn;

	urn = tracker_miner_fs_get_urn (miner, file);
	*is_iri = TRUE;

	if (!urn) {
		/* This is a new insertion, use anonymous URNs to store files */
		urn = "_:file";
		*is_iri = FALSE;
	}

	return urn;
}

static inline void
process_item (ProcessUserguideData  *data,
              GFileInfo             *file_info,
              gboolean               is_dir,
              GError               **error)
{
	TrackerSparqlBuilder *sparql;
	gchar *uri;
	const gchar *mime_type;
	const gchar *urn;
	const gchar *parent_urn;
	gboolean is_iri;
	guint64 time_;

	sparql = data->sparql;

	uri = g_file_get_uri (data->file);
	mime_type = g_file_info_get_content_type (file_info);

	urn = get_file_urn (data->miner, data->file, &is_iri);

	tracker_sparql_builder_insert_silent_open (sparql, NULL);
	tracker_sparql_builder_graph_open (sparql, TRACKER_MINER_FS_GRAPH_URN);

	if (is_iri) {
		tracker_sparql_builder_subject_iri (sparql, urn);
	} else {
		tracker_sparql_builder_subject (sparql, urn);
	}

	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:FileDataObject");
	tracker_sparql_builder_object (sparql, "nie:InformationElement");

	if (is_dir) {
		tracker_sparql_builder_object (sparql, "nfo:Folder");
	} else {
		tracker_sparql_builder_object (sparql, "nfo:HelpDocument");
	}

	parent_urn = tracker_miner_fs_get_parent_urn (TRACKER_MINER_FS (data->miner), data->file);

	if (parent_urn) {
		tracker_sparql_builder_predicate (sparql, "nfo:belongsToContainer");
		tracker_sparql_builder_object_iri (sparql, parent_urn);
	}

	tracker_sparql_builder_predicate (sparql, "nfo:fileName");
	tracker_sparql_builder_object_string (sparql, g_file_info_get_display_name (file_info));

	tracker_sparql_builder_predicate (sparql, "nfo:fileSize");
	tracker_sparql_builder_object_int64 (sparql, g_file_info_get_size (file_info));

	time_ = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	tracker_sparql_builder_predicate (sparql, "nfo:fileLastModified");
	tracker_sparql_builder_object_date (sparql, (time_t *) &time_);

	time_ = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_ACCESS);
	tracker_sparql_builder_predicate (sparql, "nfo:fileLastAccessed");
	tracker_sparql_builder_object_date (sparql, (time_t *) &time_);

	/* Laying the link between the IE and the DO. We use IE = DO */
	tracker_sparql_builder_predicate (sparql, "nie:isStoredAs");
	if (is_iri) {
		tracker_sparql_builder_object_iri (sparql, urn);
	} else {
		tracker_sparql_builder_object (sparql, urn);
	}

	/* The URL of the DataObject (because IE = DO, this is correct) */
	tracker_sparql_builder_predicate (sparql, "nie:url");
	tracker_sparql_builder_object_string (sparql, uri);

	tracker_sparql_builder_predicate (sparql, "nie:mimeType");
	tracker_sparql_builder_object_string (sparql, mime_type);

	/* FIXME: Add nie:dataSource for switching different userguides? */
	tracker_sparql_builder_predicate (sparql, "tracker:available");
	tracker_sparql_builder_object_boolean (sparql, TRUE);

	if (!is_dir) {
		gchar *content = NULL;
		gchar *title = NULL;

		/* Get content */
		parser_get_file_content (uri, MAX_EXTRACT_SIZE, &content, &title);

		g_message ("Adding userguide:'%s', uri:'%s'",
		           title,
		           uri);

		if (title && title[0]) {
			tracker_sparql_builder_predicate (sparql, "nie:title");
			tracker_sparql_builder_object_unvalidated (sparql, title);
		}

		if (content && content[0]) {
			tracker_sparql_builder_predicate (sparql, "nie:plainTextContent");
			tracker_sparql_builder_object_unvalidated (sparql, content);
		}

		g_free (content);
		g_free (title);
	} else {
		g_message ("Adding userguide directory:'%s'", uri);
	}

	tracker_sparql_builder_graph_close (sparql);
	tracker_sparql_builder_insert_close (sparql);

	g_free (uri);
}

static void
process_userguide_data_free (ProcessUserguideData *data)
{
	g_object_unref (data->miner);
	g_object_unref (data->file);
	g_object_unref (data->sparql);
	g_object_unref (data->cancellable);

	g_slice_free (ProcessUserguideData, data);
}

static void
process_file_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	ProcessUserguideData *data;
	GFileInfo *file_info;
	GError *error = NULL;
	GFile *file;
	gboolean is_dir;

	data = user_data;
	file = G_FILE (object);
	file_info = g_file_query_info_finish (file, result, &error);

	if (error) {
		tracker_miner_fs_file_notify (TRACKER_MINER_FS (data->miner), file, error);
		process_userguide_data_free (data);
		g_error_free (error);
		return;
	}

	is_dir = g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY;
	process_item (data, file_info, is_dir, &error);

	tracker_miner_fs_file_notify (TRACKER_MINER_FS (data->miner), data->file, error);
	process_userguide_data_free (data);

	if (error) {
		g_error_free (error);
	}

	if (file_info) {
		g_object_unref (file_info);
	}
}

static gboolean
miner_userguides_process_file (TrackerMinerFS       *fs,
                               GFile                *file,
                               TrackerSparqlBuilder *sparql,
                               GCancellable         *cancellable)
{
	ProcessUserguideData *data;
	const gchar *attrs;

	data = g_slice_new0 (ProcessUserguideData);
	data->miner = g_object_ref (fs);
	data->sparql = g_object_ref (sparql);
	data->file = g_object_ref (file);
	data->cancellable = g_object_ref (cancellable);

	attrs = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
		G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
		G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
		G_FILE_ATTRIBUTE_STANDARD_SIZE ","
		G_FILE_ATTRIBUTE_TIME_MODIFIED ","
		G_FILE_ATTRIBUTE_TIME_ACCESS;

	g_file_query_info_async (file,
	                         attrs,
	                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                         G_PRIORITY_DEFAULT,
	                         cancellable,
	                         process_file_cb,
	                         data);

	return TRUE;
}

static gboolean
miner_userguides_process_file_attributes (TrackerMinerFS       *fs,
                                          GFile                *file,
                                          TrackerSparqlBuilder *sparql,
                                          GCancellable         *cancellable)
{
	gchar *uri;

	/* We don't care about file attribute changes here */
	uri = g_file_get_uri (file);
	g_debug ("Ignoring file attribute changes in '%s'", uri);
	g_free (uri);

	return FALSE;
}

static void
parser_start_element (void           *ctx,
                      const xmlChar  *name,
                      const xmlChar **atts G_GNUC_UNUSED)
{
	const gchar *elem = (const gchar *) name;
	ParserContext *pctx = ctx;

	if (g_strcmp0 (elem, "title") == 0) {
		pctx->in_title = TRUE;
		return;
	}

	if (g_strcmp0 (elem, "body") == 0) {
		pctx->in_text = TRUE;
		return;
	}
}

static void
parser_end_element (void          *ctx,
                    const xmlChar *name)
{
	const gchar *elem = (const gchar *) name;
	ParserContext *pctx = ctx;

	if (g_strcmp0 (elem, "title") == 0) {
		pctx->in_title = FALSE;
	}
}

static void
parser_characters (void          *ctx,
                   const xmlChar *ch,
                   int            len G_GNUC_UNUSED)
{
	ParserContext *pctx = ctx;
	const gchar *str;
	int len_to_append;

	if (pctx->in_title) {
		const gchar *title = (const gchar *) ch;

		if (title[0]) {
			g_string_append_len (pctx->title,
			                     title,
			                     MIN(strlen (title), MAX_TITLE_LENGTH));
		}
	}

	if (!pctx->in_text) {
		return;
	}

	if ((gssize) pctx->plain_text->len >= pctx->max_length) {
		return;
	}

	str = (const gchar *) ch;

	if (!str || !str[0]) {
		return;
	}

	len_to_append = strlen (str);

	if ((gssize) pctx->plain_text->len + len_to_append > pctx->max_length) {
		len_to_append = pctx->max_length - pctx->plain_text->len;
	}

	g_string_append_len (pctx->plain_text, str, len_to_append);
}

static void
parser_error (void       *ctx,
              const char *msg,
              ...)
{
	ParserContext *pctx = ctx;
        va_list args;
        gchar *str;

        va_start (args, msg);
        str = g_strdup_vprintf (msg, args);
        va_end (args);

	g_critical ("Could not parse file '%s': %s", pctx->uri, str);
	g_free (str);
}

static void
parser_get_file_content (const gchar *uri,
                         gssize       max_extract_size,
                         gchar      **content,
                         gchar      **title)
{
	GError *error = NULL;
	gchar *filename;
	ParserContext parser_ctx = { 0 };
	htmlSAXHandler sax_handler = { 0 };
	htmlDocPtr doc;

	/* TODO: utf8 sanitization */

	filename = g_filename_from_uri (uri, NULL, &error);

	if (error) {
		g_message ("Could not open '%s': %s", uri, error->message);
		g_error_free (error);
		g_free (filename);
		return;
	}

	parser_ctx.uri = g_strdup (uri);
	parser_ctx.title = g_string_new (NULL);
	parser_ctx.in_text = FALSE;
	parser_ctx.in_title = FALSE;
	parser_ctx.plain_text = g_string_new (NULL);
	/* leave space for terminating 0 char */
	parser_ctx.max_length = max_extract_size - 1;

	sax_handler.startElement = parser_start_element;
	sax_handler.endElement = parser_end_element;
	sax_handler.characters = parser_characters;
	sax_handler.error = parser_error;

	doc = NULL;
	doc = htmlSAXParseFile (filename, "utf-8", &sax_handler, &parser_ctx);
	g_free (filename);

	if (doc) {
		xmlFreeDoc (doc);
	}

	g_free (parser_ctx.uri);

	*title = g_string_free (parser_ctx.title, FALSE);
	g_strstrip (*title);

	*content = g_string_free (parser_ctx.plain_text, FALSE);
	g_strstrip (*content);
}

/* If a reset is requested, we will remove from the store all items previously
 * inserted by the tracker-miner-userguides, this is:
 *  (a) Remove all resources which are a nfo:HelpDocument
 */
static void
miner_userguides_reset (TrackerMiner *miner)
{
	GError *error = NULL;
	TrackerSparqlBuilder *sparql;

	sparql = tracker_sparql_builder_new_update ();

	tracker_sparql_builder_delete_open (sparql, TRACKER_MINER_FS_GRAPH_URN);
	tracker_sparql_builder_subject_variable (sparql, "userguide");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "rdfs:Resource");
	tracker_sparql_builder_delete_close (sparql);

	tracker_sparql_builder_where_open (sparql);
	tracker_sparql_builder_subject_variable (sparql, "userguide");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:HelpDocument");
	tracker_sparql_builder_where_close (sparql);

	/* Execute a sync update, we don't want the userguides miner to start before
	 * we finish this. */
	tracker_sparql_connection_update (tracker_miner_get_connection (miner),
	                                  tracker_sparql_builder_get_result (sparql),
	                                  G_PRIORITY_HIGH,
	                                  NULL,
	                                  &error);

	if (error) {
		/* Some error happened performing the query, not good */
		g_critical ("Couldn't reset mined userguides: %s",
		            error ? error->message : "unknown error");
		g_error_free (error);
	}

	g_object_unref (sparql);
}

gboolean
tracker_miner_userguides_detect_locale_changed (TrackerMiner *miner)
{
	gboolean changed;

	changed = tracker_miner_applications_locale_changed ();
	if (changed) {
		g_message ("Locale change detected, so resetting miner to "
		           "remove all previously created items...");
		miner_userguides_reset (miner);
	}
	return changed;
}

TrackerMiner *
tracker_miner_userguides_new (GError **error)
{
	return g_initable_new (TRACKER_TYPE_MINER_USERGUIDES,
	                       NULL,
	                       error,
	                       "name", "Userguides",
	                       "processing-pool-wait-limit", 10,
	                       "processing-pool-ready-limit", 100,
	                       NULL);
}
