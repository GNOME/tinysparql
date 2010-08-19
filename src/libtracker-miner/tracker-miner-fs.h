/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_MINER_MINER_FS_H__
#define __LIBTRACKER_MINER_MINER_FS_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-miner-object.h"

G_BEGIN_DECLS

#define TRACKER_MINER_FS_GRAPH_URN "urn:uuid:472ed0cc-40ff-4e37-9c0c-062d78656540"

#define TRACKER_TYPE_MINER_FS         (tracker_miner_fs_get_type())
#define TRACKER_MINER_FS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_FS, TrackerMinerFS))
#define TRACKER_MINER_FS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_FS, TrackerMinerFSClass))
#define TRACKER_IS_MINER_FS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_FS))
#define TRACKER_IS_MINER_FS_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_MINER_FS))
#define TRACKER_MINER_FS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_MINER_FS, TrackerMinerFSClass))

typedef struct TrackerMinerFS        TrackerMinerFS;
typedef struct TrackerMinerFSPrivate TrackerMinerFSPrivate;

/**
 * TrackerMinerFS:
 *
 * Abstract miner abstract implementation to get data
 * from the filesystem.
 **/
struct TrackerMinerFS {
	TrackerMiner parent;
	TrackerMinerFSPrivate *private;
};

/**
 * TrackerMinerFSClass:
 * @parent: parent object class
 * @check_file: Called when a file should be checked for further processing
 * @check_directory: Called when a directory should be checked for further processing
 * @check_directory_contents: Called when a directory should be checked for further processing, based on the directory contents.
 * @process_file: Called when the metadata associated to a file is requested.
 * @ignore_next_update_file: Called after a writeback event happens on a file.
 * @monitor_directory: Called to check whether a directory should be modified.
 * @finished: Called when all processing has been performed.
 *
 * Prototype for the abstract class, @check_file, @check_directory, @check_directory_contents,
 * @process_file and @monitor_directory must be implemented in the deriving class in order to
 * actually extract data.
 **/
typedef struct {
	TrackerMinerClass parent;

	gboolean (* check_file)               (TrackerMinerFS       *fs,
	                                       GFile                *file);
	gboolean (* check_directory)          (TrackerMinerFS       *fs,
	                                       GFile                *file);
	gboolean (* check_directory_contents) (TrackerMinerFS       *fs,
	                                       GFile                *parent,
	                                       GList                *children);
	gboolean (* process_file)             (TrackerMinerFS       *fs,
	                                       GFile                *file,
	                                       TrackerSparqlBuilder *builder,
	                                       GCancellable         *cancellable);
	gboolean (* ignore_next_update_file)  (TrackerMinerFS       *fs,
	                                       GFile                *file,
	                                       TrackerSparqlBuilder *builder,
	                                       GCancellable         *cancellable);
	gboolean (* monitor_directory)        (TrackerMinerFS       *fs,
	                                       GFile                *file);
	void     (* finished)                 (TrackerMinerFS       *fs);
} TrackerMinerFSClass;

GType                 tracker_miner_fs_get_type             (void) G_GNUC_CONST;
void                  tracker_miner_fs_directory_add        (TrackerMinerFS *fs,
                                                             GFile          *file,
                                                             gboolean        recurse);
gboolean              tracker_miner_fs_directory_remove     (TrackerMinerFS *fs,
                                                             GFile          *file);
gboolean              tracker_miner_fs_directory_remove_full (TrackerMinerFS *fs,
                                                              GFile          *file);
void                  tracker_miner_fs_file_add             (TrackerMinerFS *fs,
                                                             GFile          *file,
                                                             gboolean        check_parents);
void                  tracker_miner_fs_file_notify          (TrackerMinerFS *fs,
                                                             GFile          *file,
                                                             const GError   *error);
void                  tracker_miner_fs_set_throttle         (TrackerMinerFS *fs,
                                                             gdouble         throttle);
gdouble               tracker_miner_fs_get_throttle         (TrackerMinerFS *fs);
G_CONST_RETURN gchar *tracker_miner_fs_get_urn              (TrackerMinerFS *fs,
                                                             GFile          *file);
G_CONST_RETURN gchar *tracker_miner_fs_get_parent_urn       (TrackerMinerFS *fs,
                                                             GFile          *file);
gchar                *tracker_miner_fs_query_urn            (TrackerMinerFS *fs,
                                                             GFile          *file);
void                  tracker_miner_fs_force_recheck        (TrackerMinerFS *fs);

void                  tracker_miner_fs_set_initial_crawling (TrackerMinerFS *fs,
                                                             gboolean        do_initial_crawling);
gboolean              tracker_miner_fs_get_initial_crawling (TrackerMinerFS *fs);

void                  tracker_miner_fs_add_directory_without_parent (TrackerMinerFS *fs,
                                                                     GFile          *file);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_MINER_FS_H__ */
