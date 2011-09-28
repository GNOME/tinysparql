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

struct _TrackerFileSystemPrivate {
	GNode *file_tree;
	GHashTable *properties;
};

struct _FileNodeProperty {
	GQuark prop_quark;
	gpointer value;
};

struct _FileNodeData {
	GFile *file;
	gchar *uri_suffix;
	GArray *properties;
	guint ref_count;
	guint shallow   : 1;
	guint file_type : 4;
};

G_DEFINE_TYPE (TrackerFileSystem, tracker_file_system, G_TYPE_OBJECT)

/*
 * TrackerFileSystem is a filesystem abstraction, it mainly serves 2 purposes:
 *   - Canonicalizes GFiles, so it is possible later to perform pointer
 *     comparisons on them.
 *   - Stores data for the GFile lifetime, so it may be used as cache store
 *     as long as some file is needed.
 */


static FileNodeData *
file_node_data_new (GFile     *file,
                    GFileType  file_type)
{
	FileNodeData *data;

	data = g_slice_new0 (FileNodeData);
	data->file = g_object_ref (file);
	data->file_type = file_type;
	data->properties = g_array_new (FALSE, TRUE, sizeof (FileNodeProperty));
	data->ref_count = 1;

	return data;
}

static FileNodeData *
file_node_data_root_new (void)
{
	FileNodeData *data;

	data = g_slice_new0 (FileNodeData);
	data->uri_suffix = g_strdup ("file:///");
	data->file = g_file_new_for_uri (data->uri_suffix);
	data->properties = g_array_new (FALSE, TRUE, sizeof (FileNodeProperty));
	data->file_type = G_FILE_TYPE_DIRECTORY;
	data->shallow = TRUE;
	data->ref_count = 1;

	return data;
}

static void
file_node_data_free (FileNodeData *data)
{
	g_object_unref (data->file);
	g_free (data->uri_suffix);
	g_array_free (data->properties, TRUE);

	g_slice_free (FileNodeData, data);
}

static gboolean
file_node_data_equal_or_child (GNode  *node,
                               gchar  *uri_suffix,
                               gchar **uri_remainder)
{
	FileNodeData *data;
	gsize len;

	data = node->data;
	len = strlen (data->uri_suffix);

	if (strncmp (uri_suffix, data->uri_suffix, len) == 0) {
		uri_suffix += len;

		if (uri_suffix[0] == '/') {
			uri_suffix++;
		} else if (uri_suffix[0] != '\0' &&
			   strcmp (data->uri_suffix + len - 4, ":///") != 0) {
			/* If the first char isn't an uri separator
			 * nor \0, node represents a similarly named
			 * file, but not a parent after all.
			 */
			return FALSE;
		}

		if (uri_remainder) {
			*uri_remainder = uri_suffix;
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

	g_assert (G_NODE_IS_ROOT (tree));

	uri = ptr = g_file_get_uri (file);
	node_found = parent_found = NULL;

	/* Run through the filesystem tree, comparing chunks of
	 * uri with the uri suffix in the file nodes, this would
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

	/* First check the root node */
	if (!file_node_data_equal_or_child (tree, uri, &ptr)) {
		g_free (uri);
		return NULL;
	}

	parent = tree;

	while (parent) {
		GNode *child, *next = NULL;
		gchar *ret_ptr;

		for (child = g_node_first_child (parent);
		     child != NULL;
		     child = g_node_next_sibling (child)) {
			data = child->data;

			if (data->uri_suffix[0] != ptr[0])
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
	file_node_data_free (node->data);
	return TRUE;
}

/* TrackerFileSystem implementation */

static void
tracker_file_system_finalize (GObject *object)
{
	TrackerFileSystemPrivate *priv;

	priv = TRACKER_FILE_SYSTEM (object)->priv;

	g_node_traverse (priv->file_tree,
	                 G_POST_ORDER,
	                 G_TRAVERSE_ALL, -1,
	                 file_tree_free_node_foreach,
	                 NULL);
	g_node_destroy (priv->file_tree);

	G_OBJECT_CLASS (tracker_file_system_parent_class)->finalize (object);
}

static void
tracker_file_system_class_init (TrackerFileSystemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_file_system_finalize;

	g_type_class_add_private (object_class,
	                          sizeof (TrackerFileSystemPrivate));
}
static void
tracker_file_system_init (TrackerFileSystem *file_system)
{
	TrackerFileSystemPrivate *priv;
	FileNodeData *root_data;

	file_system->priv = priv =
		G_TYPE_INSTANCE_GET_PRIVATE (file_system,
		                             TRACKER_TYPE_FILE_SYSTEM,
		                             TrackerFileSystemPrivate);

	root_data = file_node_data_root_new ();
	priv->file_tree = g_node_new (root_data);
}

TrackerFileSystem *
tracker_file_system_new (void)
{
	return g_object_new (TRACKER_TYPE_FILE_SYSTEM, NULL);
}

static void
reparent_child_nodes (GNode *node,
		      GNode *new_parent)
{
	FileNodeData *parent_data;
	GNode *child;

	parent_data = new_parent->data;
	child = g_node_first_child (node);

	while (child) {
		FileNodeData *data;
		GNode *cur;

		cur = child;
		data = cur->data;
		child = g_node_next_sibling (child);

		/* Ensure consistency is preserved */
		if (!g_file_has_prefix (data->file, parent_data->file)) {
			continue;
		}

		data->uri_suffix = g_file_get_relative_path (parent_data->file,
							     data->file);

		g_node_unlink (cur);
		g_node_append (new_parent, cur);
	}
}

TrackerFile *
tracker_file_system_get_file (TrackerFileSystem *file_system,
                              GFile             *file,
			      GFileType          file_type,
			      TrackerFile       *parent)
{
	TrackerFileSystemPrivate *priv;
	FileNodeData *data;
	GNode *node, *parent_node;
	gchar *uri_suffix = NULL;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);

	priv = file_system->priv;
	node = NULL;

	if (parent) {
		FileNodeData *parent_data;
		GNode *child;

		parent_node = (GNode *) parent;
		parent_data = parent_node->data;

		/* Find child node, if any */
		for (child = g_node_first_child (parent_node);
		     child != NULL;
		     child = g_node_next_sibling (child)) {
			FileNodeData *child_data;

			child_data = child->data;

			if (g_file_equal (child_data->file, file)) {
				node = child;
				break;
			}
		}

		if (!node) {
			gchar *uri, *parent_uri;
			const gchar *ptr;
			gint len;

			uri = g_file_get_uri (file);
			parent_uri = g_file_get_uri (parent_data->file);
			len = strlen (parent_uri);

			ptr = uri + len;
			g_assert (ptr[0] == '/');

			ptr++;
			uri_suffix = g_strdup (ptr);

			g_free (parent_uri);
			g_free (uri);
		}
	} else {
		node = file_tree_lookup (priv->file_tree, file,
					 &parent_node, &uri_suffix);
	}

	if (!node) {
		g_assert (parent_node != NULL);

		/* Parent was found, add file as child */
		data = file_node_data_new (file, file_type);
		data->uri_suffix = uri_suffix;

		node = g_node_new (data);

		/* Reparent any previously created child to it,
		 * that would currently be a child of parent_node
		 */
		reparent_child_nodes (parent_node, node);

		g_node_append (parent_node, node);
	} else {
		/* Increment reference count of node if found */
		data = node->data;
		g_atomic_int_inc (&data->ref_count);

		g_free (uri_suffix);
	}

	return (TrackerFile *) node;
}

TrackerFile *
tracker_file_system_peek_file (TrackerFileSystem *file_system,
			       GFile             *file)
{
	TrackerFileSystemPrivate *priv;
	GNode *node;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);

	priv = file_system->priv;
	node = file_tree_lookup (priv->file_tree, file, NULL, NULL);

	return (TrackerFile *) node;
}

TrackerFile *
tracker_file_system_ref_file (TrackerFileSystem  *file_system,
                              TrackerFile        *file)
{
	FileNodeData *data;

	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);
	g_return_val_if_fail (file != NULL, NULL);

	data = ((GNode *) file)->data;

	g_atomic_int_inc (&data->ref_count);

	return file;
}

void
tracker_file_system_unref_file (TrackerFileSystem  *file_system,
                                TrackerFile        *file)
{
	FileNodeData *data;
	GNode *node;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (file != NULL);

	node = (GNode *) file;
	data = node->data;

	if (g_atomic_int_dec_and_test (&data->ref_count)) {
		reparent_child_nodes (node, node->parent);

		g_assert (g_node_first_child (node) == NULL);
		g_node_destroy (node);
	}
}


GFile *
tracker_file_system_resolve_file (TrackerFileSystem *file_system,
                                  TrackerFile       *file)
{
	FileNodeData *data;

	g_return_val_if_fail (file != NULL, NULL);

	data = ((GNode *) file)->data;
	return data->file;
}

void
tracker_file_system_traverse (TrackerFileSystem             *file_system,
			      TrackerFile                   *root,
			      GTraverseType                  order,
			      TrackerFileSystemTraverseFunc  func,
			      gpointer                       user_data)
{
	TrackerFileSystemPrivate *priv;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (func != NULL);

	priv = file_system->priv;
	g_node_traverse ((root) ? (GNode *) root : priv->file_tree,
			 order,
			 G_TRAVERSE_ALL,
			 -1,
			 (GNodeTraverseFunc) func,
			 user_data);
}

void
tracker_file_system_register_property (TrackerFileSystem *file_system,
                                       GQuark             prop,
                                       GDestroyNotify     destroy_notify)
{
	TrackerFileSystemPrivate *priv;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (prop != 0);

	priv = file_system->priv;

	if (!priv->properties) {
		priv->properties = g_hash_table_new (NULL, NULL);
	}

	if (g_hash_table_lookup (priv->properties, GUINT_TO_POINTER (prop))) {
		g_warning ("FileSystem: property '%s' has been already registered",
		           g_quark_to_string (prop));
		return;
	}

	g_hash_table_insert (priv->properties,
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
                                  TrackerFile       *file,
                                  GQuark             prop,
                                  gpointer           prop_data)
{
	TrackerFileSystemPrivate *priv;
	FileNodeProperty property, *match;
	GDestroyNotify destroy_notify;
	FileNodeData *data;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (file != NULL);
	g_return_if_fail (prop != 0);

	priv = file_system->priv;

	if (!priv->properties ||
	    !g_hash_table_lookup_extended (priv->properties,
	                                   GUINT_TO_POINTER (prop),
	                                   NULL, (gpointer *) &destroy_notify)) {
		g_warning ("FileSystem: property '%s' is not registered",
		           g_quark_to_string (prop));
		return;
	}

	data = ((GNode *) file)->data;

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

gpointer
tracker_file_system_get_property (TrackerFileSystem *file_system,
                                  TrackerFile       *file,
                                  GQuark             prop)
{
	FileNodeData *data;
	FileNodeProperty property, *match;

	g_return_val_if_fail (TRACKER_IS_FILE_SYSTEM (file_system), NULL);
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (prop > 0, NULL);

	data = ((GNode *) file)->data;

	property.prop_quark = prop;

	match = bsearch (&property, data->properties->data,
	                 data->properties->len, sizeof (FileNodeProperty),
	                 search_property_node);

	return (match) ? match->value : NULL;
}

void
tracker_file_system_unset_property (TrackerFileSystem *file_system,
                                    TrackerFile       *file,
                                    GQuark             prop)
{
	TrackerFileSystemPrivate *priv;
	FileNodeData *data;
	FileNodeProperty property, *match;
	GDestroyNotify destroy_notify;
	guint index;

	g_return_if_fail (TRACKER_IS_FILE_SYSTEM (file_system));
	g_return_if_fail (file != NULL);
	g_return_if_fail (prop > 0);

	priv = file_system->priv;

	if (!priv->properties ||
	    !g_hash_table_lookup_extended (priv->properties,
	                                   GUINT_TO_POINTER (prop),
	                                   NULL,
	                                   (gpointer *) &destroy_notify)) {
		g_warning ("FileSystem: property '%s' is not registered",
		           g_quark_to_string (prop));
	}

	data = ((GNode *) file)->data;

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
	g_assert (index >= 0 && index < data->properties->len);

	g_array_remove_index (data->properties, index);
}
