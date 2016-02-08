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

#include <string.h>
#include <stdlib.h>

#include "tracker-file-system.h"

typedef struct _TrackerFileSystemPrivate TrackerFileSystemPrivate;
typedef struct _FileNodeProperty FileNodeProperty;
typedef struct _FileNodeData FileNodeData;
typedef struct _NodeLookupData NodeLookupData;

static GHashTable *properties = NULL;

struct _TrackerFileSystemPrivate {
	GNode *file_tree;
	GFile *root;
};

struct _FileNodeProperty {
	GQuark prop_quark;
	gpointer value;
};

struct _FileNodeData {
	GFile *file;
	gchar *uri_prefix;
	GArray *properties;
	guint shallow   : 1;
	guint unowned : 1;
	guint file_type : 4;
};

struct _NodeLookupData {
	TrackerFileSystem *file_system;
	GNode *node;
};

enum {
	PROP_0,
	PROP_ROOT,
};

static GQuark quark_file_node = 0;

static void file_weak_ref_notify (gpointer    user_data,
                                  GObject    *prev_location);

G_DEFINE_TYPE (TrackerFileSystem, tracker_file_system, G_TYPE_OBJECT)

/*
 * TrackerFileSystem is a filesystem abstraction, it mainly serves 2 purposes:
 *   - Canonicalizes GFiles, so it is possible later to perform pointer
 *     comparisons on them.
 *   - Stores data for the GFile lifetime, so it may be used as cache store
 *     as long as some file is needed.
 *
 * The TrackerFileSystem holds a reference on each GFile. There are two cases
 * when we want to force a cached GFile to be freed: when it no longer exists
 * on disk, and once crawling a directory has completed and we only need to
 * remember the directories. Objects may persist in the cache even after
 * tracker_file_system_forget_files() is called to delete them if there are
 * references held on them elsewhere, and they will stay until all references
 * are dropped.
 */


static void
file_node_data_free (FileNodeData *data,
                     GNode        *node)
{
	guint i;

	if (data->file) {
		if (!data->shallow) {
			g_object_weak_unref (G_OBJECT (data->file),
			                     file_weak_ref_notify,
			                     node);
		}

		if (!data->unowned) {
			g_object_unref (data->file);
		}
	}

	data->file = NULL;
	g_free (data->uri_prefix);

	for (i = 0; i < data->properties->len; i++) {
		FileNodeProperty *property;
		GDestroyNotify destroy_notify;

		property = &g_array_index (data->properties,
		                           FileNodeProperty, i);

		destroy_notify = g_hash_table_lookup (properties,
		                                      GUINT_TO_POINTER (property->prop_quark));

		if (destroy_notify) {
			(destroy_notify) (property->value);
		}
	}

	g_array_free (data->properties, TRUE);
	g_slice_free (FileNodeData, data);
}

static FileNodeData *
file_node_data_new (TrackerFileSystem *file_system,
                    GFile             *file,
                    GFileType          file_type,
                    GNode             *node)
{
	FileNodeData *data;
	NodeLookupData lookup_data;
	GArray *node_data;

	data = g_slice_new0 (FileNodeData);
	data->file = g_object_ref (file);
	data->file_type = file_type;
	data->properties = g_array_new (FALSE, TRUE, sizeof (FileNodeProperty));

	/* We use weak refs to keep track of files */
	g_object_weak_ref (G_OBJECT (data->file), file_weak_ref_notify, node);

	node_data = g_object_get_qdata (G_OBJECT (data->file),
	                                quark_file_node);

	if (!node_data) {
		node_data = g_array_new (FALSE, FALSE, sizeof (NodeLookupData));
		g_object_set_qdata_full (G_OBJECT (data->file),
		                         quark_file_node,
		                         node_data,
		                         (GDestroyNotify) g_array_unref);
	}

	lookup_data.file_system = file_system;
	lookup_data.node = node;
	g_array_append_val (node_data, lookup_data);

	g_assert (node->data == NULL);
	node->data = data;

	return data;
}

static FileNodeData *
file_node_data_root_new (GFile *root)
{
	FileNodeData *data;

	data = g_slice_new0 (FileNodeData);
	data->uri_prefix = g_file_get_uri (root);
	data->file = g_object_ref (root);
	data->properties = g_array_new (FALSE, TRUE, sizeof (FileNodeProperty));
	data->file_type = G_FILE_TYPE_DIRECTORY;
	data->shallow = TRUE;

	return data;
}

static gboolean
file_node_data_equal_or_child (GNode  *node,
                               gchar  *uri_prefix,
                               gchar **uri_remainder)
{
	FileNodeData *data;
	gsize len;

	data = node->data;
	len = strlen (data->uri_prefix);

	if (strncmp (uri_prefix, data->uri_prefix, len) == 0) {
		uri_prefix += len;

		if (uri_prefix[0] == '/') {
			uri_prefix++;
		} else if (uri_prefix[0] != '\0' &&
		           (len < 4 ||
		            strcmp (data->uri_prefix + len - 4, ":///") != 0)) {
			/* If the first char isn't an uri separator
			 * nor \0, node represents a similarly named
			 * file, but not a parent after all.
			 */
			return FALSE;
		}

		if (uri_remainder) {
			*uri_remainder = uri_prefix;
		}

		return TRUE;
	} else {
		return FALSE;
	}
}

static GNode *
file_tree_lookup (GNode     *tree,
                  GFile     *file,
                  GNode    **parent_node,
                  gchar    **uri_remainder)
{
	GNode *parent, *node_found, *parent_found;
	FileNodeData *data;
	gchar *uri, *ptr;

	uri = ptr = g_file_get_uri (file);
	node_found = parent_found = NULL;

	/* Run through the filesystem tree, comparing chunks of
	 * uri with the uri prefix in the file nodes, this would
	 * get us to the closest registered parent, or the file
	 * itself.
	 */

	if (parent_node) {
		*parent_node = NULL;
	}

	if (uri_remainder) {
		*uri_remainder = NULL;
	}

	if (!tree) {
		return NULL;
	}

	if (!G_NODE_IS_ROOT (tree)) {
		FileNodeData *parent_data;
		gchar *parent_uri;

		parent_data = tree->data;
		parent_uri = g_file_get_uri (parent_data->file);

		/* Sanity check */
		if (!g_str_has_prefix (uri, parent_uri)) {
			g_free (parent_uri);
			return NULL;
		}

		ptr += strlen (parent_uri);

		g_assert (ptr[0] == '/');
		ptr++;

		g_free (parent_uri);
	} else {
		/* First check the root node */
		if (!file_node_data_equal_or_child (tree, uri, &ptr)) {
			g_free (uri);
			return NULL;
		}

		/* Second check there is no basename and if there isn't,
		 * then this node MUST be the closest registered node
		 * we can use for the uri. The difference here is that
		 * we return tree not NULL.
		 */
		else if (ptr[0] == '\0') {
			g_free (uri);
			return tree;
                }
	}

	parent = tree;

	while (parent) {
		GNode *child, *next = NULL;
		gchar *ret_ptr;

		for (child = g_node_first_child (parent);
		     child != NULL;
		     child = g_node_next_sibling (child)) {
			data = child->data;

			if (data->uri_prefix[0] != ptr[0])
				continue;

			if (file_node_data_equal_or_child (child, ptr, &ret_ptr)) {
				ptr = ret_ptr;
				next = child;
				break;
			}
		}

		if (next) {
			if (ptr[0] == '\0') {
				/* Exact match */
				node_found = next;
				parent_found = parent;
				break;
			} else {
				/* Descent down the child */
				parent = next;
			}
		} else {
			parent_found = parent;
			break;
		}
	}

	if (parent_node) {
		*parent_node = parent_found;
	}

	if (ptr && *ptr && uri_remainder) {
		*uri_remainder = g_strdup (ptr);
	}

	g_free (uri);

	return node_found;
}

static gboolean
file_tree_free_node_foreach (GNode    *node,
                             gpointer  user_data)
{
	file_node_data_free (node->data, node);
	return FALSE;
}

/* TrackerFileSystem implementation */

static void
file_system_finalize (GObject *object)
{
	TrackerFileSystemPrivate *priv;

	priv = TRACKER_FILE_SYSTEM (object)->priv;

	g_node_traverse (priv->file_tree,
	                 G_POST_ORDER,
	                 G_TRAVERSE_ALL, -1,
	                 file_tree_free_node_foreach,
	                 NULL);
	g_node_destroy (priv->file_tree);

	if (priv->root) {
		g_object_unref (priv->root);
	}

	G_OBJECT_CLASS (tracker_file_system_parent_class)->finalize (object);
}

static void
file_system_constructed (GObject *object)
{
	TrackerFileSystemPrivate *priv;
	FileNodeData *root_data;

	G_OBJECT_CLASS (tracker_file_system_parent_class)->constructed (object);

	priv = TRACKER_FILE_SYSTEM (object)->priv;

	if (priv->root == NULL) {
		priv->root = g_file_new_for_uri ("file:///");
	}

	root_data = file_node_data_root_new (priv->root);
	priv->file_tree = g_node_new (root_data);
}

static void
file_system_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
	TrackerFileSystemPrivate *priv;

	priv = TRACKER_FILE_SYSTEM (object)->priv;

	switch (prop_id) {
	case PROP_ROOT:
		g_value_set_object (value, priv->root);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
file_system_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
	TrackerFileSystemPrivate *priv;

	priv = TRACKER_FILE_SYSTEM (object)->priv;

	switch (prop_id) {
	case PROP_ROOT:
		priv->root = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_file_system_class_init (TrackerFileSystemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = file_system_finalize;
	object_class->constructed = file_system_constructed;
	object_class->get_property = file_system_get_property;
	object_class->set_property = file_system_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_ROOT,
	                                 g_param_spec_object ("root",
	                                                      "Root URL",
	                                                      "The root GFile for the indexing tree",
	                                                      G_TYPE_FILE,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class,
	                          sizeof (TrackerFileSystemPrivate));

	quark_file_node =
		g_quark_from_static_string ("tracker-quark-file-node");
}
static void
tracker_file_system_init (TrackerFileSystem *file_system)
{
	file_system->priv =
		G_TYPE_INSTANCE_GET_PRIVATE (file_system,
		                             TRACKER_TYPE_FILE_SYSTEM,
		                             TrackerFileSystemPrivate);
}

TrackerFileSystem *
tracker_file_system_new (GFile *root)
{
	return g_object_new (TRACKER_TYPE_FILE_SYSTEM,
	                     "root", root,
	                     NULL);
}

static void
reparent_child_nodes_to_parent (GNode *node)
{
	FileNodeData *node_data;
	GNode *child, *parent;

	if (!node->parent) {
		return;
	}

	parent = node->parent;
	node_data = node->data;
	child = g_node_first_child (node);

	while (child) {
		FileNodeData *data;
		gchar *uri_prefix;
		GNode *cur;

		cur = child;
		data = cur->data;
		child = g_node_next_sibling (child);

		uri_prefix = g_strdup_printf ("%s/%s",
					      node_data->uri_prefix,
					      data->uri_prefix);

		g_free (data->uri_prefix);
		data->uri_prefix = uri_prefix;

		g_node_unlink (cur);
		g_node_prepend (parent, cur);
	}
}

static void
file_weak_ref_notify (gpointer  user_data,
                      GObject  *prev_location)
{
	FileNodeData *data;
	GNode *node;

	node = user_data;
	data = node->data;

	g_assert (data->file == (GFile *) prev_location);

	data->file = NULL;
	reparent_child_nodes_to_parent (node);

	/* Delete node tree here */
	file_node_data_free (data, NULL);
	g_node_destroy (node);
}

static GNode *
file_system_get_node (TrackerFileSystem *file_system,
                      GFile             *file)
{
	TrackerFileSystemPrivate *priv;
	GArray *node_data;
	GNode *node = NULL;

	node_data = g_object_get_qdata (G_OBJECT (file), quark_file_node);

	if (node_data) {
		NodeLookupData *cur;
		guint i;

		for (i = 0; i < node_data->len; i++) {
			cur = &g_array_index (node_data, NodeLookupData, i);

			if (cur->file_system == file_system) {
				node = cur->node;
			}
		}
	}

	if (!node) {
		priv = file_system->priv;
		node = file_tree_lookup (priv->file_tree, file,
		                         NULL, NULL);
	}

	return node;
}

GFile *
tracker_file_system_get_file (TrackerFileSystem *file_system,
                              GFile             *file,
                              GFileType          file_type,
                              GFile             *parent)
{
	TrackerFileSystemPrivate *priv;
	FileNodeData *data;
	GNode *node, *parent_node;
	gchar *uri_prefix = NULL;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);

	priv = file_system->priv;
	node = NULL;
	parent_node = NULL;

	if (parent) {
		parent_node = file_system_get_node (file_system, parent);
		node = file_tree_lookup (parent_node, file,
		                         NULL, &uri_prefix);
	} else {
		node = file_tree_lookup (priv->file_tree, file,
		                         &parent_node, &uri_prefix);
	}

	if (!node) {
		if (!parent_node) {
			gchar *uri;

			uri = g_file_get_uri (file);
			g_warning ("Could not find parent node for URI:'%s'", uri);
			g_warning ("NOTE: URI theme may be outside scheme expected, for example, expecting 'file://' when given 'http://' prefix.");
			g_free (uri);

			return NULL;
		}

		node = g_node_new (NULL);

		/* Parent was found, add file as child */
		data = file_node_data_new (file_system, file,
		                           file_type, node);
		data->uri_prefix = uri_prefix;

		g_node_append (parent_node, node);
	} else {
		data = node->data;
		g_free (uri_prefix);

		/* Update file type if it was unknown */
		if (data->file_type == G_FILE_TYPE_UNKNOWN) {
			data->file_type = file_type;
		}
	}

	return data->file;
}

GFile *
tracker_file_system_peek_file (TrackerFileSystem *file_system,
                               GFile             *file)
{
	GNode *node;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);

	node = file_system_get_node (file_system, file);

	if (node) {
		FileNodeData *data;

		data = node->data;
		return data->file;
	}

	return NULL;
}

GFile *
tracker_file_system_peek_parent (TrackerFileSystem *file_system,
                                 GFile             *file)
{
	GNode *node;

	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);

	node = file_system_get_node (file_system, file);

	if (node) {
		FileNodeData *parent_data;
		GNode *parent;

		parent = node->parent;
		parent_data = parent->data;

		return parent_data->file;
	}

	return NULL;
}

typedef struct {
	TrackerFileSystemTraverseFunc func;
	gpointer user_data;
	GSList *ignore_children;
} TraverseData;

static gint
node_is_child_of_ignored (gconstpointer a,
                          gconstpointer b)
{
	if (g_node_is_ancestor ((GNode *) a, (GNode *) b))
		return 0;

	return 1;
}

static gboolean
traverse_filesystem_func (GNode    *node,
                          gpointer  user_data)
{
	TraverseData *data = user_data;
	FileNodeData *node_data;
	gboolean retval = FALSE;

	node_data = node->data;

	if (!data->ignore_children ||
	    !g_slist_find_custom (data->ignore_children,
	                          node, node_is_child_of_ignored)) {
		/* This node isn't a child of an
		 * ignored one, execute callback
		 */
		retval = data->func (node_data->file, data->user_data);
	}

	/* Avoid recursing within the children of this node */
	if (retval) {
		data->ignore_children = g_slist_prepend (data->ignore_children,
		                                         node);
	}

	return FALSE;
}

void
tracker_file_system_traverse (TrackerFileSystem             *file_system,
                              GFile                         *root,
                              GTraverseType                  order,
                              TrackerFileSystemTraverseFunc  func,
                              gint                           max_depth,
                              gpointer                       user_data)
{
	TrackerFileSystemPrivate *priv;
	TraverseData data;
	GNode *node;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (func != NULL);

	priv = file_system->priv;

	if (root) {
		node = file_system_get_node (file_system, root);
	} else {
		node = priv->file_tree;
	}

	data.func = func;
	data.user_data = user_data;
	data.ignore_children = NULL;

	g_node_traverse (node,
	                 order,
	                 G_TRAVERSE_ALL,
	                 max_depth,
	                 traverse_filesystem_func,
	                 &data);

	g_slist_free (data.ignore_children);
}

void
tracker_file_system_register_property (GQuark             prop,
                                       GDestroyNotify     destroy_notify)
{
	g_return_if_fail (prop != 0);

	if (!properties) {
		properties = g_hash_table_new (NULL, NULL);
	}

	if (g_hash_table_contains (properties, GUINT_TO_POINTER (prop))) {
		g_warning ("FileSystem: property '%s' has been already registered",
		           g_quark_to_string (prop));
		return;
	}

	g_hash_table_insert (properties,
	                     GUINT_TO_POINTER (prop),
	                     destroy_notify);
}

static int
search_property_node (gconstpointer key,
                      gconstpointer item)
{
	const FileNodeProperty *key_prop, *prop;

	key_prop = key;
	prop = item;

	if (key_prop->prop_quark < prop->prop_quark)
		return -1;
	else if (key_prop->prop_quark > prop->prop_quark)
		return 1;

	return 0;
}

void
tracker_file_system_set_property (TrackerFileSystem *file_system,
                                  GFile             *file,
                                  GQuark             prop,
                                  gpointer           prop_data)
{
	FileNodeProperty property, *match;
	GDestroyNotify destroy_notify;
	FileNodeData *data;
	GNode *node;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (file != NULL);
	g_return_if_fail (prop != 0);

	if (!properties ||
	    !g_hash_table_lookup_extended (properties,
	                                   GUINT_TO_POINTER (prop),
	                                   NULL, (gpointer *) &destroy_notify)) {
		g_warning ("FileSystem: property '%s' is not registered",
		           g_quark_to_string (prop));
		return;
	}

	node = file_system_get_node (file_system, file);
	g_return_if_fail (node != NULL);

	data = node->data;

	property.prop_quark = prop;
	match = bsearch (&property, data->properties->data,
	                 data->properties->len, sizeof (FileNodeProperty),
	                 search_property_node);

	if (match) {
		if (destroy_notify) {
			(destroy_notify) (match->value);
		}

		match->value = prop_data;
	} else {
		FileNodeProperty *item;
		guint i;

		/* No match, insert new element */
		for (i = 0; i < data->properties->len; i++) {
			item = &g_array_index (data->properties,
			                       FileNodeProperty, i);

			if (item->prop_quark > prop) {
				break;
			}
		}

		property.value = prop_data;

		if (i >= data->properties->len) {
			g_array_append_val (data->properties, property);
		} else {
			g_array_insert_val (data->properties, i, property);
		}
	}
}

gboolean
tracker_file_system_get_property_full (TrackerFileSystem *file_system,
                                       GFile             *file,
                                       GQuark             prop,
                                       gpointer          *prop_data)
{
	FileNodeData *data;
	FileNodeProperty property, *match;
	GNode *node;

	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (prop > 0, NULL);

	node = file_system_get_node (file_system, file);
	g_return_val_if_fail (node != NULL, NULL);

	data = node->data;
	property.prop_quark = prop;

	match = bsearch (&property, data->properties->data,
	                 data->properties->len, sizeof (FileNodeProperty),
	                 search_property_node);

	if (prop_data)
		*prop_data = (match) ? match->value : NULL;

	return match != NULL;
}

gpointer
tracker_file_system_get_property (TrackerFileSystem *file_system,
                                  GFile             *file,
                                  GQuark             prop)
{
	gpointer data;

	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (prop > 0, NULL);

	tracker_file_system_get_property_full (file_system, file, prop, &data);

	return data;
}

void
tracker_file_system_unset_property (TrackerFileSystem *file_system,
                                    GFile             *file,
                                    GQuark             prop)
{
	FileNodeData *data;
	FileNodeProperty property, *match;
	GDestroyNotify destroy_notify = NULL;
	GNode *node;
	guint index;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (file != NULL);
	g_return_if_fail (prop > 0);

	if (!properties ||
	    !g_hash_table_lookup_extended (properties,
	                                   GUINT_TO_POINTER (prop),
	                                   NULL,
	                                   (gpointer *) &destroy_notify)) {
		g_warning ("FileSystem: property '%s' is not registered",
		           g_quark_to_string (prop));
	}

	node = file_system_get_node (file_system, file);
	g_return_if_fail (node != NULL);

	data = node->data;
	property.prop_quark = prop;

	match = bsearch (&property, data->properties->data,
	                 data->properties->len, sizeof (FileNodeProperty),
	                 search_property_node);

	if (!match) {
		return;
	}

	if (destroy_notify) {
		(destroy_notify) (match->value);
	}

	/* Find out the index from memory positions */
	index = (guint) ((FileNodeProperty *) match -
	                 (FileNodeProperty *) data->properties->data);
	g_assert (index < data->properties->len);

	g_array_remove_index (data->properties, index);
}

typedef struct {
	TrackerFileSystem *file_system;
	GList *list;
	GFileType file_type;
} ForgetFilesData;

static gboolean
append_deleted_files (GNode    *node,
		      gpointer  user_data)
{
	ForgetFilesData *data;
	FileNodeData *node_data;

	data = user_data;
	node_data = node->data;

	if (data->file_type == G_FILE_TYPE_UNKNOWN ||
	    node_data->file_type == data->file_type) {
		data->list = g_list_prepend (data->list, node_data);
	}

	return FALSE;
}

static void
forget_file (FileNodeData *node_data)
{
	if (!node_data->unowned) {
		node_data->unowned = TRUE;

		/* Weak reference handler will remove the file from the tree and
		 * clean up node_data if this is the final reference.
		 */
		g_object_unref (node_data->file);
	}
}

void
tracker_file_system_forget_files (TrackerFileSystem *file_system,
				  GFile             *root,
				  GFileType          file_type)
{
	ForgetFilesData data = { file_system, NULL, file_type };
	GNode *node;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (G_IS_FILE (root));

	node = file_system_get_node (file_system, root);
	g_return_if_fail (node != NULL);

	/* We need to get the files to delete into a list, so
	 * the node tree isn't modified during traversal.
	 */
	g_node_traverse (node,
	                 G_PRE_ORDER,
	                 (file_type == G_FILE_TYPE_REGULAR) ?
	                   G_TRAVERSE_LEAVES : G_TRAVERSE_ALL,
	                 -1, append_deleted_files,
	                 &data);

	g_list_foreach (data.list, (GFunc) forget_file, NULL);
	g_list_free (data.list);
}

GFileType
tracker_file_system_get_file_type (TrackerFileSystem *file_system,
                                   GFile             *file)
{
	GFileType file_type = G_FILE_TYPE_UNKNOWN;
	GNode *node;

	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), file_type);
	g_return_val_if_fail (G_IS_FILE (file), file_type);

	node = file_system_get_node (file_system, file);

	if (node) {
		FileNodeData *node_data;

		node_data = node->data;
		file_type = node_data->file_type;
	}

	return file_type;
}
