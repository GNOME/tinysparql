/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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
 * Author: Carlos Garnacho  <carlos@lanedo.com>
 */

#ifndef __TRACKER_INDEXING_TREE_H__
#define __TRACKER_INDEXING_TREE_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <gio/gio.h>
#include "tracker-miner-enums.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_INDEXING_TREE         (tracker_indexing_tree_get_type())
#define TRACKER_INDEXING_TREE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_INDEXING_TREE, TrackerIndexingTree))
#define TRACKER_INDEXING_TREE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_INDEXING_TREE, TrackerIndexingTreeClass))
#define TRACKER_IS_INDEXING_TREE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_INDEXING_TREE))
#define TRACKER_IS_INDEXING_TREE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_INDEXING_TREE))
#define TRACKER_INDEXING_TREE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_INDEXING_TREE, TrackerIndexingTreeClass))

typedef struct _TrackerIndexingTree TrackerIndexingTree;
typedef struct _TrackerIndexingTreeClass TrackerIndexingTreeClass;

struct _TrackerIndexingTree
{
	GObject parent_instance;
	gpointer priv;
};

struct _TrackerIndexingTreeClass
{
	GObjectClass parent_class;

	void (* directory_added)   (TrackerIndexingTree *indexing_tree,
				    GFile               *directory);
	void (* directory_removed) (TrackerIndexingTree *indexing_tree,
				    GFile               *directory);
};

GType                 tracker_indexing_tree_get_type (void) G_GNUC_CONST;

TrackerIndexingTree * tracker_indexing_tree_new      (void);

void      tracker_indexing_tree_add                  (TrackerIndexingTree   *tree,
                                                      GFile                 *directory,
                                                      TrackerDirectoryFlags  flags);
void      tracker_indexing_tree_remove               (TrackerIndexingTree   *tree,
                                                      GFile                 *directory);

void      tracker_indexing_tree_add_filter           (TrackerIndexingTree  *tree,
                                                      TrackerFilterType     type,
                                                      const gchar          *glob_string);
void      tracker_indexing_tree_clear_filters        (TrackerIndexingTree  *tree,
                                                      TrackerFilterType     type);
gboolean  tracker_indexing_tree_file_matches_filter  (TrackerIndexingTree  *tree,
                                                      TrackerFilterType     type,
                                                      GFile                *file);

gboolean  tracker_indexing_tree_file_is_indexable    (TrackerIndexingTree  *tree,
                                                      GFile                *file,
                                                      GFileType             file_type);
gboolean  tracker_indexing_tree_parent_is_indexable  (TrackerIndexingTree  *tree,
                                                      GFile                *parent,
                                                      GList                *children);

gboolean  tracker_indexing_tree_get_filter_hidden    (TrackerIndexingTree  *tree);
void      tracker_indexing_tree_set_filter_hidden    (TrackerIndexingTree  *tree,
						      gboolean              filter_hidden);

GFile *   tracker_indexing_tree_get_effective_parent (TrackerIndexingTree   *tree,
                                                      GFile                 *file,
                                                      TrackerDirectoryFlags *directory_flags);

G_END_DECLS

#endif /* __TRACKER_INDEXING_TREE_H__ */
