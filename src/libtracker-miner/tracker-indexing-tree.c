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
};

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
tracker_indexing_tree_finalize (GObject *object)
{
	TrackerIndexingTreePrivate *priv;
	TrackerIndexingTree *tree;

	tree = TRACKER_INDEXING_TREE (object);
	priv = tree->priv;

	g_list_foreach (priv->filter_patterns, (GFunc) pattern_data_free, NULL);
	g_list_free (priv->filter_patterns);

	g_node_traverse (priv->config_tree,
	                 G_IN_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 (GNodeTraverseFunc) node_data_free,
	                 NULL);
	g_node_destroy (priv->config_tree);

	G_OBJECT_CLASS (tracker_indexing_tree_parent_class)->finalize (object);
}

static void
tracker_indexing_tree_class_init (TrackerIndexingTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_indexing_tree_finalize;

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
	g_node_append_data (parent, directory);
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
 *
 * returns %TRUE if @file should be indexed according to the
 * parameters given through tracker_indexing_tree_add() and
 * tracker_indexing_tree_add_filter()
 *
 * Returns: %TRUE if @file should be indexed.
 **/
gboolean
tracker_indexing_tree_file_is_indexable (TrackerIndexingTree *tree,
                                         GFile               *file)
{
	TrackerIndexingTreePrivate *priv;
	TrackerFilterType filter;
	GFileType file_type;
	NodeData *data;
	GNode *parent;

	g_return_val_if_fail (TRACKER_IS_INDEXING_TREE (tree), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	priv = tree->priv;

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

	if (!data->shallow &&
	    (data->flags & TRACKER_DIRECTORY_FLAG_RECURSE ||
	     g_file_has_parent (file, data->file))) {
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
tracker_indexing_tree_parent_is_indexable (TrackerIndexingTree  *tree,
                                           GFile                *parent,
                                           GFile               **children,
                                           gint                  n_children)
{
	gint i = 0;

	if (!tracker_indexing_tree_file_is_indexable (tree, parent)) {
		return FALSE;
	}

	while (i < n_children &&
	       children[i] != NULL) {
		if (tracker_indexing_tree_file_matches_filter (tree,
		                                               TRACKER_FILTER_PARENT_DIRECTORY,
		                                               children[i])) {
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * tracker_indexing_tree_get_effective_parent:
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
tracker_indexing_tree_get_effective_parent (TrackerIndexingTree   *tree,
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
	    g_file_has_prefix (file, data->file)) {
		if (directory_flags) {
			*directory_flags = data->flags;
		}

		return data->file;
	}

	return NULL;
}
