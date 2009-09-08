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

#define DISK_SPACE_CHECK_FREQUENCY 10

#define TRACKER_MINER_FILES_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_FILES, TrackerMinerFilesPrivate))

static GQuark miner_files_error_quark = 0;

typedef struct ProcessFileData ProcessFileData;

struct ProcessFileData {
	TrackerMinerFiles *miner;
	TrackerSparqlBuilder *sparql;
	TrackerMinerFSDoneCb callback;
	gpointer callback_data;
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
static void     initialize_removable_devices  (TrackerMinerFiles    *mf);

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

static void     disk_space_check_start        (TrackerMinerFiles    *mf);
static void     disk_space_check_stop         (TrackerMinerFiles    *mf);
static void     low_disk_space_limit_cb       (GObject              *gobject,
					       GParamSpec           *arg1,
					       gpointer              user_data);

static DBusGProxy * create_extractor_proxy    (void);

static gboolean miner_files_check_file        (TrackerMinerFS       *fs,
					       GFile                *file);
static gboolean miner_files_check_directory   (TrackerMinerFS       *fs,
					       GFile                *file);
static gboolean miner_files_process_file      (TrackerMinerFS       *fs,
					       GFile                *file,
					       TrackerSparqlBuilder *sparql,
					       GCancellable         *cancellable,
					       TrackerMinerFSDoneCb  done_cb,
					       gpointer              done_cb_data);
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

	priv->extractor_proxy = create_extractor_proxy ();
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

	G_OBJECT_CLASS (tracker_miner_files_parent_class)->constructed (object);

        mf = TRACKER_MINER_FILES (object);
        fs = TRACKER_MINER_FS (object);

        if (!mf->private->config) {
                g_critical ("No config. This is mandatory");
                g_assert_not_reached ();
        }

        /* Fill in directories to inspect */
        dirs = tracker_config_get_index_single_directories (mf->private->config);

        while (dirs) {
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

		file = g_file_new_for_path (dirs->data);
                tracker_miner_fs_add_directory (fs, file, FALSE);
		g_object_unref (file);

                dirs = dirs->next;
        }

        dirs = tracker_config_get_index_recursive_directories (mf->private->config);

        while (dirs) {
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

		file = g_file_new_for_path (dirs->data);
                tracker_miner_fs_add_directory (fs, file, TRUE);
		g_object_unref (file);

                dirs = dirs->next;
        }

#ifdef HAVE_HAL
        initialize_removable_devices (mf);
#endif /* HAVE_HAL */

	g_signal_connect (mf->private->config, "notify::low-disk-space-limit",
			  G_CALLBACK (low_disk_space_limit_cb),
			  mf);

	disk_space_check_start (mf);
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

        priv = TRACKER_MINER_FILES_GET_PRIVATE (user_data);

        index_removable_devices = tracker_config_get_index_removable_devices (priv->config);

        if (index_removable_devices) {
		GFile *file;

		file = g_file_new_for_path (mount_point);
                tracker_miner_fs_add_directory (TRACKER_MINER_FS (user_data),
						file,
						TRUE);
		g_object_unref (file);
        }
}

static void
initialize_removable_devices (TrackerMinerFiles *mf)
{
        TrackerMinerFilesPrivate *priv;

        priv = TRACKER_MINER_FILES_GET_PRIVATE (mf);

        if (tracker_config_get_index_removable_devices (priv->config)) {
                GList *mounts, *m;

                mounts = tracker_storage_get_removable_device_roots (priv->storage);

                for (m = mounts; m; m = m->next) {
			GFile *as_file = g_file_new_for_path (m->data);
			tracker_miner_fs_add_directory (TRACKER_MINER_FS (mf),
							as_file, 
							TRUE);
			g_object_unref (as_file);
                }
        }
}

static void
on_battery_cb (GObject    *gobject,
	       GParamSpec *arg1,
	       gpointer    user_data)
{
	TrackerMinerFiles *mf = user_data;
	gboolean on_battery;

	/* FIXME: Get this working again */
	/* set_up_throttle (TRUE); */

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
						     g_get_application_name (),
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

	/* FIXME: Get this working again */
	/* set_up_throttle (FALSE); */
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

static DBusGProxy *
create_extractor_proxy (void)
{
	DBusGProxy *proxy;
	DBusGConnection *connection;
	GError *error = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the DBus session bus, %s",
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
						     g_get_application_name (),
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
	gboolean should_process;

	file_info = NULL;
	should_process = FALSE;
	basename = NULL;

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

	basename = g_file_get_basename (file);
	
	for (l = tracker_config_get_ignored_file_patterns (mf->private->config); l; l = l->next) {
		if (g_pattern_match_string (l->data, basename)) {
			goto done;
		}
	}
	
	should_process = TRUE;

done:
	if (file_info) {
		g_object_unref (file_info);
	}

	g_free (basename);

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
	gboolean should_process;

	should_process = FALSE;
	basename = NULL;

	/* Most common things to ignore */
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, NULL);

	if (file_info && g_file_info_get_is_hidden (file_info)) {
		TrackerMinerFiles *mf;
		GSList *allowed_directories;
		gchar *path;

		mf = TRACKER_MINER_FILES (fs);
		path = g_file_get_path (file);

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

		g_free (path);

		/* Ignore hidden dirs */
		goto done;
	}

	mf = TRACKER_MINER_FILES (fs);

	basename = g_file_get_basename (file);

	for (l = tracker_config_get_ignored_file_patterns (mf->private->config); l; l = l->next) {
		if (g_pattern_match_string (l->data, basename)) {
			goto done;
		}
	}

	/* Check module directory ignore patterns */
	should_process = TRUE;

done:
	if (file_info) {
		g_object_unref (file_info);
	}

	g_free (basename);

	return should_process;
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

static void
get_embedded_metadata_cb (DBusGProxy *proxy,
			  gchar      *sparql,
			  GError     *error,
			  gpointer    user_data)
{
	ProcessFileData *data = user_data;

	if (error) {
		/* Something bad happened, notify about the error */
		data->callback (TRACKER_MINER_FS (data->miner), data->file, data->sparql, error, data->callback_data);
		process_file_data_free (data);
		g_error_free (error);
		return;
	}

	if (sparql) {
		tracker_sparql_builder_insert_close (data->sparql);
		tracker_sparql_builder_append (data->sparql, sparql);
		tracker_sparql_builder_insert_open (data->sparql);
		g_free (sparql);
	}


	/* Notify about the success */
	data->callback (TRACKER_MINER_FS (data->miner), data->file, data->sparql, NULL, data->callback_data);
	process_file_data_free (data);
}

static void
get_embedded_metadata_cancel (GCancellable    *cancellable,
			      ProcessFileData *data)
{
	GError *error;

	/* Cancel extractor call */
	dbus_g_proxy_cancel_call (data->miner->private->extractor_proxy,
				  data->call);

	error = g_error_new_literal (miner_files_error_quark, 0, "Embedded metadata extraction was cancelled");
	data->callback (TRACKER_MINER_FS (data->miner), data->file, data->sparql, error, data->callback_data);

	process_file_data_free (data);
	g_error_free (error);
}

static void
get_embedded_metadata (ProcessFileData *data,
		       const gchar     *uri,
		       const gchar     *mime_type)
{
	data->call = org_freedesktop_Tracker1_Extract_get_metadata_async (data->miner->private->extractor_proxy,
									  uri,
									  mime_type,
									  get_embedded_metadata_cb,
									  data);
	g_signal_connect (data->cancellable, "cancelled",
			  G_CALLBACK (get_embedded_metadata_cancel), data);
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
		data->callback (TRACKER_MINER_FS (data->miner), file, sparql, error, data->callback_data);
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

	/* Next step, getting embedded metadata */
	get_embedded_metadata (data, uri, mime_type);

	g_free (uri);
}

static gboolean
miner_files_process_file (TrackerMinerFS       *fs,
			  GFile                *file,
			  TrackerSparqlBuilder *sparql,
			  GCancellable         *cancellable,
			  TrackerMinerFSDoneCb  done_cb,
			  gpointer              done_cb_data)
{
	ProcessFileData *data;
	const gchar *attrs;

	data = g_slice_new0 (ProcessFileData);
	data->callback = done_cb;
	data->callback_data = done_cb_data;
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
