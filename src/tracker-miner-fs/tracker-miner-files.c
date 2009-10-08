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

#include <sys/statvfs.h>

#include <glib/gi18n.h>

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-power.h>
#include <libtracker-common/tracker-storage.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-miner/tracker-miner.h>

#include "tracker-miner-files.h"
#include "tracker-config.h"
#include "tracker-extract-client.h"
#include "tracker-thumbnailer.h"
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

	guint disk_space_check_id;
	guint disk_space_pause_cookie;

	guint low_battery_pause_cookie;

	DBusGProxy *extractor_proxy;
};

enum {
	VOLUME_MOUNTED_IN_STORE = 1 << 0,
	VOLUME_MOUNTED = 1 << 1
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

static void     mount_pre_unmount_cb          (GVolumeMonitor       *volume_monitor,
					       GMount               *mount,
					       TrackerMinerFiles    *mf);

#ifdef HAVE_HAL
static void     mount_point_added_cb          (TrackerStorage       *storage,
					       const gchar          *udi,
					       const gchar          *mount_point,
					       gpointer              user_data);
static void     mount_point_removed_cb        (TrackerStorage       *storage,
					       const gchar          *udi,
					       const gchar          *mount_point,
					       gpointer              user_data);
static void     on_battery_cb                 (GObject              *gobject,
					       GParamSpec           *arg1,
					       gpointer              user_data);
static void     on_low_battery_cb             (GObject              *object,
					       GParamSpec           *pspec,
					       gpointer              user_data);
static void     on_battery_percentage_cb      (GObject              *gobject,
					       GParamSpec           *arg1,
					       gpointer              user_data);
#endif

static void     init_mount_points             (TrackerMinerFiles *miner);
static void     disk_space_check_start        (TrackerMinerFiles    *mf);
static void     disk_space_check_stop         (TrackerMinerFiles    *mf);
static void     low_disk_space_limit_cb       (GObject              *gobject,
					       GParamSpec           *arg1,
					       gpointer              user_data);

static DBusGProxy * extractor_create_proxy    (void);
static void    extractor_queue_thumbnail_cb   (DBusGProxy           *proxy,
					       const gchar          *filename, 
					       const gchar          *mime_type,
					       gpointer              user_data);
	
static gboolean miner_files_check_file        (TrackerMinerFS       *fs,
					       GFile                *file);
static gboolean miner_files_check_directory   (TrackerMinerFS       *fs,
					       GFile                *file);
static gboolean miner_files_check_directory_contents (TrackerMinerFS       *fs,
						      GFile                *parent,
						      GList                *children);
static gboolean miner_files_process_file      (TrackerMinerFS       *fs,
					       GFile                *file,
					       TrackerSparqlBuilder *sparql,
					       GCancellable         *cancellable);
static gboolean miner_files_monitor_directory (TrackerMinerFS       *fs,
					       GFile                *file);

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

#ifdef HAVE_HAL
        priv->storage = tracker_storage_new ();

        g_signal_connect (priv->storage, "mount-point-added",
                          G_CALLBACK (mount_point_added_cb), 
			  mf);

	g_signal_connect (priv->storage, "mount-point-removed",
	                  G_CALLBACK (mount_point_removed_cb), 
	                  mf);

	priv->power = tracker_power_new ();

	g_signal_connect (priv->power, "notify::on-low-battery",
			  G_CALLBACK (on_low_battery_cb),
			  mf);
	g_signal_connect (priv->power, "notify::on-battery",
			  G_CALLBACK (on_battery_cb),
			  mf);
	g_signal_connect (priv->power, "notify::battery-percentage",
			  G_CALLBACK (on_battery_percentage_cb),
			  mf);
#endif /* HAVE_HAL */

	priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect (priv->volume_monitor, "mount-pre-unmount",
			  G_CALLBACK (mount_pre_unmount_cb),
			  mf);

	/* Set up extractor and signals */
	priv->extractor_proxy = extractor_create_proxy ();

	dbus_g_object_register_marshaller (tracker_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE,
					   G_TYPE_STRING,
					   G_TYPE_STRING,
					   G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->extractor_proxy, "QueueThumbnail",
				 G_TYPE_STRING,
				 G_TYPE_STRING, 
				 G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (priv->extractor_proxy, "QueueThumbnail",
				     G_CALLBACK (extractor_queue_thumbnail_cb),
				     NULL, NULL);
	
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

	dbus_g_proxy_disconnect_signal (priv->extractor_proxy, "QueueThumbnail",
					G_CALLBACK (extractor_queue_thumbnail_cb),
					NULL);
	
	g_object_unref (priv->extractor_proxy);

	g_signal_handlers_disconnect_by_func (priv->config,
					      low_disk_space_limit_cb,
					      NULL);

	g_object_unref (priv->config);

	disk_space_check_stop (TRACKER_MINER_FILES (object));

#ifdef HAVE_HAL
	g_signal_handlers_disconnect_by_func (priv->power,
					      on_battery_percentage_cb,
					      mf);
	g_signal_handlers_disconnect_by_func (priv->power,
					      on_battery_cb,
					      mf);
	g_signal_handlers_disconnect_by_func (priv->power,
					      on_low_battery_cb,
					      mf);
        g_object_unref (priv->power);

	g_signal_handlers_disconnect_by_func (priv->storage,
					      mount_point_added_cb,
					      mf);

        g_object_unref (priv->storage);
#endif /* HAVE_HAL */

	g_signal_handlers_disconnect_by_func (priv->volume_monitor,
					      mount_pre_unmount_cb,
					      object);
	g_object_unref (priv->volume_monitor);

        G_OBJECT_CLASS (tracker_miner_files_parent_class)->finalize (object);
}

static void
miner_files_constructed (GObject *object)
{
        TrackerMinerFiles *mf;
        TrackerMinerFS *fs;
        GSList *dirs;
	GSList *mounts = NULL, *m;
	gint throttle;

	G_OBJECT_CLASS (tracker_miner_files_parent_class)->constructed (object);

        mf = TRACKER_MINER_FILES (object);
        fs = TRACKER_MINER_FS (object);

        if (!mf->private->config) {
                g_critical ("No config. This is mandatory");
                g_assert_not_reached ();
        }

#ifdef HAVE_HAL
        if (tracker_config_get_index_removable_devices (mf->private->config)) {
                mounts = tracker_storage_get_removable_device_roots (mf->private->storage);
        }
#endif /* HAVE_HAL */

	g_message ("Setting up directories to iterate from config (IndexSingleDirectory)");

        /* Fill in directories to inspect */
        dirs = tracker_config_get_index_single_directories (mf->private->config);

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
                tracker_miner_fs_add_directory (fs, file, FALSE);
		g_object_unref (file);
        }

	g_message ("Setting up directories to iterate from config (IndexRecursiveDirectory)");

        dirs = tracker_config_get_index_recursive_directories (mf->private->config);

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
                tracker_miner_fs_add_directory (fs, file, TRUE);
		g_object_unref (file);
        }

	throttle = tracker_config_get_throttle (mf->private->config);

	/* Throttle in config goes from 0 to 20, translate to 0.0 -> 1.0 */
	tracker_miner_fs_set_throttle (TRACKER_MINER_FS (mf), (1.0 / 20) * throttle);

	/* Add removable media */
	g_message ("Setting up directories to iterate which are removable devices");

	for (m = mounts; m; m = m->next) {
		GFile *file = g_file_new_for_path (m->data);
		
		g_message ("  Adding:'%s'", (gchar*) m->data);
		tracker_miner_fs_add_directory (TRACKER_MINER_FS (mf),
						file, 
						TRUE);
		g_object_unref (file);
	}

	g_signal_connect (mf->private->config, "notify::low-disk-space-limit",
			  G_CALLBACK (low_disk_space_limit_cb),
			  mf);

	g_slist_foreach (mounts, (GFunc) g_free, NULL);
	g_slist_free (mounts);

	disk_space_check_start (mf);
}

static void
set_up_mount_point (TrackerMinerFiles *miner,
                    const gchar       *removable_device_urn,
                    const gchar       *mount_point,
                    gboolean           mounted,
                    GString           *accumulator)
{
	GString *queries;
	GError *error = NULL;

	g_debug ("Setting up mount point '%s'", removable_device_urn);

	queries = g_string_new (NULL);

	if (mounted) {
		if (mount_point) {
			GFile *file;
			gchar *uri;

			file = g_file_new_for_path (mount_point);
			uri = g_file_get_uri (file);

			g_string_append_printf (queries,
			                        "DROP GRAPH <%s>\nINSERT { <%s> a tracker:Volume; tracker:mountPoint <%s> } ",
			                        removable_device_urn, removable_device_urn, uri);

			g_object_unref (file);
			g_free (uri);
		}

		g_string_append_printf (queries,
		                        "DELETE { <%s> tracker:isMounted ?unknown } WHERE { <%s> a tracker:Volume; tracker:isMounted ?unknown } ",
		                        removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT { <%s> a tracker:Volume; tracker:isMounted true } ",
		                        removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT { ?do tracker:available true } WHERE { ?do nie:dataSource <%s> } ",
		                        removable_device_urn);
	} else {
		gchar *now;

		now = tracker_date_to_string (time (NULL));

		g_string_append_printf (queries,
		                        "DELETE { <%s> tracker:unmountDate ?unknown } WHERE { <%s> a tracker:Volume; tracker:unmountDate ?unknown } ",
		                        removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT { <%s> a tracker:Volume; tracker:unmountDate \"%s\" } ",
		                        removable_device_urn, now);

		g_string_append_printf (queries,
		                        "DELETE { <%s> tracker:isMounted ?unknown } WHERE { <%s> a tracker:Volume; tracker:isMounted ?unknown } ",
		                        removable_device_urn, removable_device_urn);

		g_string_append_printf (queries,
		                        "INSERT { <%s> a tracker:Volume; tracker:isMounted false } ",
		                        removable_device_urn);

		g_string_append_printf (queries,
		                        "DELETE { ?do tracker:available true } WHERE { ?do nie:dataSource <%s> } ",
		                        removable_device_urn);
		g_free (now);
	}

	if (accumulator) {
		g_string_append_printf (accumulator, "%s ", queries->str);
	} else {

		tracker_miner_execute_update (TRACKER_MINER (miner), queries->str, &error);

		if (error) {
			g_critical ("Could not set up mount point '%s': %s",
			            removable_device_urn, error->message);
			g_error_free (error);
		}
	}

	g_string_free (queries, TRUE);
}

static void
init_mount_points (TrackerMinerFiles *miner)
{
	TrackerMinerFilesPrivate *priv;
	GHashTable *volumes;
	GHashTableIter iter;
	gpointer key, value;
	GPtrArray *sparql_result;
	GError *error = NULL;
	GString *accumulator;
	gint i;
#ifdef HAVE_HAL
	GSList *udis, *u;
#endif

	priv = TRACKER_MINER_FILES_GET_PRIVATE (miner);

	g_debug ("Initializing mount points");

	volumes = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                 (GDestroyNotify) g_free,
	                                 NULL);

	/* First, get all mounted volumes, according to tracker-store */
	sparql_result = tracker_miner_execute_sparql (TRACKER_MINER (miner),
	                                              "SELECT ?v WHERE { ?v a tracker:Volume ; tracker:isMounted true }",
	                                              &error);

	if (error) {
		g_critical ("Could not obtain the mounted volumes");
		g_error_free (error);
		return;
	}

	for (i = 0; i < sparql_result->len; i++) {
		gchar **row;
		gint state;

		row = g_ptr_array_index (sparql_result, i);
		state = VOLUME_MOUNTED_IN_STORE;

		if (strcmp (row[0], TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN) == 0) {
			/* Report non-removable media to be mounted by HAL as well */
			state |= VOLUME_MOUNTED;
		}

		g_hash_table_insert (volumes, g_strdup (row[0]), GINT_TO_POINTER (state));
	}

	g_ptr_array_foreach (sparql_result, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (sparql_result, TRUE);

	g_hash_table_replace (volumes, g_strdup (TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN),
	                      GINT_TO_POINTER (VOLUME_MOUNTED));

#ifdef HAVE_HAL
	udis = tracker_storage_get_removable_device_udis (priv->storage);

	/* Then, get all currently mounted volumes, according to HAL */
	for (u = udis; u; u = u->next) {
		const gchar *udi;
		gchar *removable_device_urn;
		gint state;

		udi = u->data;
		removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", udi);

		state = GPOINTER_TO_INT (g_hash_table_lookup (volumes, removable_device_urn));
		state |= VOLUME_MOUNTED;

		g_hash_table_replace (volumes, removable_device_urn, GINT_TO_POINTER (state));
	}

	g_slist_foreach (udis, (GFunc) g_free, NULL);
	g_slist_free (udis);
#endif

	accumulator = g_string_new (NULL);
	g_hash_table_iter_init (&iter, volumes);

	/* Finally, set up volumes based on the composed info */
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *urn = key;
		gint state = GPOINTER_TO_INT (value);

		if ((state & VOLUME_MOUNTED) &&
		    !(state & VOLUME_MOUNTED_IN_STORE)) {
			const gchar *mount_point = NULL;

#ifdef HAVE_HAL
			if (g_str_has_prefix (urn, TRACKER_DATASOURCE_URN_PREFIX)) {
				const gchar *udi;

				udi = urn + strlen (TRACKER_DATASOURCE_URN_PREFIX);
				mount_point = tracker_storage_udi_get_mount_point (priv->storage, udi);
			}
#endif

			g_debug ("URN '%s' (mount point: %s) was not reported to be mounted, but now it is, updating state",
			         mount_point, urn);
			set_up_mount_point (miner, urn, mount_point, TRUE, accumulator);
		} else if (!(state & VOLUME_MOUNTED) &&
			   (state & VOLUME_MOUNTED_IN_STORE)) {
			g_debug ("URN '%s' was reported to be mounted, but it isn't anymore, updating state", urn);
			set_up_mount_point (miner, urn, NULL, FALSE, accumulator);
		}
	}

	if (accumulator->str[0] != '\0') {
		tracker_miner_execute_update (TRACKER_MINER (miner), accumulator->str, &error);

		if (error) {
			g_critical ("Could not initialize currently active mount points: %s",
			            error->message);
			g_error_free (error);
		}
	}

	g_string_free (accumulator, TRUE);
	g_hash_table_unref (volumes);
}

#ifdef HAVE_HAL

static void
mount_point_removed_cb (TrackerStorage *storage,
			const gchar    *udi,
			const gchar    *mount_point,
			gpointer        user_data)
{
	TrackerMinerFiles *miner = user_data;
	gchar *urn;

	g_debug ("Removing mount point '%s'", mount_point);

	urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", udi);

	set_up_mount_point (miner, urn, mount_point, FALSE, NULL);
	g_free (urn);
}

static void
mount_point_added_cb (TrackerStorage *storage,
                      const gchar    *udi,
                      const gchar    *mount_point,
                      gpointer        user_data)
{
	TrackerMinerFiles *miner = user_data;
	TrackerMinerFilesPrivate *priv;
	gchar *urn;
        gboolean index_removable_devices;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (miner);

        index_removable_devices = tracker_config_get_index_removable_devices (priv->config);

        if (index_removable_devices) {
		GFile *file;

		file = g_file_new_for_path (mount_point);
                tracker_miner_fs_add_directory (TRACKER_MINER_FS (user_data),
						file,
						TRUE);
		g_object_unref (file);
        }

	g_debug ("Configuring added mount point '%s'", mount_point);

	urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", udi);

	set_up_mount_point (miner, urn, mount_point, TRUE, NULL);
	g_free (urn);
}

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
on_battery_cb (GObject    *gobject,
	       GParamSpec *arg1,
	       gpointer    user_data)
{
	TrackerMinerFiles *mf = user_data;
	gboolean on_battery;

	set_up_throttle (mf, TRUE);
	on_battery = tracker_power_get_on_battery (mf->private->power);

	if (on_battery) {
		/* We only pause on low battery */
		return;
	}

	/* Resume if we are not on battery */
	if (mf->private->low_battery_pause_cookie != 0) {
		tracker_miner_resume (TRACKER_MINER (mf),
				      mf->private->low_battery_pause_cookie,
				      NULL);
		mf->private->low_battery_pause_cookie = 0;
	}
}

static void
on_low_battery_cb (GObject    *object,
		   GParamSpec *pspec,
		   gpointer    user_data)
{
	TrackerMinerFiles *mf = user_data;
	gboolean on_low_battery;
	gboolean on_battery;
	gboolean should_pause = FALSE;

	on_low_battery = tracker_power_get_on_low_battery (mf->private->power);
	on_battery = tracker_power_get_on_battery (mf->private->power);

	if (on_battery && on_low_battery) {
		gdouble percentage;

		should_pause = TRUE;
		percentage = tracker_power_get_battery_percentage (mf->private->power);

		g_message ("WARNING: Available battery power is getting low (%.0f%%)",
			   percentage * 100);
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

	set_up_throttle (mf, FALSE);
}

static void
on_battery_percentage_cb (GObject    *gobject,
			  GParamSpec *arg1,
			  gpointer    user_data)
{
	TrackerMinerFiles *mf = user_data;
	gdouble percentage;

	percentage = tracker_power_get_battery_percentage (mf->private->power);

	g_message ("Battery percentage is now %.0f%%",
		   percentage * 100);
}

#endif /* HAVE_HAL */

static void
mount_pre_unmount_cb (GVolumeMonitor    *volume_monitor,
		      GMount            *mount,
		      TrackerMinerFiles *mf)
{
	GFile *mount_root;

	mount_root = g_mount_get_root (mount);
	tracker_miner_fs_remove_directory (TRACKER_MINER_FS (mf), mount_root);
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

static gboolean
miner_files_check_file (TrackerMinerFS *fs,
			GFile          *file)
{
	TrackerMinerFiles *mf;
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

	/* Check module file ignore patterns */
	mf = TRACKER_MINER_FILES (fs);

	path = g_file_get_path (file);

	for (l = tracker_config_get_ignored_file_paths (mf->private->config); l; l = l->next) {
		if (strcmp (l->data, path) == 0) {
			goto done;
		}
	}

	basename = g_file_get_basename (file);
	
	for (l = tracker_config_get_ignored_file_patterns (mf->private->config); l; l = l->next) {
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

static gboolean
miner_files_check_directory (TrackerMinerFS *fs,
			     GFile          *file)
{
	TrackerMinerFiles *mf;
	GFileInfo *file_info;
	GSList *l;
	gchar *basename;
	gchar *path;
	gboolean should_process;

	should_process = FALSE;
	basename = NULL;

	/* Most common things to ignore */
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, NULL);

	path = g_file_get_path (file);

	if (file_info && g_file_info_get_is_hidden (file_info)) {
		TrackerMinerFiles *mf;
		GSList *allowed_directories;

		mf = TRACKER_MINER_FILES (fs);

		/* FIXME: We need to check if the file is actually a
		 * config specified location before blanket ignoring
		 * all hidden files. 
		 */
		allowed_directories = 
			tracker_config_get_index_recursive_directories (mf->private->config);

		if (tracker_string_in_gslist (path, allowed_directories)) {
			should_process = TRUE;
		}

		allowed_directories = 
			tracker_config_get_index_single_directories (mf->private->config);

		if (tracker_string_in_gslist (path, allowed_directories)) {
			should_process = TRUE;
		}

		/* Ignore hidden dirs */
		goto done;
	}

	mf = TRACKER_MINER_FILES (fs);

	for (l = tracker_config_get_ignored_directory_paths (mf->private->config); l; l = l->next) {
		if (strcmp (l->data, path) == 0) {
			goto done;
		}
	}

	basename = g_file_get_basename (file);

	for (l = tracker_config_get_ignored_file_patterns (mf->private->config); l; l = l->next) {
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

static gboolean
miner_files_check_directory_contents (TrackerMinerFS *fs,
				      GFile          *parent,
				      GList          *children)
{
	TrackerMinerFiles *mf;
	GSList *ignored_content, *l;

	mf = TRACKER_MINER_FILES (fs);
	ignored_content = tracker_config_get_ignored_directories_with_content (mf->private->config);

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
				g_debug ("Directory '%s' ignored since it contains a file named '%s'",
					 parent_uri, basename);

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

static gboolean
miner_files_monitor_directory (TrackerMinerFS *fs,
			       GFile          *file)
{
	TrackerMinerFiles *mf;

	mf = TRACKER_MINER_FILES (fs);

	if (!tracker_config_get_enable_monitors (mf->private->config)) {
		return FALSE;
	}
		
	/* Fallback to the check directory routine, since we don't
	 * monitor anything we don't process. 
	 */
	return miner_files_check_directory (fs, file);
}

static void
miner_files_add_to_datasource (TrackerMinerFiles    *mf,
			       GFile                *file,
			       TrackerSparqlBuilder *sparql)
{
        TrackerMinerFilesPrivate *priv;
	const gchar *removable_device_udi;
	gchar *removable_device_urn, *uri;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (mf);
	uri = g_file_get_uri (file);

#ifdef HAVE_HAL
	removable_device_udi = tracker_storage_get_volume_udi_for_file (priv->storage, file);
#else  /* HAVE_HAL */
	removable_device_udi = NULL;
#endif /* HAVE_HAL */

	if (removable_device_udi) {
		removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s",
						        removable_device_udi);
	} else {
		removable_device_urn = g_strdup (TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN);
	}

	tracker_sparql_builder_subject_iri (sparql, uri);
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
extractor_queue_thumbnail_cb (DBusGProxy  *proxy,
			      const gchar *filename, 
			      const gchar *mime_type,
			      gpointer     user_data)
{
	tracker_thumbnailer_queue_add (filename, mime_type);
}

static void
extractor_get_embedded_metadata_cb (DBusGProxy *proxy,
				    gchar      *sparql,
				    GError     *error,
				    gpointer    user_data)
{
	ProcessFileData *data = user_data;

	if (error) {
		/* Something bad happened, notify about the error */
		tracker_miner_fs_notify_file (TRACKER_MINER_FS (data->miner), data->file, error);
		process_file_data_free (data);
		g_error_free (error);
		return;
	}

	if (sparql) {
		tracker_sparql_builder_insert_close (data->sparql);
		tracker_sparql_builder_append (data->sparql, sparql);
		g_free (sparql);
	}

	/* Notify about the success */
	tracker_miner_fs_notify_file (TRACKER_MINER_FS (data->miner), data->file, NULL);
	process_file_data_free (data);
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
	tracker_miner_fs_notify_file (TRACKER_MINER_FS (data->miner), data->file, error);

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
	TrackerSparqlBuilder *sparql;
	ProcessFileData *data;
	const gchar *mime_type;
	GFileInfo *file_info;
	guint64 time_;
	GFile *file, *parent;
	gchar *uri, *parent_uri;
	GError *error = NULL;

	data = user_data;
	file = G_FILE (object);
	sparql = data->sparql;
	file_info = g_file_query_info_finish (file, result, &error);

	if (error) {
		/* Something bad happened, notify about the error */
		tracker_miner_fs_notify_file (TRACKER_MINER_FS (data->miner), file, error);
		process_file_data_free (data);
		return;
	}

	uri = g_file_get_uri (file);
	mime_type = g_file_info_get_content_type (file_info);

        tracker_sparql_builder_insert_open (sparql);

        tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:FileDataObject");

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

	tracker_sparql_builder_predicate (sparql, "nfo:fileSize");
	tracker_sparql_builder_object_int64 (sparql, g_file_info_get_size (file_info));

	time_ = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	tracker_sparql_builder_predicate (sparql, "nfo:fileLastModified");
	tracker_sparql_builder_object_date (sparql, (time_t *) &time_);

	time_ = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_ACCESS);
        tracker_sparql_builder_predicate (sparql, "nfo:fileLastAccessed");
	tracker_sparql_builder_object_date (sparql, (time_t *) &time_);

	tracker_sparql_builder_subject_iri (sparql, uri); /* Change to URN */
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nie:InformationElement");

	if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
		tracker_sparql_builder_object (sparql, "nfo:Folder");
	}

	tracker_sparql_builder_predicate (sparql, "nie:isStoredAs");
	tracker_sparql_builder_object_iri (sparql, uri);

	tracker_sparql_builder_predicate (sparql, "nie:mimeType");
	tracker_sparql_builder_object_string (sparql, mime_type);

        miner_files_add_to_datasource (data->miner, file, sparql);

	/* Send file/mime data to thumbnailer (which adds it to the
	 * queue if the thumbnailer handles those mime types).
	 */
	tracker_thumbnailer_queue_add (uri, mime_type);

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

TrackerMiner *
tracker_miner_files_new (TrackerConfig *config)
{
        return g_object_new (TRACKER_TYPE_MINER_FILES,
                             "name", "Files",
                             "config", config,
                             NULL);
}
