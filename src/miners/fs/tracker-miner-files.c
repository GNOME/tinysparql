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

#include <sys/statvfs.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/msdos_fs.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-common/tracker-power.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-miner-files.h"
#include "tracker-config.h"
#include "tracker-extract-client.h"
#include "tracker-marshal.h"

#define DISK_SPACE_CHECK_FREQUENCY 10

#define TRACKER_MINER_FILES_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_FILES, TrackerMinerFilesPrivate))

static GQuark miner_files_error_quark = 0;

typedef struct ProcessFileData ProcessFileData;

struct ProcessFileData {
	TrackerMinerFiles *miner;
	TrackerSparqlBuilder *sparql;
	GCancellable *cancellable;
	GFile *file;
	DBusGProxyCall *call;
};

struct TrackerMinerFilesPrivate {
	TrackerConfig *config;
	TrackerStorage *storage;
	TrackerPower *power;

	GVolumeMonitor *volume_monitor;

        GSList *index_recursive_directories;
        GSList *index_single_directories;

	guint disk_space_check_id;
	guint disk_space_pause_cookie;

	guint low_battery_pause_cookie;

	DBusGProxy *extractor_proxy;

	GQuark quark_mount_point_uuid;
	GQuark quark_directory_config_root;

	guint force_recheck_id;
};

enum {
	VOLUME_MOUNTED_IN_STORE = 1 << 0,
	VOLUME_MOUNTED = 1 << 1
};

enum {
	PROP_0,
	PROP_CONFIG
};

static void        miner_files_set_property             (GObject              *object,
                                                         guint                 param_id,
                                                         const GValue         *value,
                                                         GParamSpec           *pspec);
static void        miner_files_get_property             (GObject              *object,
                                                         guint                 param_id,
                                                         GValue               *value,
                                                         GParamSpec           *pspec);
static void        miner_files_finalize                 (GObject              *object);
static void        miner_files_constructed              (GObject              *object);
static void        mount_pre_unmount_cb                 (GVolumeMonitor       *volume_monitor,
                                                         GMount               *mount,
                                                         TrackerMinerFiles    *mf);

static void        mount_point_added_cb                 (TrackerStorage       *storage,
                                                         const gchar          *uuid,
                                                         const gchar          *mount_point,
                                                         gboolean              removable,
                                                         gboolean              optical,
                                                         gpointer              user_data);
static void        mount_point_removed_cb               (TrackerStorage       *storage,
                                                         const gchar          *uuid,
                                                         const gchar          *mount_point,
                                                         gpointer              user_data);
static void        check_battery_status                 (TrackerMinerFiles    *fs);
static void        battery_status_cb                    (GObject              *object,
                                                         GParamSpec           *pspec,
                                                         gpointer              user_data);

static void        init_mount_points                    (TrackerMinerFiles    *miner);
static void        disk_space_check_start               (TrackerMinerFiles    *mf);
static void        disk_space_check_stop                (TrackerMinerFiles    *mf);
static void        low_disk_space_limit_cb              (GObject              *gobject,
                                                         GParamSpec           *arg1,
                                                         gpointer              user_data);
static void        index_recursive_directories_cb       (GObject              *gobject,
                                                         GParamSpec           *arg1,
                                                         gpointer              user_data);
static void        index_single_directories_cb          (GObject              *gobject,
                                                         GParamSpec           *arg1,
                                                         gpointer              user_data);
static void        ignore_directories_cb                (GObject              *gobject,
							 GParamSpec           *arg1,
							 gpointer              user_data);
static DBusGProxy *extractor_create_proxy               (void);
static gboolean    miner_files_check_file               (TrackerMinerFS       *fs,
                                                         GFile                *file);
static gboolean    miner_files_check_directory          (TrackerMinerFS       *fs,
                                                         GFile                *file);
static gboolean    miner_files_check_directory_contents (TrackerMinerFS       *fs,
                                                         GFile                *parent,
                                                         GList                *children);
static gboolean    miner_files_process_file             (TrackerMinerFS       *fs,
                                                         GFile                *file,
                                                         TrackerSparqlBuilder *sparql,
                                                         GCancellable         *cancellable);
static gboolean    miner_files_monitor_directory        (TrackerMinerFS       *fs,
                                                         GFile                *file);
static gboolean    miner_files_ignore_next_update_file  (TrackerMinerFS       *fs,
                                                         GFile                *file,
                                                         TrackerSparqlBuilder *sparql,
                                                         GCancellable         *cancellable);
static void      extractor_get_embedded_metadata_cancel (GCancellable    *cancellable,
                                                         ProcessFileData *data);


G_DEFINE_TYPE (TrackerMinerFiles, tracker_miner_files, TRACKER_TYPE_MINER_FS)

static void
tracker_miner_files_class_init (TrackerMinerFilesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerFSClass *miner_fs_class = TRACKER_MINER_FS_CLASS (klass);

	object_class->finalize = miner_files_finalize;
	object_class->get_property = miner_files_get_property;
	object_class->set_property = miner_files_set_property;
	object_class->constructed = miner_files_constructed;

	miner_fs_class->check_file = miner_files_check_file;
	miner_fs_class->check_directory = miner_files_check_directory;
	miner_fs_class->check_directory_contents = miner_files_check_directory_contents;
	miner_fs_class->monitor_directory = miner_files_monitor_directory;
	miner_fs_class->process_file = miner_files_process_file;
	miner_fs_class->ignore_next_update_file = miner_files_ignore_next_update_file;

	g_object_class_install_property (object_class,
	                                 PROP_CONFIG,
	                                 g_param_spec_object ("config",
	                                                      "Config",
	                                                      "Config",
	                                                      TRACKER_TYPE_CONFIG,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (klass, sizeof (TrackerMinerFilesPrivate));

	miner_files_error_quark = g_quark_from_static_string ("TrackerMinerFiles");
}

static void
tracker_miner_files_init (TrackerMinerFiles *mf)
{
	TrackerMinerFilesPrivate *priv;

	priv = mf->private = TRACKER_MINER_FILES_GET_PRIVATE (mf);

	priv->storage = tracker_storage_new ();

	g_signal_connect (priv->storage, "mount-point-added",
	                  G_CALLBACK (mount_point_added_cb),
	                  mf);

	g_signal_connect (priv->storage, "mount-point-removed",
	                  G_CALLBACK (mount_point_removed_cb),
	                  mf);

#if defined(HAVE_DEVKIT_POWER) || defined(HAVE_HAL)
	priv->power = tracker_power_new ();

	g_signal_connect (priv->power, "notify::on-low-battery",
	                  G_CALLBACK (battery_status_cb),
	                  mf);
	g_signal_connect (priv->power, "notify::on-battery",
	                  G_CALLBACK (battery_status_cb),
	                  mf);
#endif /* defined(HAVE_DEVKIT_POWER) || defined(HAVE_HAL) */

	priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect (priv->volume_monitor, "mount-pre-unmount",
	                  G_CALLBACK (mount_pre_unmount_cb),
	                  mf);

	/* Set up extractor and signals */
	priv->extractor_proxy = extractor_create_proxy ();

	priv->quark_mount_point_uuid = g_quark_from_static_string ("tracker-mount-point-uuid");
	priv->quark_directory_config_root = g_quark_from_static_string ("tracker-directory-config-root");

	init_mount_points (mf);
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
	TrackerMinerFiles *mf;
	TrackerMinerFilesPrivate *priv;

	mf = TRACKER_MINER_FILES (object);
	priv = mf->private;

	g_object_unref (priv->extractor_proxy);

	g_signal_handlers_disconnect_by_func (priv->config,
	                                      low_disk_space_limit_cb,
	                                      NULL);

	g_object_unref (priv->config);

	disk_space_check_stop (TRACKER_MINER_FILES (object));

        if (priv->index_recursive_directories) {
                g_slist_foreach (priv->index_recursive_directories, (GFunc) g_free, NULL);
                g_slist_free (priv->index_recursive_directories);
        }

        if (priv->index_single_directories) {
                g_slist_foreach (priv->index_single_directories, (GFunc) g_free, NULL);
                g_slist_free (priv->index_single_directories);
        }

#if defined(HAVE_DEVKIT_POWER) || defined(HAVE_HAL)
	g_object_unref (priv->power);
#endif /* defined(HAVE_DEVKIT_POWER) || defined(HAVE_HAL) */

	g_object_unref (priv->storage);

	g_signal_handlers_disconnect_by_func (priv->volume_monitor,
	                                      mount_pre_unmount_cb,
	                                      object);
	g_object_unref (priv->volume_monitor);

	if (priv->force_recheck_id) {
		g_source_remove (priv->force_recheck_id);
		priv->force_recheck_id = 0;
	}

	G_OBJECT_CLASS (tracker_miner_files_parent_class)->finalize (object);
}

static void
miner_files_constructed (GObject *object)
{
	TrackerMinerFiles *mf;
	TrackerMinerFS *fs;
	GSList *dirs;
	GSList *mounts = NULL, *m;
	gboolean index_removable_devices;
	gboolean index_optical_discs;
	TrackerStorageType type = 0;

	G_OBJECT_CLASS (tracker_miner_files_parent_class)->constructed (object);

	mf = TRACKER_MINER_FILES (object);
	fs = TRACKER_MINER_FS (object);

	if (G_UNLIKELY (!mf->private->config)) {
		g_message ("No config for miner %p (%s).", object, G_OBJECT_TYPE_NAME (object));
		return;
	}

	if (tracker_config_get_index_removable_devices (mf->private->config)) {
		index_removable_devices = TRUE;
		type |= TRACKER_STORAGE_REMOVABLE;
	}

	if (tracker_config_get_index_optical_discs (mf->private->config)) {
		index_optical_discs = TRUE;
		type |= TRACKER_STORAGE_OPTICAL;
	}

	mounts = tracker_storage_get_device_roots (mf->private->storage, type, TRUE);

#if defined(HAVE_DEVKIT_POWER) || defined(HAVE_HAL)
	check_battery_status (mf);
#endif /* defined(HAVE_DEVKIT_POWER) || defined(HAVE_HAL) */

	g_message ("Setting up directories to iterate from config (IndexSingleDirectory)");

	/* Fill in directories to inspect */
	dirs = tracker_config_get_index_single_directories (mf->private->config);

        /* Copy in case of config changes */
        mf->private->index_single_directories = tracker_gslist_copy_with_string_data (dirs);

	for (; dirs; dirs = dirs->next) {
		GFile *file;

		/* Do some simple checks for silly locations */
		if (strcmp (dirs->data, "/dev") == 0 ||
		    strcmp (dirs->data, "/lib") == 0 ||
		    strcmp (dirs->data, "/proc") == 0 ||
		    strcmp (dirs->data, "/sys") == 0) {
			continue;
		}

		if (g_str_has_prefix (dirs->data, g_get_tmp_dir ())) {
			continue;
		}

		/* Make sure we don't crawl volumes. */
		if (mounts) {
			gboolean found = FALSE;

			for (m = mounts; m && !found; m = m->next) {
				found = strcmp (m->data, dirs->data) == 0;
			}

			if (found) {
				g_message ("  Duplicate found:'%s' - same as removable device path",
				           (gchar*) dirs->data);
				continue;
			}
		}

		g_message ("  Adding:'%s'", (gchar*) dirs->data);

		file = g_file_new_for_path (dirs->data);
		g_object_set_qdata (G_OBJECT (file),
		                    mf->private->quark_directory_config_root,
		                    GINT_TO_POINTER (TRUE));

		tracker_miner_fs_directory_add (fs, file, FALSE);
		g_object_unref (file);
	}

	g_message ("Setting up directories to iterate from config (IndexRecursiveDirectory)");

	dirs = tracker_config_get_index_recursive_directories (mf->private->config);

        /* Copy in case of config changes */
        mf->private->index_recursive_directories = tracker_gslist_copy_with_string_data (dirs);

	for (; dirs; dirs = dirs->next) {
		GFile *file;

		/* Do some simple checks for silly locations */
		if (strcmp (dirs->data, "/dev") == 0 ||
		    strcmp (dirs->data, "/lib") == 0 ||
		    strcmp (dirs->data, "/proc") == 0 ||
		    strcmp (dirs->data, "/sys") == 0) {
			continue;
		}

		if (g_str_has_prefix (dirs->data, g_get_tmp_dir ())) {
			continue;
		}

		/* Make sure we don't crawl volumes. */
		if (mounts) {
			gboolean found = FALSE;

			for (m = mounts; m && !found; m = m->next) {
				found = strcmp (m->data, dirs->data) == 0;
			}

			if (found) {
				g_message ("  Duplicate found:'%s' - same as removable device path",
				           (gchar*) dirs->data);
				continue;
			}
		}

		g_message ("  Adding:'%s'", (gchar*) dirs->data);

		file = g_file_new_for_path (dirs->data);
		g_object_set_qdata (G_OBJECT (file),
		                    mf->private->quark_directory_config_root,
		                    GINT_TO_POINTER (TRUE));

		tracker_miner_fs_directory_add (fs, file, TRUE);
		g_object_unref (file);
	}

	/* Add mounts */
	g_message ("Setting up directories to iterate from devices/discs");

	if (!index_removable_devices) {
		g_message ("  Removable devices are disabled in the config");
	}

	if (!index_optical_discs) {
		g_message ("  Optical discs are disabled in the config");
	}

	for (m = mounts; m; m = m->next) {
		GFile *file = g_file_new_for_path (m->data);
		const gchar *uuid = tracker_storage_get_uuid_for_file (mf->private->storage, file);

		g_object_set_qdata_full (G_OBJECT (file),
					 mf->private->quark_mount_point_uuid,
					 g_strdup (uuid),
					 (GDestroyNotify) g_free);
		g_object_set_qdata (G_OBJECT (file),
		                    mf->private->quark_directory_config_root,
		                    GINT_TO_POINTER (TRUE));

		g_message ("  Adding:'%s'", (gchar*) m->data);
		tracker_miner_fs_directory_add (TRACKER_MINER_FS (mf),
		                                file,
		                                TRUE);
		g_object_unref (file);
	}

	/* Add optical media */

	g_signal_connect (mf->private->config, "notify::low-disk-space-limit",
	                  G_CALLBACK (low_disk_space_limit_cb),
	                  mf);
	g_signal_connect (mf->private->config, "notify::index-recursive-directories",
	                  G_CALLBACK (index_recursive_directories_cb),
	                  mf);
	g_signal_connect (mf->private->config, "notify::index-single-directories",
	                  G_CALLBACK (index_single_directories_cb),
	                  mf);
	g_signal_connect (mf->private->config, "notify::ignored-directories",
	                  G_CALLBACK (ignore_directories_cb),
	                  mf);
	g_signal_connect (mf->private->config, "notify::ignored-directories-with-content",
	                  G_CALLBACK (ignore_directories_cb),
	                  mf);

	g_slist_foreach (mounts, (GFunc) g_free, NULL);
	g_slist_free (mounts);

	disk_space_check_start (mf);
}

static void
set_up_mount_point_cb (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	gchar *removable_device_urn = user_data;

	GError *error = NULL;
	tracker_miner_execute_update_finish (TRACKER_MINER (source),
	                                     result,
	                                     &error);

	if (error) {
		g_critical ("Could not set up mount point '%s': %s",
		            removable_device_urn, error->message);
		g_error_free (error);
	}

	g_free (removable_device_urn);
}

static void
set_up_mount_point (TrackerMinerFiles *miner,
                    const gchar       *removable_device_urn,
                    const gchar       *mount_point,
                    gboolean           mounted,
                    GString           *accumulator)
{
	GString *queries;

	g_debug ("Setting mount point '%s' state in database (URN '%s')", 
	         mount_point,
	         removable_device_urn);

	queries = g_string_new (NULL);

	if (mounted) {
		if (mount_point) {
			GFile *file;
			gchar *uri;

			file = g_file_new_for_path (mount_point);
			uri = g_file_get_uri (file);

			g_string_append_printf (queries,
			                        "DROP GRAPH <%s> "
			                        "INSERT INTO <%s> { "
			                        "  <%s> a tracker:Volume; "
			                        "       tracker:mountPoint ?u "
			                        "} WHERE { "
			                        "  ?u a nfo:FileDataObject; "
			                        "     nie:url \"%s\" "
			                        "}",
						removable_device_urn, removable_device_urn, removable_device_urn, uri);

			g_object_unref (file);
			g_free (uri);
		}

		g_string_append_printf (queries,
		                        "DELETE FROM <%s> { <%s> tracker:isMounted ?unknown } WHERE { <%s> a tracker:Volume; tracker:isMounted ?unknown } ",
		                        removable_device_urn, removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT INTO <%s> { <%s> a tracker:Volume; tracker:isMounted true } ",
		                        removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT INTO <%s> { ?do tracker:available true } WHERE { ?do nie:dataSource <%s> OPTIONAL { ?do tracker:available ?available } FILTER (!bound(?available)) } ",
		                        removable_device_urn, removable_device_urn);
	} else {
		gchar *now;

		now = tracker_date_to_string (time (NULL));

		g_string_append_printf (queries,
		                        "DELETE FROM <%s> { <%s> tracker:unmountDate ?unknown } WHERE { <%s> a tracker:Volume; tracker:unmountDate ?unknown } ",
		                        removable_device_urn, removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT INTO <%s> { <%s> a tracker:Volume; tracker:unmountDate \"%s\" } ",
		                        removable_device_urn, removable_device_urn, now);

		g_string_append_printf (queries,
		                        "DELETE FROM <%s> { <%s> tracker:isMounted ?unknown } WHERE { <%s> a tracker:Volume; tracker:isMounted ?unknown } ",
		                        removable_device_urn, removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT INTO <%s> { <%s> a tracker:Volume; tracker:isMounted false } ",
		                        removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "DELETE FROM <%s> { ?do tracker:available true } WHERE { ?do nie:dataSource <%s> ; tracker:available true } ",
		                        removable_device_urn, removable_device_urn);
		g_free (now);
	}

	if (accumulator) {
		g_string_append_printf (accumulator, "%s ", queries->str);
	} else {
		tracker_miner_execute_update (TRACKER_MINER (miner),
		                              queries->str,
		                              NULL,
		                              set_up_mount_point_cb,
		                              g_strdup (removable_device_urn));
	}

	g_string_free (queries, TRUE);
}

static void
init_mount_points_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
	GError *error = NULL;

	tracker_miner_execute_update_finish (TRACKER_MINER (source),
	                                     result,
	                                     &error);

	if (error) {
		g_critical ("Could not initialize currently active mount points: %s",
		            error->message);
		g_error_free (error);
	}
}

static void
query_mount_points_cb (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	TrackerMiner *miner = TRACKER_MINER (source);
	TrackerMinerFilesPrivate *priv;
	GHashTable *volumes;
	GHashTableIter iter;
	gpointer key, value;
	GString *accumulator;
	gint i;
	GError *error = NULL;
	const GPtrArray *query_results;
	GSList *uuids, *u;

	query_results = tracker_miner_execute_sparql_finish (miner,
	                                                     result,
	                                                     &error);
	if (error) {
		g_critical ("Could not obtain the mounted volumes: %s", error->message);
		g_error_free (error);
		return;
	}

	priv = TRACKER_MINER_FILES_GET_PRIVATE (miner);

	volumes = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                 (GDestroyNotify) g_free,
	                                 NULL);

	for (i = 0; i < query_results->len; i++) {
		gchar **row;
		gint state;

		row = g_ptr_array_index (query_results, i);
		state = VOLUME_MOUNTED_IN_STORE;

		if (strcmp (row[0], TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN) == 0) {
			/* Report non-removable media to be mounted by HAL as well */
			state |= VOLUME_MOUNTED;
		}

		g_hash_table_insert (volumes, g_strdup (row[0]), GINT_TO_POINTER (state));
	}

	g_hash_table_replace (volumes, g_strdup (TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN),
	                      GINT_TO_POINTER (VOLUME_MOUNTED));

	uuids = tracker_storage_get_device_uuids (priv->storage, TRACKER_STORAGE_REMOVABLE, FALSE);

	/* Then, get all currently mounted volumes, according to GIO */
	for (u = uuids; u; u = u->next) {
		const gchar *uuid;
		gchar *removable_device_urn;
		gint state;

		uuid = u->data;
		removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", uuid);

		state = GPOINTER_TO_INT (g_hash_table_lookup (volumes, removable_device_urn));
		state |= VOLUME_MOUNTED;

		g_hash_table_replace (volumes, removable_device_urn, GINT_TO_POINTER (state));
	}

	g_slist_foreach (uuids, (GFunc) g_free, NULL);
	g_slist_free (uuids);

	accumulator = g_string_new (NULL);
	g_hash_table_iter_init (&iter, volumes);

	/* Finally, set up volumes based on the composed info */
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *urn = key;
		gint state = GPOINTER_TO_INT (value);

		if ((state & VOLUME_MOUNTED) &&
		    !(state & VOLUME_MOUNTED_IN_STORE)) {
			const gchar *mount_point = NULL;

			if (g_str_has_prefix (urn, TRACKER_DATASOURCE_URN_PREFIX)) {
				const gchar *uuid;

				uuid = urn + strlen (TRACKER_DATASOURCE_URN_PREFIX);
				mount_point = tracker_storage_get_mount_point_for_uuid (priv->storage, uuid);
			}

			if (urn) {
				if (mount_point) {
					g_debug ("URN '%s' (mount point: %s) was not reported to be mounted, but now it is, updating state",
					         urn, mount_point);
				} else {
					g_debug ("URN '%s' was not reported to be mounted, but now it is, updating state", urn);
				}
				set_up_mount_point (TRACKER_MINER_FILES (miner), urn, mount_point, TRUE, accumulator);
			}
		} else if (!(state & VOLUME_MOUNTED) &&
		           (state & VOLUME_MOUNTED_IN_STORE)) {
			if (urn) {
				g_debug ("URN '%s' was reported to be mounted, but it isn't anymore, updating state", urn);
				set_up_mount_point (TRACKER_MINER_FILES (miner), urn, NULL, FALSE, accumulator);
			}
		}
	}

	if (accumulator->str[0] != '\0') {
		tracker_miner_execute_update (miner,
		                              accumulator->str,
		                              NULL,
		                              init_mount_points_cb,
		                              NULL);
	}

	g_string_free (accumulator, TRUE);
	g_hash_table_unref (volumes);
}

static void
init_mount_points (TrackerMinerFiles *miner)
{
	g_debug ("Initializing mount points");

	/* First, get all mounted volumes, according to tracker-store */
	tracker_miner_execute_sparql (TRACKER_MINER (miner),
	                              "SELECT ?v WHERE { ?v a tracker:Volume ; tracker:isMounted true }",
	                              NULL,
	                              query_mount_points_cb,
	                              NULL);
}

static void
mount_point_removed_cb (TrackerStorage *storage,
                        const gchar    *uuid,
                        const gchar    *mount_point,
                        gpointer        user_data)
{
	TrackerMinerFiles *miner = user_data;
	gchar *urn;

	g_debug ("Removing mount point '%s'", mount_point);

	urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", uuid);

	set_up_mount_point (miner, urn, mount_point, FALSE, NULL);
	g_free (urn);
}

static void
mount_point_added_cb (TrackerStorage *storage,
                      const gchar    *uuid,
                      const gchar    *mount_point,
                      gboolean        removable,
                      gboolean        optical,
                      gpointer        user_data)
{
	TrackerMinerFiles *miner = user_data;
	TrackerMinerFilesPrivate *priv;
	GFile *file;
	gchar *urn;
	gboolean index_removable_devices;
	gboolean index_optical_discs;
	gboolean should_crawl;

	priv = TRACKER_MINER_FILES_GET_PRIVATE (miner);

	index_removable_devices = tracker_config_get_index_removable_devices (priv->config);
	index_optical_discs = tracker_config_get_index_optical_discs (priv->config);

	g_message ("Added mount point '%s'", mount_point);

	should_crawl = TRUE;	

	if (removable && !tracker_config_get_index_removable_devices (priv->config)) {
		g_message ("  Not crawling, removable devices disabled in config");
		should_crawl = FALSE;
	}

	if (optical && !tracker_config_get_index_optical_discs (priv->config)) {
		g_message ("  Not crawling, optical devices discs disabled in config");
		should_crawl = FALSE;
	}

	if (should_crawl) {
		g_message ("  Adding directory to crawler's queue");

		file = g_file_new_for_path (mount_point);
		g_object_set_qdata_full (G_OBJECT (file),
		                         priv->quark_mount_point_uuid,
		                         g_strdup (uuid),
		                         (GDestroyNotify) g_free);

		g_object_set_qdata (G_OBJECT (file),
		                    priv->quark_directory_config_root,
		                    GINT_TO_POINTER (TRUE));

		tracker_miner_fs_directory_add (TRACKER_MINER_FS (user_data),
		                                file,
		                                TRUE);
		g_object_unref (file);
	}

	urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", uuid);
	set_up_mount_point (miner, urn, mount_point, TRUE, NULL);
	g_free (urn);
}

#if defined(HAVE_DEVKIT_POWER) || defined(HAVE_HAL)

static void
set_up_throttle (TrackerMinerFiles *mf,
                 gboolean           enable)
{
	gdouble throttle;
	gint config_throttle;

	config_throttle = tracker_config_get_throttle (mf->private->config);
	throttle = (1.0 / 20) * config_throttle;

	if (enable) {
		throttle += 0.25;
	}

	throttle = CLAMP (throttle, 0, 1);

	g_debug ("Setting new throttle to %0.3f", throttle);
	tracker_miner_fs_set_throttle (TRACKER_MINER_FS (mf), throttle);
}

static void
check_battery_status (TrackerMinerFiles *mf)
{
	gboolean on_battery, on_low_battery;
	gboolean should_pause = FALSE;
	gboolean should_throttle = FALSE;

	on_low_battery = tracker_power_get_on_low_battery (mf->private->power);
	on_battery = tracker_power_get_on_battery (mf->private->power);

	if (!on_battery) {
		g_message ("Running on AC power");
		should_pause = FALSE;
		should_throttle = FALSE;
	} else {
		g_message ("Running on battery");

		should_throttle = TRUE;

		if (on_low_battery) {
			g_message ("  Battery is LOW, pausing");
			should_pause = TRUE;
		}
	}

	if (should_pause) {
		/* Don't try to pause again */
		if (mf->private->low_battery_pause_cookie == 0) {
			mf->private->low_battery_pause_cookie =
				tracker_miner_pause (TRACKER_MINER (mf),
				                     _("Low battery"),
				                     NULL);
		}
	} else {
		/* Don't try to resume again */
		if (mf->private->low_battery_pause_cookie != 0) {
			tracker_miner_resume (TRACKER_MINER (mf),
			                      mf->private->low_battery_pause_cookie,
			                      NULL);
			mf->private->low_battery_pause_cookie = 0;
		}
	}

	set_up_throttle (mf, should_throttle);
}

static void
battery_status_cb (GObject    *object,
                   GParamSpec *pspec,
                   gpointer    user_data)
{
	TrackerMinerFiles *mf = user_data;

	check_battery_status (mf);
}

#endif /* defined(HAVE_DEVKIT_POWER) || defined(HAVE_HAL) */

static void
mount_pre_unmount_cb (GVolumeMonitor    *volume_monitor,
                      GMount            *mount,
                      TrackerMinerFiles *mf)
{
	GFile *mount_root;

	mount_root = g_mount_get_root (mount);
	tracker_miner_fs_directory_remove (TRACKER_MINER_FS (mf), mount_root);
	g_object_unref (mount_root);
}

static gboolean
disk_space_check (TrackerMinerFiles *mf)
{
	struct statvfs st;
	gint limit;
	gchar *data_dir;

	limit = tracker_config_get_low_disk_space_limit (mf->private->config);

	if (limit < 1) {
		return FALSE;
	}

	data_dir = g_build_filename (g_get_user_cache_dir (), "tracker", NULL);

	if (statvfs (data_dir, &st) == -1) {
		g_warning ("Could not statvfs() '%s'", data_dir);
		g_free (data_dir);
		return FALSE;
	}

	g_free (data_dir);

	if (((long long) st.f_bavail * 100 / st.f_blocks) <= limit) {
		g_message ("WARNING: Available disk space is below configured "
		           "threshold for acceptable working (%d%%)",
		           limit);
		return TRUE;
	}

	return FALSE;
}

static gboolean
disk_space_check_cb (gpointer user_data)
{
	TrackerMinerFiles *mf = user_data;

	if (disk_space_check (mf)) {
		/* Don't try to pause again */
		if (mf->private->disk_space_pause_cookie == 0) {
			mf->private->disk_space_pause_cookie =
				tracker_miner_pause (TRACKER_MINER (mf),
				                     _("Low disk space"),
				                     NULL);
		}
	} else {
		/* Don't try to resume again */
		if (mf->private->disk_space_pause_cookie != 0) {
			tracker_miner_resume (TRACKER_MINER (mf),
			                      mf->private->disk_space_pause_cookie,
			                      NULL);
			mf->private->disk_space_pause_cookie = 0;
		}
	}

	return TRUE;
}

static void
disk_space_check_start (TrackerMinerFiles *mf)
{
	gint limit;

	if (mf->private->disk_space_check_id != 0) {
		return;
	}

	limit = tracker_config_get_low_disk_space_limit (mf->private->config);

	if (limit != -1) {
		g_message ("Starting disk space check for every %d seconds",
		           DISK_SPACE_CHECK_FREQUENCY);
		mf->private->disk_space_check_id =
			g_timeout_add_seconds (DISK_SPACE_CHECK_FREQUENCY,
			                       disk_space_check_cb,
			                       mf);

		/* Call the function now too to make sure we have an
		 * initial value too!
		 */
		disk_space_check_cb (mf);
	} else {
		g_message ("Not setting disk space, configuration is set to -1 (disabled)");
	}
}

static void
disk_space_check_stop (TrackerMinerFiles *mf)
{
	if (mf->private->disk_space_check_id) {
		g_message ("Stopping disk space check");
		g_source_remove (mf->private->disk_space_check_id);
		mf->private->disk_space_check_id = 0;
	}
}

static void
low_disk_space_limit_cb (GObject    *gobject,
                         GParamSpec *arg1,
                         gpointer    user_data)
{
	TrackerMinerFiles *mf = user_data;

	disk_space_check_cb (mf);
}

static void
update_directories_from_new_config (TrackerMinerFS *mf,
                                    GSList         *new_dirs,
                                    GSList         *old_dirs,
                                    gboolean        recurse)
{
	TrackerMinerFilesPrivate *priv;
        GSList *sl;

	priv = TRACKER_MINER_FILES_GET_PRIVATE (mf);

        g_message ("Updating %s directories changed from configuration",
                   recurse ? "recursive" : "single");

        /* First remove all directories removed from the config */
        for (sl = old_dirs; sl; sl = sl->next) {
                const gchar *path;

                path = sl->data;

                /* If we are not still in the list, remove the dir */
                if (!tracker_string_in_gslist (path, new_dirs)) {
                        GFile *file;

                        g_message ("  Removing directory:'%s'", path);

                        file = g_file_new_for_path (path);
                        tracker_miner_fs_directory_remove (TRACKER_MINER_FS (mf), file);
                        g_object_unref (file);
                }
        }

        /* Second add directories which are new */
        for (sl = new_dirs; sl; sl = sl->next) {
                const gchar *path;

                path = sl->data;

                /* If we are now in the list, add the dir */
                if (!tracker_string_in_gslist (path, old_dirs)) {
                        GFile *file;

                        g_message ("  Adding directory:'%s'", path);

                        file = g_file_new_for_path (path);
			g_object_set_qdata (G_OBJECT (file),
			                    priv->quark_directory_config_root,
			                    GINT_TO_POINTER (TRUE));

			tracker_miner_fs_directory_add (TRACKER_MINER_FS (mf), file, recurse);
                        g_object_unref (file);
                }
        }
}

static void
index_recursive_directories_cb (GObject    *gobject,
                                GParamSpec *arg1,
                                gpointer    user_data)
{
        TrackerMinerFilesPrivate *private;
        GSList *new_dirs, *old_dirs;

        private = TRACKER_MINER_FILES_GET_PRIVATE (user_data);

        new_dirs = tracker_config_get_index_recursive_directories (private->config);
        old_dirs = private->index_recursive_directories;

        update_directories_from_new_config (TRACKER_MINER_FS (user_data),
                                            new_dirs,
                                            old_dirs,
                                            TRUE);

        /* Re-set the stored config in case it changes again */
        if (private->index_recursive_directories) {
                g_slist_foreach (private->index_recursive_directories, (GFunc) g_free, NULL);
                g_slist_free (private->index_recursive_directories);
        }

        private->index_recursive_directories = tracker_gslist_copy_with_string_data (new_dirs);
}

static void
index_single_directories_cb (GObject    *gobject,
                             GParamSpec *arg1,
                             gpointer    user_data)
{
        TrackerMinerFilesPrivate *private;
        GSList *new_dirs, *old_dirs;

        private = TRACKER_MINER_FILES_GET_PRIVATE (user_data);

        new_dirs = tracker_config_get_index_single_directories (private->config);
        old_dirs = private->index_single_directories;

        update_directories_from_new_config (TRACKER_MINER_FS (user_data),
                                            new_dirs,
                                            old_dirs,
                                            FALSE);

        /* Re-set the stored config in case it changes again */
        if (private->index_single_directories) {
                g_slist_foreach (private->index_single_directories, (GFunc) g_free, NULL);
                g_slist_free (private->index_single_directories);
        }

        private->index_single_directories = tracker_gslist_copy_with_string_data (new_dirs);
}

static gboolean
miner_files_force_recheck_idle (gpointer user_data)
{
	TrackerMinerFiles *miner_files = user_data;

	/* Recheck all directories for compliance with the new config */
	tracker_miner_fs_force_recheck (TRACKER_MINER_FS (miner_files));

	miner_files->private->force_recheck_id = 0;

	return FALSE;
}

static void
ignore_directories_cb (GObject    *gobject,
                       GParamSpec *arg1,
                       gpointer    user_data)
{
	TrackerMinerFiles *miner_files = user_data;

	if (miner_files->private->force_recheck_id == 0) {
		/* Set idle so multiple changes in the config lead to one recheck */
		miner_files->private->force_recheck_id =
			g_idle_add (miner_files_force_recheck_idle, miner_files);
	}
}

static gboolean
miner_files_check_file (TrackerMinerFS *fs,
                        GFile          *file)
{
	TrackerMinerFiles *mf;

	/* Check module file ignore patterns */
	mf = TRACKER_MINER_FILES (fs);

	if (G_UNLIKELY (!mf->private->config)) {
		return TRUE;
	}

	return tracker_miner_files_check_file (file,
	                                       tracker_config_get_ignored_file_paths (mf->private->config),
	                                       tracker_config_get_ignored_file_patterns (mf->private->config));
}

static gboolean
miner_files_check_directory (TrackerMinerFS *fs,
                             GFile          *file)
{
	TrackerMinerFiles *mf;

	/* Check module file ignore patterns */
	mf = TRACKER_MINER_FILES (fs);

	if (G_UNLIKELY (!mf->private->config)) {
		return TRUE;
	}

	return tracker_miner_files_check_directory (file,
	                                            tracker_config_get_index_recursive_directories (mf->private->config),
	                                            tracker_config_get_index_single_directories (mf->private->config),
	                                            tracker_config_get_ignored_directory_paths (mf->private->config),
	                                            tracker_config_get_ignored_directory_patterns (mf->private->config));


}

static gboolean
miner_files_check_directory_contents (TrackerMinerFS *fs,
                                      GFile          *parent,
                                      GList          *children)
{
	TrackerMinerFiles *mf;

	mf = TRACKER_MINER_FILES (fs);

	if (G_UNLIKELY (!mf->private->config)) {
		return TRUE;
	}

	return tracker_miner_files_check_directory_contents (parent,
	                                                     children,
	                                                     tracker_config_get_ignored_directories_with_content (mf->private->config));
}

static gboolean
miner_files_monitor_directory (TrackerMinerFS *fs,
                               GFile          *file)
{
	TrackerMinerFiles *mf;

	mf = TRACKER_MINER_FILES (fs);

	if (G_UNLIKELY (!mf->private->config)) {
		return TRUE;
	}

	return tracker_miner_files_monitor_directory (file,
	                                              tracker_config_get_enable_monitors (mf->private->config),
	                                              mf->private->index_single_directories);
}

static const gchar *
miner_files_get_file_urn (TrackerMinerFiles *miner,
                          GFile             *file,
			  gboolean          *is_iri)
{
	const gchar *urn;

	urn = tracker_miner_fs_get_urn (TRACKER_MINER_FS (miner), file);
	*is_iri = TRUE;

	if (!urn) {
		/* This is a new insertion, use anonymous URNs to store files */
		urn = "_:file";
		*is_iri = FALSE;
	}

	return urn;
}

static void
miner_files_add_to_datasource (TrackerMinerFiles    *mf,
                               GFile                *file,
                               TrackerSparqlBuilder *sparql)
{
	TrackerMinerFilesPrivate *priv;
	const gchar *removable_device_uuid;
	gchar *removable_device_urn, *uri;
	const gchar *urn;
	gboolean is_iri;

	priv = TRACKER_MINER_FILES_GET_PRIVATE (mf);
	uri = g_file_get_uri (file);

	removable_device_uuid = tracker_storage_get_uuid_for_file (priv->storage, file);

	if (removable_device_uuid) {
		removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s",
		                                        removable_device_uuid);
	} else {
		removable_device_urn = g_strdup (TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN);
	}

	urn = miner_files_get_file_urn (mf, file, &is_iri);

	if (is_iri) {
		tracker_sparql_builder_subject_iri (sparql, urn);
	} else {
		tracker_sparql_builder_subject (sparql, urn);
	}

	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:FileDataObject");

	tracker_sparql_builder_predicate (sparql, "nie:dataSource");
	tracker_sparql_builder_object_iri (sparql, removable_device_urn);

	tracker_sparql_builder_predicate (sparql, "tracker:available");
	tracker_sparql_builder_object_boolean (sparql, TRUE);

	g_free (removable_device_urn);
	g_free (uri);
}

static void
process_file_data_free (ProcessFileData *data)
{
	g_signal_handlers_disconnect_by_func (data->cancellable,
	                                      extractor_get_embedded_metadata_cancel,
	                                      data);

	g_object_unref (data->miner);
	g_object_unref (data->sparql);
	g_object_unref (data->cancellable);
	g_object_unref (data->file);
	g_slice_free (ProcessFileData, data);
}

static DBusGProxy *
extractor_create_proxy (void)
{
	DBusGProxy *proxy;
	DBusGConnection *connection;
	GError *error = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return FALSE;
	}

	/* Get proxy for the extractor */
	proxy = dbus_g_proxy_new_for_name (connection,
	                                   "org.freedesktop.Tracker1.Extract",
	                                   "/org/freedesktop/Tracker1/Extract",
	                                   "org.freedesktop.Tracker1.Extract");

	if (!proxy) {
		g_critical ("Could not create a DBusGProxy to the extract service");
	}

	return proxy;
}

static void
extractor_get_embedded_metadata_cb (DBusGProxy *proxy,
                                    gchar      *preupdate,
                                    gchar      *sparql,
                                    GError     *error,
                                    gpointer    user_data)
{
	TrackerMinerFilesPrivate *priv;
	ProcessFileData *data = user_data;
	const gchar *uuid;

	priv = TRACKER_MINER_FILES_GET_PRIVATE (data->miner);

	if (error) {
		/* Something bad happened, notify about the error */
		tracker_miner_fs_file_notify (TRACKER_MINER_FS (data->miner), data->file, error);
		process_file_data_free (data);
		g_error_free (error);
		return;
	}

	if (sparql && *sparql) {
		gboolean is_iri;
		const gchar *urn;

		urn = miner_files_get_file_urn (data->miner, data->file, &is_iri);

		if (is_iri) {
			gchar *str;

			str = g_strdup_printf ("<%s>", urn);
			tracker_sparql_builder_append (data->sparql, str);
			g_free (str);
		} else {
			tracker_sparql_builder_append (data->sparql, urn);
		}

		tracker_sparql_builder_append (data->sparql, sparql);
	}

	tracker_sparql_builder_insert_close (data->sparql);

	/* Prepend preupdate queries */
	if (preupdate && *preupdate) {
		tracker_sparql_builder_prepend (data->sparql, preupdate);
	}

	uuid = g_object_get_qdata (G_OBJECT (data->file),
				  data->miner->private->quark_mount_point_uuid);

	/* File represents a mount point */
	if (G_UNLIKELY (uuid)) {
		GString *queries;
		gchar *removable_device_urn, *uri;

		removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", uuid);
		uri = g_file_get_uri (G_FILE (data->file));

		queries = g_string_new ("");
		g_string_append_printf (queries,
		                        "DELETE FROM <%s> { "
		                        "  <%s> tracker:mountPoint ?unknown "
		                        "} WHERE { "
		                        "  <%s> a tracker:Volume; "
		                        "       tracker:mountPoint ?unknown "
		                        "} ",
		                        removable_device_urn, removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT INTO <%s> { "
		                        "  <%s> a tracker:Volume; "
		                        "       tracker:mountPoint ?u "
		                        "} WHERE { "
		                        "  ?u a nfo:FileDataObject; "
		                        "     nie:url \"%s\" "
		                        "}",
		                        removable_device_urn, removable_device_urn, uri);

		tracker_sparql_builder_append (data->sparql, queries->str);
		g_string_free (queries, TRUE);
		g_free (removable_device_urn);
		g_free (uri);
	}

	/* Notify about the success */
	tracker_miner_fs_file_notify (TRACKER_MINER_FS (data->miner), data->file, NULL);

	process_file_data_free (data);
	g_free (preupdate);
	g_free (sparql);
}

static void
extractor_get_embedded_metadata_cancel (GCancellable    *cancellable,
                                        ProcessFileData *data)
{
	GError *error;

	/* Cancel extractor call */
	dbus_g_proxy_cancel_call (data->miner->private->extractor_proxy,
	                          data->call);

	error = g_error_new_literal (miner_files_error_quark, 0, "Embedded metadata extraction was cancelled");
	tracker_miner_fs_file_notify (TRACKER_MINER_FS (data->miner), data->file, error);

	process_file_data_free (data);
	g_error_free (error);
}

static void
extractor_get_embedded_metadata (ProcessFileData *data,
                                 const gchar     *uri,
                                 const gchar     *mime_type)
{
	data->call = org_freedesktop_Tracker1_Extract_get_metadata_async (data->miner->private->extractor_proxy,
	                                                                  uri,
	                                                                  mime_type,
	                                                                  extractor_get_embedded_metadata_cb,
	                                                                  data);
	g_signal_connect (data->cancellable, "cancelled",
	                  G_CALLBACK (extractor_get_embedded_metadata_cancel), data);
}

static void
process_file_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	TrackerMinerFilesPrivate *priv;
	TrackerSparqlBuilder *sparql;
	ProcessFileData *data;
	const gchar *mime_type, *urn, *parent_urn;
	GFileInfo *file_info;
	guint64 time_;
	GFile *file;
	gchar *uri;
	GError *error = NULL;
	gboolean is_iri;

	data = user_data;
	file = G_FILE (object);
	sparql = data->sparql;
	priv = TRACKER_MINER_FILES_GET_PRIVATE (data->miner);
	file_info = g_file_query_info_finish (file, result, &error);

	if (error) {
		/* Something bad happened, notify about the error */
		tracker_miner_fs_file_notify (TRACKER_MINER_FS (data->miner), file, error);
		process_file_data_free (data);
		g_error_free (error);
		return;
	}

	uri = g_file_get_uri (file);
	mime_type = g_file_info_get_content_type (file_info);
	urn = miner_files_get_file_urn (TRACKER_MINER_FILES (data->miner), file, &is_iri);

	tracker_sparql_builder_insert_open (sparql, uri);

	if (is_iri) {
		tracker_sparql_builder_subject_iri (sparql, urn);
	} else {
		tracker_sparql_builder_subject (sparql, urn);
	}

	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:FileDataObject");
	tracker_sparql_builder_object (sparql, "nie:InformationElement");

	if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
		tracker_sparql_builder_object (sparql, "nfo:Folder");
	}

	parent_urn = tracker_miner_fs_get_parent_urn (TRACKER_MINER_FS (data->miner), file);

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

	miner_files_add_to_datasource (data->miner, file, sparql);

	/* Next step, getting embedded metadata */
	extractor_get_embedded_metadata (data, uri, mime_type);

	g_object_unref (file_info);
	g_free (uri);
}

static gboolean
miner_files_process_file (TrackerMinerFS       *fs,
                          GFile                *file,
                          TrackerSparqlBuilder *sparql,
                          GCancellable         *cancellable)
{
	ProcessFileData *data;
	const gchar *attrs;

	data = g_slice_new0 (ProcessFileData);
	data->miner = g_object_ref (fs);
	data->cancellable = g_object_ref (cancellable);
	data->sparql = g_object_ref (sparql);
	data->file = g_object_ref (file);

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
miner_files_ignore_next_update_file (TrackerMinerFS       *fs,
                                     GFile                *file,
                                     TrackerSparqlBuilder *sparql,
                                     GCancellable         *cancellable)
{
	const gchar *attrs;
	const gchar *mime_type;
	GFileInfo *file_info;
	guint64 time_;
	gchar *uri;
	GError *error = NULL;

	attrs = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
		G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
		G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
		G_FILE_ATTRIBUTE_STANDARD_SIZE ","
		G_FILE_ATTRIBUTE_TIME_MODIFIED ","
		G_FILE_ATTRIBUTE_TIME_ACCESS;

	file_info = g_file_query_info (file, attrs,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               cancellable, &error);

	if (error) {
		g_warning ("Can't ignore-next-update: '%s'", error->message);
		g_clear_error (&error);
		return FALSE;
	}

	uri = g_file_get_uri (file);
	mime_type = g_file_info_get_content_type (file_info);

	/* For ignore-next-update we only write a few properties back. These properties
	 * should NEVER be marked as tracker:writeback in the ontology! (else you break
	 * the tracker-writeback feature) */

	tracker_sparql_builder_insert_open (sparql, uri);

	tracker_sparql_builder_subject_variable (sparql, "urn");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:FileDataObject");

	tracker_sparql_builder_predicate (sparql, "nfo:fileSize");
	tracker_sparql_builder_object_int64 (sparql, g_file_info_get_size (file_info));

	time_ = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	tracker_sparql_builder_predicate (sparql, "nfo:fileLastModified");
	tracker_sparql_builder_object_date (sparql, (time_t *) &time_);

	time_ = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_ACCESS);
	tracker_sparql_builder_predicate (sparql, "nfo:fileLastAccessed");
	tracker_sparql_builder_object_date (sparql, (time_t *) &time_);

	tracker_sparql_builder_predicate (sparql, "nie:mimeType");
	tracker_sparql_builder_object_string (sparql, mime_type);

	tracker_sparql_builder_insert_close (sparql);

	tracker_sparql_builder_where_open (sparql);

	tracker_sparql_builder_subject_variable (sparql, "urn");
	tracker_sparql_builder_predicate (sparql, "nie:url");
	tracker_sparql_builder_object_string (sparql, uri);

	tracker_sparql_builder_where_close (sparql);

	g_object_unref (file_info);
	g_free (uri);

	return TRUE;
}


TrackerMiner *
tracker_miner_files_new (TrackerConfig *config)
{
	return g_object_new (TRACKER_TYPE_MINER_FILES,
	                     "name", "Files",
	                     "config", config,
	                     "process-pool-limit", 10,
	                     NULL);
}

gboolean
tracker_miner_files_check_file (GFile  *file,
                                GSList *ignored_file_paths,
                                GSList *ignored_file_patterns)
{
	GFileInfo *file_info;
	GSList *l;
	gchar *basename;
	gchar *path;
	gboolean should_process;

	file_info = NULL;
	should_process = FALSE;
	basename = NULL;
	path = NULL;

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL, NULL);

	if (file_info && g_file_info_get_is_hidden (file_info)) {
		/* Ignore hidden files */
		goto done;
	}


	path = g_file_get_path (file);

	for (l = ignored_file_paths; l; l = l->next) {
		if (strcmp (l->data, path) == 0) {
			goto done;
		}
	}

	basename = g_file_get_basename (file);

	for (l = ignored_file_patterns; l; l = l->next) {
		if (g_pattern_match_string (l->data, basename)) {
			goto done;
		}
	}

	should_process = TRUE;

done:
	g_free (basename);
	g_free (path);

	if (file_info) {
		g_object_unref (file_info);
	}

	return should_process;
}

gboolean
tracker_miner_files_check_directory (GFile  *file,
                                     GSList *index_recursive_directories,
                                     GSList *index_single_directories,
                                     GSList *ignored_directory_paths,
                                     GSList *ignored_directory_patterns)
{
	GFileInfo *file_info;
	GSList *l;
	gchar *basename;
	gchar *path;
	gboolean should_process;
	gboolean is_hidden;

	should_process = FALSE;
	basename = NULL;

	/* Most common things to ignore */
	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL, NULL);

	path = g_file_get_path (file);

	/* First we check the GIO hidden check. This does a number of
	 * things for us which is good (like checking ".foo" dirs).
	 */
	is_hidden = file_info && g_file_info_get_is_hidden (file_info);

	/* Second we check if the file is on FAT and if the hidden
	 * attribute is set. GIO does this but ONLY on a Windows OS,
	 * not for Windows files under a Linux OS, so we have to check
	 * anyway.
	 */
	if (!is_hidden) {
		int fd;

		fd = g_open (path, O_RDONLY, 0);
		if (fd != -1) {
			__u32 attrs;

			if (ioctl (fd, FAT_IOCTL_GET_ATTRIBUTES, &attrs) == 0) {
				is_hidden = attrs & ATTR_HIDDEN ? TRUE : FALSE;
			}

			close (fd);
		}
	}

	if (is_hidden) {
		/* FIXME: We need to check if the file is actually a
		 * config specified location before blanket ignoring
		 * all hidden files.
		 */
		if (tracker_string_in_gslist (path, index_recursive_directories)) {
			should_process = TRUE;
		}

		if (tracker_string_in_gslist (path, index_single_directories)) {
			should_process = TRUE;
		}

		/* Ignore hidden dirs */
		goto done;
	}

	for (l = ignored_directory_paths; l; l = l->next) {
		if (strcmp (l->data, path) == 0) {
			goto done;
		}
	}

	basename = g_file_get_basename (file);

	for (l = ignored_directory_patterns; l; l = l->next) {
		if (g_pattern_match_string (l->data, basename)) {
			goto done;
		}
	}

	/* Check module directory ignore patterns */
	should_process = TRUE;

 done:
	g_free (basename);
	g_free (path);

	if (file_info) {
		g_object_unref (file_info);
	}

	return should_process;
}

gboolean
tracker_miner_files_check_directory_contents (GFile  *parent,
                                              GList  *children,
                                              GSList *ignored_content)
{
	GSList *l;

	if (!ignored_content) {
		return TRUE;
	}

	while (children) {
		gchar *basename;

		basename = g_file_get_basename (children->data);

		for (l = ignored_content; l; l = l->next) {
			if (g_strcmp0 (basename, l->data) == 0) {
				gchar *parent_uri;

				parent_uri = g_file_get_uri (parent);
				/* g_debug ("Directory '%s' ignored since it contains a file named '%s'", */
				/*          parent_uri, basename); */

				g_free (parent_uri);
				g_free (basename);

				return FALSE;
			}
		}

		children = children->next;
		g_free (basename);
	}

	return TRUE;
}

gboolean
tracker_miner_files_monitor_directory (GFile    *file,
                                       gboolean  enable_monitors,
                                       GSList   *directories_to_check)
{
	if (!enable_monitors) {
		return FALSE;
	}

	/* We'll only get this signal for the directories where check_directory()
	 * and check_directory_contents() returned TRUE, so by default we want
	 * these directories to be indexed.
         */
	return TRUE;
}
