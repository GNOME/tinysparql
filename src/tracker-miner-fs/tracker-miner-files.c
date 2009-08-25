/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <libtracker-common/tracker-storage.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-miner-files.h"
#include "tracker-config.h"

#define TRACKER_MINER_FILES_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_FILES, TrackerMinerFilesPrivate))

typedef struct _TrackerMinerFilesPrivate TrackerMinerFilesPrivate;

struct _TrackerMinerFilesPrivate {
        TrackerConfig *config;
        TrackerStorage *storage;
	GVolumeMonitor *volume_monitor;
};

enum {
        PROP_0,
        PROP_CONFIG
};

static void     miner_files_set_property      (GObject              *object,
					       guint                 param_id,
					       const GValue         *value,
					       GParamSpec           *pspec);
static void     miner_files_get_property      (GObject              *object,
					       guint                 param_id,
					       GValue               *value,
					       GParamSpec           *pspec);
static void     miner_files_finalize          (GObject              *object);
static void     miner_files_constructed       (GObject              *object);
static gboolean miner_files_check_file        (TrackerMinerProcess  *miner,
					       GFile                *file);
static gboolean miner_files_check_directory   (TrackerMinerProcess  *miner,
					       GFile                *file);
static gboolean miner_files_process_file      (TrackerMinerProcess  *miner,
					       GFile                *file,
					       TrackerSparqlBuilder *sparql);
static gboolean miner_files_monitor_directory (TrackerMinerProcess  *miner,
					       GFile                *file);

G_DEFINE_TYPE (TrackerMinerFiles, tracker_miner_files, TRACKER_TYPE_MINER_PROCESS)

static void
tracker_miner_files_class_init (TrackerMinerFilesClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerMinerProcessClass *miner_process_class = TRACKER_MINER_PROCESS_CLASS (klass);

        object_class->finalize = miner_files_finalize;
        object_class->get_property = miner_files_get_property;
        object_class->set_property = miner_files_set_property;
        object_class->constructed = miner_files_constructed;

        miner_process_class->check_file = miner_files_check_file;
	miner_process_class->check_directory = miner_files_check_directory;
	miner_process_class->monitor_directory = miner_files_monitor_directory;
        miner_process_class->process_file = miner_files_process_file;

       	g_object_class_install_property (object_class,
					 PROP_CONFIG,
					 g_param_spec_object ("config",
							      "Config",
							      "Config",
                                                              TRACKER_TYPE_CONFIG,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_type_class_add_private (klass, sizeof (TrackerMinerFilesPrivate));
}

#ifdef HAVE_HAL

static void
mount_point_added_cb (TrackerStorage *storage,
                      const gchar    *udi,
                      const gchar    *mount_point,
                      gpointer        user_data)
{
        TrackerMinerFilesPrivate *priv;
        gboolean index_removable_devices;
        TrackerMinerProcess *miner;

        miner = TRACKER_MINER_PROCESS (user_data);
        priv = TRACKER_MINER_FILES_GET_PRIVATE (user_data);

        index_removable_devices = tracker_config_get_index_removable_devices (priv->config);

        if (index_removable_devices) {
                tracker_miner_process_add_directory (miner, mount_point, TRUE);
        }
}

static void
initialize_removable_devices (TrackerMinerFiles *miner)
{
        TrackerMinerFilesPrivate *priv;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (miner);

        if (tracker_config_get_index_removable_devices (priv->config)) {
                GList *mounts, *m;

                mounts = tracker_storage_get_removable_device_roots (priv->storage);

                for (m = mounts; m; m = m->next) {
                        tracker_miner_process_add_directory (TRACKER_MINER_PROCESS (miner),
                                                             m->data, TRUE);
                }
        }
}

#endif

static void
mount_pre_unmount_cb (GVolumeMonitor    *volume_monitor,
		      GMount            *mount,
		      TrackerMinerFiles *miner)
{
	GFile *mount_root;
	gchar *path;

	mount_root = g_mount_get_root (mount);
	path = g_file_get_path (mount_root);

	tracker_miner_process_remove_directory (TRACKER_MINER_PROCESS (miner), path);

	g_free (path);
	g_object_unref (mount_root);
}

static void
tracker_miner_files_init (TrackerMinerFiles *miner)
{
        TrackerMinerFilesPrivate *priv;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (miner);

#ifdef HAVE_HAL
        priv->storage = tracker_storage_new ();

        g_signal_connect (priv->storage, "mount-point-added",
                          G_CALLBACK (mount_point_added_cb), miner);
#endif

	priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect (priv->volume_monitor, "mount-pre-unmount",
			  G_CALLBACK (mount_pre_unmount_cb),
			  miner);
}

static void
miner_files_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
        TrackerMinerFilesPrivate *priv;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (object);

        switch (prop_id) {
        case PROP_CONFIG:
                priv->config = g_value_dup_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
        }
}

static void
miner_files_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
        TrackerMinerFilesPrivate *priv;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (object);

        switch (prop_id) {
        case PROP_CONFIG:
                g_value_set_object (value, priv->config);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
        }
}

static void
miner_files_finalize (GObject *object)
{
        TrackerMinerFilesPrivate *priv;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (object);

        g_object_unref (priv->config);

#ifdef HAVE_HAL
        g_object_unref (priv->storage);
#endif

	g_signal_handlers_disconnect_by_func (priv->volume_monitor,
					      mount_pre_unmount_cb,
					      object);
	g_object_unref (priv->volume_monitor);

        G_OBJECT_CLASS (tracker_miner_files_parent_class)->finalize (object);
}

static void
miner_files_constructed (GObject *object)
{
        TrackerMinerFilesPrivate *priv;
        TrackerMinerProcess *miner;
        GSList *dirs;

	G_OBJECT_CLASS (tracker_miner_files_parent_class)->constructed (object);

        priv = TRACKER_MINER_FILES_GET_PRIVATE (object);
        miner = TRACKER_MINER_PROCESS (object);

        if (!priv->config) {
                g_critical ("No config. This is mandatory");
                g_assert_not_reached ();
        }

        /* Fill in directories to inspect */
        dirs = tracker_config_get_index_single_directories (priv->config);

        while (dirs) {
                tracker_miner_process_add_directory (miner, dirs->data, FALSE);
                dirs = dirs->next;
        }

        dirs = tracker_config_get_index_recursive_directories (priv->config);

        while (dirs) {
                tracker_miner_process_add_directory (miner, dirs->data, TRUE);
                dirs = dirs->next;
        }

#ifdef HAVE_HAL
        initialize_removable_devices (TRACKER_MINER_FILES (miner));
#endif
}

static gboolean
miner_files_check_file (TrackerMinerProcess *miner,
			GFile               *file)
{
	GFileInfo *file_info;
	gchar *path;
	gboolean should_process;

	file_info = NULL;
	should_process = FALSE;
	path = g_file_get_path (file);

	if (tracker_is_empty_string (path)) {
		goto done;
	}

	if (!g_utf8_validate (path, -1, NULL)) {
		g_message ("Ignoring path:'%s', not valid UTF-8", path);
		goto done;
	}

	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, NULL);

	if (file_info && g_file_info_get_is_hidden (file_info)) {
		/* Ignore hidden files */
		goto done;
	}

        /* FIXME: Check config */

	should_process = TRUE;

done:
	if (file_info) {
		g_object_unref (file_info);
	}

	g_free (path);

	return should_process;
}

static gboolean
miner_files_check_directory (TrackerMinerProcess *miner,
			     GFile               *file)
{
	GFileInfo *file_info;
	gchar *path;
	gboolean should_process;

	file_info = NULL;
	should_process = FALSE;
	path = g_file_get_path (file);

	if (tracker_is_empty_string (path)) {
		goto done;
	}

	if (!g_utf8_validate (path, -1, NULL)) {
		g_message ("Ignoring path:'%s', not valid UTF-8", path);
		goto done;
	}

	/* Most common things to ignore */
	if (strcmp (path, "/dev") == 0 ||
	    strcmp (path, "/lib") == 0 ||
	    strcmp (path, "/proc") == 0 ||
	    strcmp (path, "/sys") == 0) {
		goto done;
	}
	
	if (g_str_has_prefix (path, g_get_tmp_dir ())) {
		goto done;
	}

	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, NULL);

	if (file_info && g_file_info_get_is_hidden (file_info)) {
		/* Ignore hidden dirs */
		goto done;
	}

        /* FIXME: Check config */

	/* Check module directory ignore patterns */
	should_process = TRUE;

done:
	if (file_info) {
		g_object_unref (file_info);
	}

	g_free (path);

	return should_process;
}

static gboolean
miner_files_monitor_directory (TrackerMinerProcess *miner,
			       GFile               *file)
{
        TrackerMinerFilesPrivate *priv;
	GFileInfo *file_info;
	gchar *path;
	gboolean should_process;

	file_info = NULL;
	should_process = FALSE;
	path = g_file_get_path (file);

        priv = TRACKER_MINER_FILES_GET_PRIVATE (miner);

        if (!tracker_config_get_enable_monitors (priv->config)) {
		goto done;
	}

	if (tracker_is_empty_string (path)) {
		goto done;
	}

	if (!g_utf8_validate (path, -1, NULL)) {
		g_message ("Ignoring path:'%s', not valid UTF-8", path);
		goto done;
	}

	/* Most common things to ignore */
	if (strcmp (path, "/dev") == 0 ||
	    strcmp (path, "/lib") == 0 ||
	    strcmp (path, "/proc") == 0 ||
	    strcmp (path, "/sys") == 0) {
		goto done;
	}
	
	if (g_str_has_prefix (path, g_get_tmp_dir ())) {
		goto done;
	}

	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, NULL);

	if (file_info && g_file_info_get_is_hidden (file_info)) {
		/* Ignore hidden dirs */
		goto done;
	}

        /* FIXME: Check config */

	/* Check module directory ignore patterns */
	should_process = TRUE;

done:
	if (file_info) {
		g_object_unref (file_info);
	}

	g_free (path);

	return should_process;
}

static void
miner_files_add_to_datasource (TrackerMinerFiles    *miner,
			       GFile                *file,
			       TrackerSparqlBuilder *sparql)
{
        TrackerMinerFilesPrivate *priv;
	const gchar *removable_device_udi;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (miner);

#ifdef HAVE_HAL
	removable_device_udi = tracker_storage_get_volume_udi_for_file (priv->storage, file);
#else
	removable_device_udi = NULL;
#endif

	if (removable_device_udi) {
		gchar *removable_device_urn;

		removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s",
						        removable_device_udi);

		tracker_sparql_builder_subject_iri (sparql, removable_device_urn);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "tracker:Volume");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, removable_device_urn);

		g_free (removable_device_urn);
	} else {
		tracker_sparql_builder_subject_iri (sparql, TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "tracker:Volume");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN);
	}
}

static gboolean
miner_files_process_file (TrackerMinerProcess  *miner,
			  GFile                *file,
			  TrackerSparqlBuilder *sparql)
{
	gchar *uri, *mime_type;
	GFileInfo *file_info;
	guint64 time_;
	GFile *parent;
	gchar *parent_uri;
	const gchar *attrs;

	attrs = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
		G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
		G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
		G_FILE_ATTRIBUTE_STANDARD_SIZE ","
		G_FILE_ATTRIBUTE_TIME_MODIFIED ","
		G_FILE_ATTRIBUTE_TIME_ACCESS;

	file_info = g_file_query_info (file,
				       attrs,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, NULL);
	if (!file_info) {
		return FALSE;
	}

	uri = g_file_get_uri (file);
	mime_type = g_strdup (g_file_info_get_content_type (file_info));

        tracker_sparql_builder_insert_open (sparql);

        tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:FileDataObject");

	tracker_sparql_builder_subject_iri (sparql, uri); /* Change to URN */
	tracker_sparql_builder_predicate (sparql, "nie:isStoredAs");
	tracker_sparql_builder_object_iri (sparql, uri);

	if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
		tracker_sparql_builder_object (sparql, "nfo:Folder");
	}

	parent = g_file_get_parent (file);
	if (parent) {
		parent_uri = g_file_get_uri (parent);
		tracker_sparql_builder_predicate (sparql, "nfo:belongsToContainer");
		tracker_sparql_builder_object_iri (sparql, parent_uri);
		g_free (parent_uri);
		g_object_unref (parent);
	}

	tracker_sparql_builder_predicate (sparql, "nfo:fileName");
	tracker_sparql_builder_object_string (sparql, g_file_info_get_display_name (file_info));

	tracker_sparql_builder_predicate (sparql, "nie:mimeType");
	tracker_sparql_builder_object_string (sparql, mime_type);

	tracker_sparql_builder_predicate (sparql, "nfo:fileSize");
	tracker_sparql_builder_object_int64 (sparql, g_file_info_get_size (file_info));

	time_ = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	tracker_sparql_builder_predicate (sparql, "nfo:fileLastModified");
	tracker_sparql_builder_object_date (sparql, (time_t *) &time_);

	time_ = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_ACCESS);
        tracker_sparql_builder_predicate (sparql, "nfo:fileLastAccessed");
	tracker_sparql_builder_object_date (sparql, (time_t *) &time_);

        miner_files_add_to_datasource (TRACKER_MINER_FILES (miner), file, sparql);

        /* FIXME: Missing embedded data and text */

	g_free (uri);

	return TRUE;
}

TrackerMiner *
tracker_miner_files_new (TrackerConfig *config)
{
        return g_object_new (TRACKER_TYPE_MINER_FILES,
                             "name", "Files",
                             "config", config,
                             NULL);
}
