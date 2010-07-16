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

#include <string.h>

#include <gio/gio.h>
#include <gio/gunixmounts.h>

#include <libtracker-common/tracker-log.h>

#include "tracker-storage.h"
#include "tracker-utils.h"
#include "tracker-marshal.h"

/**
 * SECTION:tracker-storage
 * @short_description: Removable storage and mount point convenience API
 * @include: libtracker-miner/tracker-miner.h
 *
 * This API is a convenience to to be able to keep track of volumes
 * which are mounted and also the type of removable media available.
 * The API is built upon the top of GIO's #GMount, #GDrive and #GVolume API.
 **/

#define TRACKER_STORAGE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_STORAGE, TrackerStoragePrivate))

typedef struct {
	GVolumeMonitor *volume_monitor;

	GNode *mounts;
	GHashTable *mounts_by_uuid;
} TrackerStoragePrivate;

typedef struct {
	gchar *mount_point;
	gchar *uuid;
	guint removable : 1;
	guint optical : 1;
} MountInfo;

typedef struct {
	const gchar *path;
	GNode *node;
} TraverseData;

typedef struct {
	GSList *roots;
	TrackerStorageType type;
	gboolean exact_match;
} GetRoots;

static void     tracker_storage_finalize (GObject        *object);
static gboolean mount_info_free          (GNode          *node,
                                          gpointer        user_data);
static void     mount_node_free          (GNode          *node);
static gboolean mounts_setup             (TrackerStorage *storage);
static void     mount_added_cb           (GVolumeMonitor *monitor,
                                          GMount         *mount,
                                          gpointer        user_data);
static void     mount_removed_cb         (GVolumeMonitor *monitor,
                                          GMount         *mount,
                                          gpointer        user_data);

enum {
	MOUNT_POINT_ADDED,
	MOUNT_POINT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (TrackerStorage, tracker_storage, G_TYPE_OBJECT);

static void
tracker_storage_class_init (TrackerStorageClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = tracker_storage_finalize;

	signals[MOUNT_POINT_ADDED] =
		g_signal_new ("mount-point-added",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              tracker_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN,
		              G_TYPE_NONE,
		              4,
		              G_TYPE_STRING,
		              G_TYPE_STRING,
		              G_TYPE_BOOLEAN,
		              G_TYPE_BOOLEAN);

	signals[MOUNT_POINT_REMOVED] =
		g_signal_new ("mount-point-removed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              tracker_marshal_VOID__STRING_STRING,
		              G_TYPE_NONE,
		              2,
		              G_TYPE_STRING,
		              G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (TrackerStoragePrivate));
}

static void
tracker_storage_init (TrackerStorage *storage)
{
	TrackerStoragePrivate *priv;

	g_message ("Initializing Storage...");

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	priv->mounts = g_node_new (NULL);

	priv->mounts_by_uuid = g_hash_table_new_full (g_str_hash,
	                                              g_str_equal,
	                                              (GDestroyNotify) g_free,
	                                              NULL);

	priv->volume_monitor = g_volume_monitor_get ();

	/* Volume and property notification callbacks */
	g_signal_connect_object (priv->volume_monitor, "mount-removed",
	                         G_CALLBACK (mount_removed_cb), storage, 0);
	g_signal_connect_object (priv->volume_monitor, "mount-pre-unmount",
	                         G_CALLBACK (mount_removed_cb), storage, 0);
	g_signal_connect_object (priv->volume_monitor, "mount-added",
	                         G_CALLBACK (mount_added_cb), storage, 0);

	g_message ("Mount monitors set up for to watch for added, removed and pre-unmounts...");

	/* Get all mounts and set them up */
	if (!mounts_setup (storage)) {
		return;
	}
}

static void
tracker_storage_finalize (GObject *object)
{
	TrackerStoragePrivate *priv;

	priv = TRACKER_STORAGE_GET_PRIVATE (object);

	if (priv->mounts_by_uuid) {
		g_hash_table_unref (priv->mounts_by_uuid);
	}

	if (priv->mounts) {
		mount_node_free (priv->mounts);
	}

	if (priv->volume_monitor) {
		g_object_unref (priv->volume_monitor);
	}

	(G_OBJECT_CLASS (tracker_storage_parent_class)->finalize) (object);
}

static void
mount_node_free (GNode *node)
{
	g_node_traverse (node,
	                 G_POST_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 mount_info_free,
	                 NULL);

	g_node_destroy (node);
}

static gboolean
mount_node_traverse_func (GNode    *node,
                          gpointer  user_data)
{
	TraverseData *data;
	MountInfo *info;

	if (!node->data) {
		/* Root node */
		return FALSE;
	}

	data = user_data;
	info = node->data;

	if (g_str_has_prefix (data->path, info->mount_point)) {
		data->node = node;
		return TRUE;
	}

	return FALSE;
}

static GNode *
mount_node_find (GNode       *root,
                 const gchar *path)
{
	TraverseData data = { path, NULL };

	g_node_traverse (root,
	                 G_POST_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 mount_node_traverse_func,
	                 &data);

	return data.node;
}

static gboolean
mount_info_free (GNode    *node,
                 gpointer  user_data)
{
	MountInfo *info;

	info = node->data;

	if (info) {
		g_free (info->mount_point);
		g_free (info->uuid);

		g_slice_free (MountInfo, info);
	}

	return FALSE;
}

static MountInfo *
mount_info_find (GNode       *root,
                 const gchar *path)
{
	GNode *node;

	node = mount_node_find (root, path);
	return (node) ? node->data : NULL;
}

static TrackerStorageType
mount_info_get_type (MountInfo *info)
{
	TrackerStorageType mount_type = 0;

	if (info->removable) {
		mount_type |= TRACKER_STORAGE_REMOVABLE;
	}

	if (info->optical) {
		mount_type |= TRACKER_STORAGE_OPTICAL;
	}

	return mount_type;
}

static gchar *
mount_point_normalize (const gchar *mount_point)
{
	gchar *mp;

	/* Normalize all mount points to have a / at the end */
	if (g_str_has_suffix (mount_point, G_DIR_SEPARATOR_S)) {
		mp = g_strdup (mount_point);
	} else {
		mp = g_strconcat (mount_point, G_DIR_SEPARATOR_S, NULL);
	}

	return mp;
}

static GNode *
mount_add_hierarchy (GNode       *root,
                     const gchar *uuid,
                     const gchar *mount_point,
                     gboolean     removable,
                     gboolean     optical)
{
	MountInfo *info;
	GNode *node;
	gchar *mp;

	mp = mount_point_normalize (mount_point);
	node = mount_node_find (root, mp);

	if (!node) {
		node = root;
	}

	info = g_slice_new (MountInfo);
	info->mount_point = mp;
	info->uuid = g_strdup (uuid);
	info->removable = removable;
	info->optical = optical;

	return g_node_append_data (node, info);
}

static void
mount_add_new (TrackerStorage *storage,
               const gchar    *uuid,
               const gchar    *mount_point,
               gboolean        removable_device,
               gboolean        optical_disc)
{
	TrackerStoragePrivate *priv;
	GNode *node;

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	node = mount_add_hierarchy (priv->mounts, uuid, mount_point, removable_device, optical_disc);
	g_hash_table_insert (priv->mounts_by_uuid, g_strdup (uuid), node);

	g_signal_emit (storage,
	               signals[MOUNT_POINT_ADDED],
	               0,
	               uuid,
	               mount_point,
	               removable_device,
	               optical_disc,
	               NULL);
}

static gchar *
mount_guess_content_type (GFile    *mount_root,
                          GVolume  *volume,
                          gboolean *is_optical,
                          gboolean *is_multimedia,
                          gboolean *is_blank)
{
	GUnixMountEntry *entry;
	gchar *content_type = NULL;
	gchar *mount_path;
	gchar **guess_type;

	/* This function has 2 purposes:
	 *
	 * 1. Detect if we are using optical media
	 * 2. Detect if we are video or music, we can't index those types
	 */

	if (g_file_has_uri_scheme (mount_root, "cdda")) {
		g_debug ("  Scheme is CDDA, assuming this is a CD");

		*is_optical = TRUE;
		*is_multimedia = TRUE;

		return g_strdup ("x-content/audio-cdda");
	}

	*is_optical = FALSE;
	*is_multimedia = FALSE;
	*is_blank = FALSE;

	mount_path = g_file_get_path (mount_root);

	/* FIXME: Try to assume we have a unix mount :(
	 * EEK, once in a while, I have to write crack, oh well
	 */
	if (mount_path &&
	    (entry = g_unix_mount_at (mount_path, NULL)) != NULL) {
		const gchar *filesystem_type;
		gchar *device_path = NULL;

		filesystem_type = g_unix_mount_get_fs_type (entry);
		g_debug ("  Using filesystem type:'%s'",
			 filesystem_type);

		/* Volume may be NULL */
		if (volume) {
			device_path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
			g_debug ("  Using device path:'%s'",
			         device_path);
		}

		/* NOTE: This code was taken from guess_mount_type()
		 * in GIO's gunixmounts.c and adapted purely for
		 * guessing optical media. We don't use the guessing
		 * code for other types such as MEMSTICKS, ZIPs,
		 * IPODs, etc.
		 *
		 * This code may need updating over time since it is
		 * very situational depending on how distributions
		 * mount their devices and how devices are named in
		 * /dev.
		 */
		if (strcmp (filesystem_type, "udf") == 0 ||
		    strcmp (filesystem_type, "iso9660") == 0 ||
		    strcmp (filesystem_type, "cd9660") == 0 ||
		    (device_path &&
		     (g_str_has_prefix (device_path, "/dev/cdrom") ||
		      g_str_has_prefix (device_path, "/dev/acd") ||
		      g_str_has_prefix (device_path, "/dev/cd")))) {
			*is_optical = TRUE;
		} else if (device_path &&
		           g_str_has_prefix (device_path, "/vol/")) {
			const gchar *name;

			name = mount_path + strlen ("/");

			if (g_str_has_prefix (name, "cdrom")) {
				*is_optical = TRUE;
			}
		} else {
			gchar *basename = g_path_get_basename (mount_path);

			if (g_str_has_prefix (basename, "cdr") ||
			    g_str_has_prefix (basename, "cdwriter") ||
			    g_str_has_prefix (basename, "burn") ||
			    g_str_has_prefix (basename, "dvdr")) {
				*is_optical = TRUE;
			}

			g_free (basename);
		}

		g_free (device_path);
		g_free (mount_path);
		g_unix_mount_free (entry);
	} else {
		g_debug ("  No GUnixMountEntry found, needed for detecting if optical media... :(");
		g_free (mount_path);
	}

	/* We try to determine the content type because we don't want
	 * to store Volume information in Tracker about DVDs and media
	 * which has no real data for us to mine.
	 *
	 * Generally, if is_multimedia is TRUE then we end up ignoring
	 * the media.
	 */
	guess_type = g_content_type_guess_for_tree (mount_root);
	if (guess_type) {
		gint i = 0;

		while (!content_type && guess_type[i]) {
			if (!g_strcmp0 (guess_type[i], "x-content/image-picturecd")) {
				/* Images */
				content_type = g_strdup (guess_type[i]);
			} else if (!g_strcmp0 (guess_type[i], "x-content/video-bluray") ||
			           !g_strcmp0 (guess_type[i], "x-content/video-dvd") ||
			           !g_strcmp0 (guess_type[i], "x-content/video-hddvd") ||
			           !g_strcmp0 (guess_type[i], "x-content/video-svcd") ||
			           !g_strcmp0 (guess_type[i], "x-content/video-vcd")) {
				/* Videos */
				*is_multimedia = TRUE;
				content_type = g_strdup (guess_type[i]);
			} else if (!g_strcmp0 (guess_type[i], "x-content/audio-cdda") ||
			           !g_strcmp0 (guess_type[i], "x-content/audio-dvd") ||
			           !g_strcmp0 (guess_type[i], "x-content/audio-player")) {
				/* Audios */
				*is_multimedia = TRUE;
				content_type = g_strdup (guess_type[i]);
			} else if (!g_strcmp0 (guess_type[i], "x-content/blank-bd") ||
			           !g_strcmp0 (guess_type[i], "x-content/blank-cd") ||
			           !g_strcmp0 (guess_type[i], "x-content/blank-dvd") ||
			           !g_strcmp0 (guess_type[i], "x-content/blank-hddvd")) {
				/* Blank */
				*is_blank = TRUE;
				content_type = g_strdup (guess_type[i]);
			} else if (!g_strcmp0 (guess_type[i], "x-content/software") ||
			           !g_strcmp0 (guess_type[i], "x-content/unix-software") ||
			           !g_strcmp0 (guess_type[i], "x-content/win32-software")) {
				/* NOTE: This one is a guess, can we
				 * have this content type on
				 * none-optical mount points?
				 */
				content_type = g_strdup (guess_type[i]);
			} else {
				/* else, keep on with the next guess, if any */
				i++;
			}
		}

		/* If we didn't have an exact match on possible guessed content types,
		 *  then use the first one returned (best guess always first) if any */
		if (!content_type && guess_type[0]) {
			content_type = g_strdup (guess_type[0]);
		}

		g_strfreev (guess_type);
	}

	/* If none of the previous methods worked, return NULL content type and
	 * set is_blank so that it's not indexed */
	if (!content_type) {
		*is_blank = TRUE;
	}

	return content_type;
}

static void
mount_add (TrackerStorage *storage,
           GMount         *mount)
{
	TrackerStoragePrivate *priv;
	GFile *root;
	GVolume *volume;
	gchar *mount_name, *mount_path, *uuid;
	gboolean is_optical = FALSE;
	gboolean is_removable = FALSE;

	/* Get mount name */
	mount_name = g_mount_get_name (mount);

	/* Get root path of the mount */
	root = g_mount_get_root (mount);
	mount_path = g_file_get_path (root);

	g_debug ("Found '%s' mounted on path '%s'",
	         mount_name,
		 mount_path);

	/* Do not process shadowed mounts! */
	if (g_mount_is_shadowed (mount)) {
		g_debug ("  Skipping shadowed mount '%s'", mount_name);
		g_object_unref (root);
		g_free (mount_path);
		g_free (mount_name);
		return;
	}

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	/* fstab partitions may not have corresponding
	 * GVolumes, so volume may be NULL */
	volume = g_mount_get_volume (mount);
	if (volume) {
		/* GMount with GVolume */

		/* Try to get UUID from the Volume.
		 * Note that g_volume_get_uuid() is NOT equivalent */
		uuid = g_volume_get_identifier (volume,
		                                G_VOLUME_IDENTIFIER_KIND_UUID);
		if (!uuid) {
			gchar *content_type;
			gboolean is_multimedia;
			gboolean is_blank;

			/* Optical discs usually won't have UUID in the GVolume */
			content_type = mount_guess_content_type (root, volume, &is_optical, &is_multimedia, &is_blank);
			is_removable = TRUE;

			/* We don't index content which is video, music or blank */
			if (!is_multimedia && !is_blank) {
				uuid = g_compute_checksum_for_string (G_CHECKSUM_MD5,
								      mount_name,
								      -1);
				g_debug ("  No UUID, generated:'%s' (based on mount name)", uuid);
				g_debug ("  Assuming GVolume has removable media, if wrong report a bug! "
				         "content type is '%s'",
				         content_type);
			} else {
				g_debug ("  Being ignored because mount with volume is music/video/blank "
				         "(content type:%s, optical:%s, multimedia:%s, blank:%s)",
				         content_type,
				         is_optical ? "yes" : "no",
				         is_multimedia ? "yes" : "no",
				         is_blank ? "yes" : "no");
			}

			g_free (content_type);
		} else {
			/* Any other removable media will have UUID in the GVolume.
			 * Note that this also may include some partitions in the machine
			 * which have GVolumes associated to the GMounts. So, we need to
			 * explicitly check if the drive is media-removable (machine
			 * partitions won't be media-removable) */
			GDrive *drive;

			drive = g_volume_get_drive (volume);
			if (drive) {
				is_removable = g_drive_is_media_removable (drive);
				g_object_unref (drive);
			} else {
				/* Note: not sure when this can happen... */
				g_debug ("  Assuming GDrive has removable media, if wrong report a bug!");
				is_removable = TRUE;
			}
		}

		g_object_unref (volume);
	} else {
		/* GMount without GVolume.
		 * Note: Never found a case where this g_mount_get_uuid() returns
		 * non-NULL... :-) */
		uuid = g_mount_get_uuid (mount);
		if (!uuid) {
			if (mount_path) {
				gchar *content_type;
				gboolean is_multimedia;
				gboolean is_blank;

				content_type = mount_guess_content_type (root, volume, &is_optical, &is_multimedia, &is_blank);

				/* Note: for GMounts without GVolume, is_blank should NOT be considered,
				 * as it may give unwanted results... */
				if (!is_multimedia) {
					uuid = g_compute_checksum_for_string (G_CHECKSUM_MD5,
									      mount_path,
									      -1);
					g_debug ("  No UUID, generated:'%s' (based on mount path)", uuid);
				} else {
					g_debug ("  Being ignored because mount is music/video "
					         "(content type:%s, optical:%s, multimedia:%s)",
					         content_type,
					         is_optical ? "yes" : "no",
					         is_multimedia ? "yes" : "no");
				}

				g_free (content_type);
			} else {
				g_debug ("  Being ignored because mount has no GVolume (i.e. not user mountable) "
				         "and has mount root path available");
			}
		}
	}

	/* If we got something to be used as UUID, then add the mount
	 * to the TrackerStorage */
	if (uuid && !g_hash_table_lookup (priv->mounts_by_uuid, uuid)) {
		g_debug ("  Adding mount point with UUID:'%s', removable: %s, optical: %s",
		         uuid,
		         is_removable ? "yes" : "no",
		         is_optical ? "yes" : "no");
		mount_add_new (storage, uuid, mount_path, is_removable, is_optical);
	}

	g_free (mount_name);
	g_free (mount_path);
	g_free (uuid);
	g_object_unref (root);
}

static gboolean
mounts_setup (TrackerStorage *storage)
{
	TrackerStoragePrivate *priv;
	GList *mounts, *lm;

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	mounts = g_volume_monitor_get_mounts (priv->volume_monitor);

	if (!mounts) {
		g_message ("No mounts found to iterate");
		return TRUE;
	}

	/* Iterate over all available mounts and add them.
	 * Note that GVolumeMonitor shows only those mounts which are
	 * actually mounted. */
	for (lm = mounts; lm; lm = g_list_next (lm)) {
		mount_add (storage, lm->data);
		g_object_unref (lm->data);
	}

	g_list_free (mounts);

	return TRUE;
}

static void
mount_added_cb (GVolumeMonitor *monitor,
                GMount         *mount,
                gpointer        user_data)
{
	mount_add (user_data, mount);
}

static void
mount_removed_cb (GVolumeMonitor *monitor,
                  GMount         *mount,
                  gpointer        user_data)
{
	TrackerStorage *storage;
	TrackerStoragePrivate *priv;
	MountInfo *info;
	GNode *node;
	GFile *file;
	gchar *name;
	gchar *mount_point;
	gchar *mp;

	storage = user_data;
	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	file = g_mount_get_root (mount);
	mount_point = g_file_get_path (file);
	name = g_mount_get_name (mount);

	mp = mount_point_normalize (mount_point);
	node = mount_node_find (priv->mounts, mp);
	g_free (mp);

	if (node) {
		info = node->data;

		g_message ("Mount:'%s' with UUID:'%s' now unmounted from:'%s'",
		           name,
		           info->uuid,
		           mount_point);

		g_signal_emit (storage, signals[MOUNT_POINT_REMOVED], 0, info->uuid, mount_point, NULL);

		g_hash_table_remove (priv->mounts_by_uuid, info->uuid);
		mount_node_free (node);
	} else {
		g_message ("Mount:'%s' now unmounted from:'%s' (was not tracked)",
		           name,
		           mount_point);
	}

	g_free (name);
	g_free (mount_point);
	g_object_unref (file);
}

/**
 * tracker_storage_new:
 *
 * Creates a new instance of #TrackerStorage.
 *
 * Returns: The newly created #TrackerStorage.
 **/
TrackerStorage *
tracker_storage_new (void)
{
	return g_object_new (TRACKER_TYPE_STORAGE, NULL);
}

static void
get_mount_point_by_uuid_foreach (gpointer key,
                                 gpointer value,
                                 gpointer user_data)
{
	GetRoots *gr;
	const gchar *uuid;
	GNode *node;
	MountInfo *info;
	TrackerStorageType mount_type;

	gr = user_data;
	uuid = key;
	node = value;
	info = node->data;
	mount_type = mount_info_get_type (info);

	/* is mount of the type we're looking for? */
	if ((gr->exact_match && mount_type == gr->type) ||
	    (!gr->exact_match && (mount_type & gr->type))) {
		gchar *normalized_mount_point;
		gint len;

		normalized_mount_point = g_strdup (info->mount_point);
		len = strlen (normalized_mount_point);

		/* Don't include trailing slashes */
		if (len > 2 && normalized_mount_point[len - 1] == G_DIR_SEPARATOR) {
			normalized_mount_point[len - 1] = '\0';
		}

		gr->roots = g_slist_prepend (gr->roots, normalized_mount_point);
	}
}

/**
 * tracker_storage_get_device_roots:
 * @storage: A #TrackerStorage
 * @type: A #TrackerStorageType
 * @exact_match: if all devices should exactly match the types
 *
 * Returns: a #GSList of strings containing the root directories for
 * devices with @type based on @exact_match. Each element must be
 * freed using g_free() and the list itself through g_slist_free().
 **/
GSList *
tracker_storage_get_device_roots (TrackerStorage     *storage,
                                  TrackerStorageType  type,
                                  gboolean            exact_match)
{
	TrackerStoragePrivate *priv;
	GetRoots gr;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), NULL);

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	gr.roots = NULL;
	gr.type = type;
	gr.exact_match = exact_match;

	g_hash_table_foreach (priv->mounts_by_uuid,
	                      get_mount_point_by_uuid_foreach,
	                      &gr);

	return gr.roots;
}

/**
 * tracker_storage_get_device_uuids:
 * @storage: A #TrackerStorage
 * @type: A #TrackerStorageType
 * @exact_match: if all devices should exactly match the types
 *
 * Returns: a #GSList of strings containing the UUID for devices with
 * @type based on @exact_match. Each element must be freed using
 * g_free() and the list itself through g_slist_free().
 **/
GSList *
tracker_storage_get_device_uuids (TrackerStorage     *storage,
                                  TrackerStorageType  type,
                                  gboolean            exact_match)
{
	TrackerStoragePrivate *priv;
	GHashTableIter iter;
	gpointer key, value;
	GSList *uuids;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), NULL);

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	uuids = NULL;

	g_hash_table_iter_init (&iter, priv->mounts_by_uuid);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *uuid;
		GNode *node;
		MountInfo *info;
		TrackerStorageType mount_type;

		uuid = key;
		node = value;
		info = node->data;

		mount_type = mount_info_get_type (info);

		/* is mount of the type we're looking for? */
		if ((exact_match && mount_type == type) ||
		    (!exact_match && (mount_type & type))) {
			uuids = g_slist_prepend (uuids, g_strdup (uuid));
		}
	}

	return uuids;
}

/**
 * tracker_storage_get_mount_point_for_uuid:
 * @storage: A #TrackerStorage
 * @uuid: A string pointer to the UUID for the %GVolume.
 *
 * Returns: The mount point for @uuid, this should not be freed.
 **/
const gchar *
tracker_storage_get_mount_point_for_uuid (TrackerStorage *storage,
                                          const gchar    *uuid)
{
	TrackerStoragePrivate *priv;
	GNode *node;
	MountInfo *info;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), NULL);
	g_return_val_if_fail (uuid != NULL, NULL);

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	node = g_hash_table_lookup (priv->mounts_by_uuid, uuid);

	if (!node) {
		return NULL;
	}

	info = node->data;

	return info->mount_point;
}

/**
 * tracker_storage_get_type_for_uuid:
 * @storage: A #TrackerStorage
 * @uuid: A string pointer to the UUID for the %GVolume.
 *
 * Returns: The type flags for @uuid.
 **/
TrackerStorageType
tracker_storage_get_type_for_uuid (TrackerStorage     *storage,
                                   const gchar        *uuid)
{
	TrackerStoragePrivate *priv;
	GNode *node;
	TrackerStorageType type = 0;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), 0);
	g_return_val_if_fail (uuid != NULL, 0);

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	node = g_hash_table_lookup (priv->mounts_by_uuid, uuid);

	if (node) {
		MountInfo *info;

		info = node->data;

		if (info->removable) {
			type |= TRACKER_STORAGE_REMOVABLE;
		}
		if (info->optical) {
			type |= TRACKER_STORAGE_OPTICAL;
		}
	}

	return type;
}

/**
 * tracker_storage_get_uuid_for_file:
 * @storage: A #TrackerStorage
 * @file: a file
 *
 * Returns the UUID of the removable device for @file
 *
 * Returns: Returns the UUID of the removable device for @file, this
 * should not be freed.
 **/
const gchar *
tracker_storage_get_uuid_for_file (TrackerStorage *storage,
                                   GFile          *file)
{
	TrackerStoragePrivate *priv;
	gchar *path;
	MountInfo *info;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), FALSE);

	path = g_file_get_path (file);

	if (!path) {
		return NULL;
	}

	/* Normalize all paths to have a / at the end */
	if (!g_str_has_suffix (path, G_DIR_SEPARATOR_S)) {
		gchar *norm_path;

		norm_path = g_strconcat (path, G_DIR_SEPARATOR_S, NULL);
		g_free (path);
		path = norm_path;
	}

	priv = TRACKER_STORAGE_GET_PRIVATE (storage);

	info = mount_info_find (priv->mounts, path);

	if (!info) {
		g_free (path);
		return NULL;
	}

	/* g_debug ("Mount for path '%s' is '%s' (UUID:'%s')", */
	/*          path, info->mount_point, info->uuid); */

	g_free (path);

	return info->uuid;
}

