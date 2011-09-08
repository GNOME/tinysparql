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

#include <libtracker-common/tracker-file-utils.h>
#include "tracker-indexing-tree.h"

G_DEFINE_TYPE (TrackerIndexingTree, tracker_indexing_tree, G_TYPE_OBJECT)

typedef struct _TrackerIndexingTreePrivate TrackerIndexingTreePrivate;
typedef struct _NodeData NodeData;
typedef struct _PatternData PatternData;
typedef struct _FindNodeData FindNodeData;

struct _NodeData
{
	GFile *file;
	guint flags : 7;
	guint shallow : 1;
};

struct _PatternData
{
	GPatternSpec *pattern;
	TrackerFilterType type;
};

struct _FindNodeData
{
	GEqualFunc func;
	GNode *node;
	GFile *file;
};

struct _TrackerIndexingTreePrivate
{
	GNode *config_tree;
	GList *filter_patterns;

	guint filter_hidden : 1;
};

enum {
	PROP_0,
	PROP_FILTER_HIDDEN
};

enum {
	DIRECTORY_ADDED,
	DIRECTORY_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static NodeData *
node_data_new (GFile *file,
               guint  flags)
{
	NodeData *data;

	data = g_slice_new (NodeData);
	data->file = g_object_ref (file);
	data->flags = flags;
	data->shallow = FALSE;

	return data;
}

static void
node_data_free (NodeData *data)
{
	g_object_unref (data->file);
	g_slice_free (NodeData, data);
}

static void
node_free (GNode *node)
{
	node_data_free (node->data);
}

static PatternData *
pattern_data_new (const gchar *glob_string,
                  guint        type)
{
	PatternData *data;

	data = g_slice_new (PatternData);
	data->pattern = g_pattern_spec_new (glob_string);
	data->type = type;

	return data;
}

static void
pattern_data_free (PatternData *data)
{
	g_pattern_spec_free (data->pattern);
	g_slice_free (PatternData, data);
}

static void
tracker_indexing_tree_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	TrackerIndexingTreePrivate *priv;

	priv = TRACKER_INDEXING_TREE (object)->priv;

	switch (prop_id) {
	case PROP_FILTER_HIDDEN:
		g_value_set_boolean (value, priv->filter_hidden);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_indexing_tree_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
	TrackerIndexingTree *tree;

	tree = TRACKER_INDEXING_TREE (object);

	switch (prop_id) {
	case PROP_FILTER_HIDDEN:
		tracker_indexing_tree_set_filter_hidden (tree,
							 g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_indexing_tree_finalize (GObject *object)
{
	TrackerIndexingTreePrivate *priv;
	TrackerIndexingTree *tree;

	tree = TRACKER_INDEXING_TREE (object);
	priv = tree->priv;

	g_list_foreach (priv->filter_patterns, (GFunc) pattern_data_free, NULL);
	g_list_free (priv->filter_patterns);

	g_node_traverse (priv->config_tree,
	                 G_POST_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 (GNodeTraverseFunc) node_free,
	                 NULL);
	g_node_destroy (priv->config_tree);

	G_OBJECT_CLASS (tracker_indexing_tree_parent_class)->finalize (object);
}

static void
tracker_indexing_tree_class_init (TrackerIndexingTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_indexing_tree_finalize;
	object_class->set_property = tracker_indexing_tree_set_property;
	object_class->get_property = tracker_indexing_tree_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_FILTER_HIDDEN,
	                                 g_param_spec_boolean ("filter-hidden",
							       "Filter hidden",
							       "Whether hidden resources are filtered",
							       FALSE,
							       G_PARAM_READWRITE));
	signals[DIRECTORY_ADDED] =
		g_signal_new ("directory-added",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerIndexingTreeClass,
					       directory_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1, G_TYPE_FILE);
	signals[DIRECTORY_REMOVED] =
		g_signal_new ("directory-removed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerIndexingTreeClass,
					       directory_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1, G_TYPE_FILE);

	g_type_class_add_private (object_class,
	                          sizeof (TrackerIndexingTreePrivate));
}

static void
tracker_indexing_tree_init (TrackerIndexingTree *tree)
{
	TrackerIndexingTreePrivate *priv;
	NodeData *data;
	GFile *root;

	priv = tree->priv = G_TYPE_INSTANCE_GET_PRIVATE (tree,
	                                                 TRACKER_TYPE_INDEXING_TREE,
	                                                 TrackerIndexingTreePrivate);
	/* Add a shallow root node */
	root = g_file_new_for_uri ("file:///");
	data = node_data_new (root, 0);
	data->shallow = TRUE;

	priv->config_tree = g_node_new (data);
	g_object_unref (root);
}

/**
 * tracker_indexing_tree_new:
 *
 * Returns a newly created #TrackerIndexingTree
 *
 * Returns: a newly allocated #TrackerIndexingTree
 **/
TrackerIndexingTree *
tracker_indexing_tree_new (void)
{
	return g_object_new (TRACKER_TYPE_INDEXING_TREE, NULL);
}

#ifdef PRINT_INDEXING_TREE
static gboolean
print_node_foreach (GNode    *node,
                    gpointer  user_data)
{
	NodeData *node_data = node->data;
	gchar *uri;

	uri = g_file_get_uri (node_data->file);
	g_debug ("%*s %s", g_node_depth (node), "-", uri);
	g_free (uri);

	return FALSE;
}

static void
print_tree (GNode *node)
{
	g_debug ("Printing modified tree...");
	g_node_traverse (node,
	                 G_PRE_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 print_node_foreach,
	                 NULL);
}

#endif /* PRINT_INDEXING_TREE */

static gboolean
find_node_foreach (GNode    *node,
                   gpointer  user_data)
{
	FindNodeData *data = user_data;
	NodeData *node_data = node->data;

	if ((data->func) (data->file, node_data->file)) {
		data->node = node;
		return TRUE;
	}

	return FALSE;
}

static GNode *
find_directory_node (GNode      *node,
                     GFile      *file,
                     GEqualFunc  func)
{
	FindNodeData data;

	data.file = file;
	data.node = NULL;
	data.func = func;

	g_node_traverse (node,
	                 G_POST_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 find_node_foreach,
	                 &data);

	return data.node;
}

static void
check_reparent_node (GNode    *node,
                     gpointer  user_data)
{
	GNode *new_node = user_data;
	NodeData *new_node_data, *node_data;

	new_node_data = new_node->data;
	node_data = node->data;

	if (g_file_has_prefix (node_data->file,
	                       new_node_data->file)) {
		g_node_unlink (node);
		g_node_append (new_node, node);
	}
}

/**
 * tracker_indexing_tree_add:
 * @tree: a #TrackerIndexingTree
 * @directory: #GFile pointing to a directory
 * @flags: Configuration flags for the directory
 *
 * Adds a directory to the indexing tree with the
 * given configuration flags.
 **/
void
tracker_indexing_tree_add (TrackerIndexingTree   *tree,
                           GFile                 *directory,
                           TrackerDirectoryFlags  flags)
{
	TrackerIndexingTreePrivate *priv;
	GNode *parent, *node;
	NodeData *data;

	g_return_if_fail (TRACKER_IS_INDEXING_TREE (tree));
	g_return_if_fail (G_IS_FILE (directory));

	priv = tree->priv;
	node = find_directory_node (priv->config_tree, directory,
	                            (GEqualFunc) g_file_equal);

	if (node) {
		/* Node already existed */
		data = node->data;
		data->shallow = FALSE;

		/* Overwrite flags if they are different */
		if (data->flags != flags) {
			gchar *uri;

			uri = g_file_get_uri (directory);
			g_warning ("Overwriting flags for directory '%s'", uri);
			g_free (uri);

			data->flags = flags;
		}
		return;
	}

	/* Find out the parent */
	parent = find_directory_node (priv->config_tree, directory,
	                              (GEqualFunc) g_file_has_prefix);

	/* Create node, move children of parent that
	 * could be children of this new node now.
	 */
	data = node_data_new (directory, flags);
	node = g_node_new (data);

	g_node_children_foreach (parent, G_TRAVERSE_ALL,
	                         check_reparent_node, node);

	/* Add the new node underneath the parent */
	g_node_append (parent, node);

	g_signal_emit (tree, signals[DIRECTORY_ADDED], 0, directory);

#ifdef PRINT_INDEXING_TREE
	/* Print tree */
	print_tree (priv->config_tree);
#endif /* PRINT_INDEXING_TREE */
}

/**
 * tracker_indexing_tree_remove:
 * @tree: a #TrackerIndexingTree
 * @directory: #GFile pointing to a directory
 *
 * Removes @directory from the indexing tree, note that
 * only directories previously added with tracker_indexing_tree_add()
 * can be effectively removed.
 **/
void
tracker_indexing_tree_remove (TrackerIndexingTree *tree,
                              GFile               *directory)
{
	TrackerIndexingTreePrivate *priv;
	GNode *node, *parent;
	NodeData *data;

	g_return_if_fail (TRACKER_IS_INDEXING_TREE (tree));
	g_return_if_fail (G_IS_FILE (directory));

	priv = tree->priv;
	node = find_directory_node (priv->config_tree, directory,
	                            (GEqualFunc) g_file_equal);
	if (!node) {
		return;
	}

	if (!node->parent) {
		/* Node is the config tree
		 * root, mark as shallow again
		 */
		data = node->data;
		data->shallow = TRUE;
		return;
	}

	parent = node->parent;
	g_node_unlink (node);

	/* Move children to parent */
	g_node_children_foreach (node, G_TRAVERSE_ALL,
	                         check_reparent_node, parent);

	g_signal_emit (tree, signals[DIRECTORY_REMOVED], 0, directory);

	node_data_free (node->data);
	g_node_destroy (node);
}

/**
 * tracker_indexing_tree_add_filter:
 * @tree: a #TrackerIndexingTree
 * @filter: filter type
 * @glob_string: glob-style string for the filter
 *
 * Adds a new filter for basenames.
 **/
void
tracker_indexing_tree_add_filter (TrackerIndexingTree *tree,
                                  TrackerFilterType    filter,
                                  const gchar         *glob_string)
{
	TrackerIndexingTreePrivate *priv;
	PatternData *data;

	g_return_if_fail (TRACKER_IS_INDEXING_TREE (tree));
	g_return_if_fail (glob_string != NULL);

	priv = tree->priv;

	data = pattern_data_new (glob_string, filter);
	priv->filter_patterns = g_list_prepend (priv->filter_patterns, data);
}

/**
 * tracker_indexing_tree_clear_filters:
 * @tree: a #TrackerIndexingTree
 * @type: filter type to clear
 *
 * Clears all filters of a given type.
 **/
void
tracker_indexing_tree_clear_filters (TrackerIndexingTree *tree,
                                     TrackerFilterType    type)
{
	TrackerIndexingTreePrivate *priv;
	GList *filters;

	g_return_if_fail (TRACKER_IS_INDEXING_TREE (tree));

	priv = tree->priv;
	filters = priv->filter_patterns;

	while (filters) {
		PatternData *data = filters->data;
		GList *cur = filters;

		filters = filters->next;

		if (data->type == type) {
			pattern_data_free (data);
			priv->filter_patterns = g_list_remove (priv->filter_patterns,
			                                       cur);
		}
	}
}

/**
 * tracker_indexing_tree_file_matches_filter:
 * @tree: a #TrackerIndexingTree
 * @type: filter type
 * @file: a #GFile
 *
 * Returns %TRUE if @file matches any filter of the given filter type.
 *
 * Returns: %TRUE if @file is filtered
 **/
gboolean
tracker_indexing_tree_file_matches_filter (TrackerIndexingTree *tree,
                                           TrackerFilterType    type,
                                           GFile               *file)
{
	TrackerIndexingTreePrivate *priv;
	GList *filters;
	gchar *basename;

	g_return_val_if_fail (TRACKER_IS_INDEXING_TREE (tree), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	priv = tree->priv;
	filters = priv->filter_patterns;
	basename = g_file_get_basename (file);

	while (filters) {
		PatternData *data = filters->data;

		filters = filters->next;

		if (data->type == type &&
		    g_pattern_match_string (data->pattern, basename)) {
			g_free (basename);
			return TRUE;
		}
	}

	g_free (basename);
	return FALSE;
}

static gboolean
parent_or_equals (GFile *file1,
                  GFile *file2)
{
	return (file1 == file2 ||
	        g_file_equal (file1, file2) ||
	        g_file_has_prefix (file1, file2));
}

/**
 * tracker_indexing_tree_file_is_indexable:
 * @tree: a #TrackerIndexingTree
 * @file: a #GFile
 * @file_type: a #GFileType
 *
 * returns %TRUE if @file should be indexed according to the
 * parameters given through tracker_indexing_tree_add() and
 * tracker_indexing_tree_add_filter().
 *
 * If @file_type is #G_FILE_TYPE_UNKNOWN, file type will be queried to the
 * file system.
 *
 * Returns: %TRUE if @file should be indexed.
 **/
gboolean
tracker_indexing_tree_file_is_indexable (TrackerIndexingTree *tree,
                                         GFile               *file,
                                         GFileType            file_type)
{
	TrackerIndexingTreePrivate *priv;
	TrackerFilterType filter;
	NodeData *data;
	GNode *parent;

	g_return_val_if_fail (TRACKER_IS_INDEXING_TREE (tree), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	priv = tree->priv;

	if (file_type == G_FILE_TYPE_UNKNOWN)
		file_type = g_file_query_file_type (file,
		                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
		                                    NULL);

	filter = (file_type == G_FILE_TYPE_DIRECTORY) ?
		TRACKER_FILTER_DIRECTORY : TRACKER_FILTER_FILE;

	if (tracker_indexing_tree_file_matches_filter (tree, filter, file)) {
		return FALSE;
	}

	parent = find_directory_node (priv->config_tree, file,
	                              (GEqualFunc) parent_or_equals);
	if (!parent) {
		return FALSE;
	}

	data = parent->data;

	if (!data->shallow                               &&
	    data->flags & TRACKER_DIRECTORY_FLAG_MONITOR &&
	    (g_file_equal (file, data->file)             ||   /* Exact path being monitored */
	     g_file_has_parent (file, data->file)        ||   /* Direct parent being monitored */
	     data->flags & TRACKER_DIRECTORY_FLAG_RECURSE)) { /* Parent not direct, but recursively monitored */
		return TRUE;
	}

	return FALSE;
}

/**
 * tracker_indexing_tree_parent_is_indexable:
 * @tree: a #TrackerIndexingTree
 * @parent: parent directory
 * @children: children within @parent
 * @n_children: number of children
 *
 * returns %TRUE if @parent should be indexed based on its contents.
 *
 * Returns: %TRUE if @parent should be indexed.
 **/
gboolean
tracker_indexing_tree_parent_is_indexable (TrackerIndexingTree *tree,
                                           GFile               *parent,
                                           GList               *children)
{
	if (!tracker_indexing_tree_file_is_indexable (tree,
	                                              parent,
	                                              G_FILE_TYPE_DIRECTORY)) {
		return FALSE;
	}

	while (children) {
		if (tracker_indexing_tree_file_matches_filter (tree,
		                                               TRACKER_FILTER_PARENT_DIRECTORY,
		                                               children->data)) {
			return FALSE;
		}

		children = children->next;
	}

	return TRUE;
}

gboolean
tracker_indexing_tree_get_filter_hidden (TrackerIndexingTree *tree)
{
	TrackerIndexingTreePrivate *priv;

	g_return_val_if_fail (TRACKER_IS_INDEXING_TREE (tree), FALSE);

	priv = tree->priv;
	return priv->filter_hidden;
}

void
tracker_indexing_tree_set_filter_hidden (TrackerIndexingTree *tree,
					 gboolean             filter_hidden)
{
	TrackerIndexingTreePrivate *priv;

	g_return_if_fail (TRACKER_IS_INDEXING_TREE (tree));

	priv = tree->priv;
	priv->filter_hidden = filter_hidden;

	g_object_notify (G_OBJECT (tree), "filter-hidden");
}

/**
 * tracker_indexing_tree_get_root:
 * @tree: a #TrackerIndexingtree
 * @file: a #GFile
 * @directory_flags: (out): return location for the applying #TrackerDirectoryFlags
 *
 * Returns the #GFile that was previously added through tracker_indexing_tree_add()
 * and would equal or contain @file, or %NULL if none applies.
 *
 * If the return value is non-%NULL, @directory_flags would contain the
 * #TrackerDirectoryFlags applying to @file.
 *
 * Returns: (transfer none): the effective parent in @tree, or %NULL
 **/
GFile *
tracker_indexing_tree_get_root (TrackerIndexingTree   *tree,
				GFile                 *file,
				TrackerDirectoryFlags *directory_flags)
{
	TrackerIndexingTreePrivate *priv;
	NodeData *data;
	GNode *parent;

	g_return_val_if_fail (TRACKER_IS_INDEXING_TREE (tree), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	priv = tree->priv;
	parent = find_directory_node (priv->config_tree, file,
	                              (GEqualFunc) parent_or_equals);
	if (!parent) {
		return NULL;
	}

	data = parent->data;

	if (!data->shallow &&
	    (file == data->file ||
	     g_file_equal (file, data->file) ||
	     g_file_has_prefix (file, data->file))) {
		if (directory_flags) {
			*directory_flags = data->flags;
		}

		return data->file;
	}

	return NULL;
}
