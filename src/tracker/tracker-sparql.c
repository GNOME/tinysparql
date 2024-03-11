/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2014, Softathome <philippe.judge@softathome.com>
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <sys/param.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-common/tracker-common.h>

#include "tracker-sparql.h"
#include "tracker-color.h"

#define LINK_STR "[🡕]" /* NORTH EAST SANS-SERIF ARROW, in consistence with systemd */

#define SPARQL_OPTIONS_ENABLED() \
	(list_classes || \
	 list_class_prefixes || \
	 list_properties || \
	 list_notifies || \
	 list_indexes || \
	 list_graphs || \
	 tree || \
	 search || \
	 get_shorthand || \
	 get_longhand || \
	 file || \
	 query)

typedef struct _NodeData NodeData;
typedef struct _NodeFindData NodeFindData;
typedef struct _NodePrintData NodePrintData;

struct _NodeData {
	gchar *class;
	GSList *properties;
	guint parent_known:1;
};

struct _NodeFindData {
	GEqualFunc func;
	GNode *node;
	const gchar *class;
};

struct _NodePrintData {
	TrackerNamespaceManager *namespaces;
	GHashTable  *filter_parents;
	const gchar *highlight_text;
};

static gboolean parse_list_properties (const gchar  *option_name,
                                       const gchar  *value,
                                       gpointer      data,
                                       GError      **error);
static gboolean parse_list_notifies   (const gchar  *option_name,
                                       const gchar  *value,
                                       gpointer      data,
                                       GError      **error);
static gboolean parse_list_indexes    (const gchar  *option_name,
                                       const gchar  *value,
                                       gpointer      data,
                                       GError      **error);
static gboolean parse_tree            (const gchar  *option_name,
                                       const gchar  *value,
                                       gpointer      data,
                                       GError      **error);

static gchar *file;
static gchar *query;
static gboolean update;
static gboolean list_classes;
static gboolean list_class_prefixes;
static gchar *list_properties;
static gchar *list_notifies;
static gchar *list_indexes;
static gchar *list_graphs;
static gchar *tree;
static gchar *get_shorthand;
static gchar *get_longhand;
static gchar *search;
static gchar *database_path;
static gchar *dbus_service;
static gchar *remote_service;
static gchar **args;

static GOptionEntry entries[] = {
	{ "database", 'd', 0, G_OPTION_ARG_FILENAME, &database_path,
	  N_("Location of the database"),
	  N_("FILE")
	},
	{ "dbus-service", 'b', 0, G_OPTION_ARG_STRING, &dbus_service,
	  N_("Connects to a DBus service"),
	  N_("DBus service name")
	},
	{ "remote-service", 'r', 0, G_OPTION_ARG_STRING, &remote_service,
	  N_("Connects to a remote service"),
	  N_("Remote service URI")
	},
	{ "file", 'f', 0, G_OPTION_ARG_FILENAME, &file,
	  N_("Path to use to run a query or update from file"),
	  N_("FILE"),
	},
	{ "query", 'q', 0, G_OPTION_ARG_STRING, &query,
	  N_("SPARQL query"),
	  N_("SPARQL"),
	},
	{ "update", 'u', 0, G_OPTION_ARG_NONE, &update,
	  N_("This is used with --query and for database updates only."),
	  NULL,
	},
	{ "list-classes", 'c', 0, G_OPTION_ARG_NONE, &list_classes,
	  N_("Retrieve classes"),
	  NULL,
	},
	{ "list-class-prefixes", 'x', 0, G_OPTION_ARG_NONE, &list_class_prefixes,
	  N_("Retrieve class prefixes"),
	  NULL,
	},
	{ "list-properties", 'p', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, parse_list_properties,
	  N_("Retrieve properties for a class, prefixes can be used too (e.g. rdfs:Resource)"),
	  N_("CLASS"),
	},
	{ "list-notifies", 'n', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, parse_list_notifies,
	  N_("Retrieve classes which notify changes in the database (CLASS is optional)"),
	  N_("CLASS"),
	},
	{ "list-indexes", 'i', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, parse_list_indexes,
	  N_("Retrieve indexes used in database to improve performance (PROPERTY is optional)"),
	  N_("PROPERTY"),
	},
	{ "list-graphs", 'g', 0, G_OPTION_ARG_NONE, &list_graphs,
	  N_("Retrieve all named graphs"),
	  NULL,
	},
	{ "tree", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, parse_tree,
	  N_("Describe subclasses, superclasses (can be used with -s to highlight parts of the tree and -p to show properties)"),
	  N_("CLASS"),
	},
	{ "search", 's', 0, G_OPTION_ARG_STRING, &search,
	  N_("Search for a class or property and display more information (e.g. Document)"),
	  N_("CLASS/PROPERTY"),
	},
	{ "get-shorthand", 0, 0, G_OPTION_ARG_STRING, &get_shorthand,
	  N_("Returns the shorthand for a class (e.g. nfo:FileDataObject)."),
	  N_("CLASS"),
	},
	{ "get-longhand", 0, 0, G_OPTION_ARG_STRING, &get_longhand,
	  N_("Returns the full namespace for a class."),
	  N_("CLASS"),
	},
	{ "arg", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &args,
	  N_("Provides an argument for a query parameter."),
	  N_("PARAMETER:TYPE:VALUE"),
	},
	{ NULL }
};

static TrackerSparqlConnection *
create_connection (GError **error)
{
	if (database_path && !dbus_service && !remote_service) {
		GFile *file;

		file = g_file_new_for_commandline_arg (database_path);
		return tracker_sparql_connection_new (update ?
						      TRACKER_SPARQL_CONNECTION_FLAGS_NONE :
						      TRACKER_SPARQL_CONNECTION_FLAGS_READONLY,
		                                      file, NULL, NULL, error);
	} else if (dbus_service && !database_path && !remote_service) {
		GDBusConnection *dbus_conn;

		dbus_conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
		if (!dbus_conn)
			return NULL;

		return tracker_sparql_connection_bus_new (dbus_service, NULL, dbus_conn, error);
	} else if (remote_service && !database_path && !dbus_service) {
		return tracker_sparql_connection_remote_new (remote_service);
	} else {
		/* TRANSLATORS: Those are commandline arguments */
		g_printerr ("%s\n", _("Specify one “--database”, “--dbus-service” or “--remote-service” option"));
		exit (EXIT_FAILURE);
	}
}

static gboolean
parse_list_properties (const gchar  *option_name,
                       const gchar  *value,
                       gpointer      data,
                       GError      **error)
{
	if (!value) {
		list_properties = g_strdup ("");
	} else {
		list_properties = g_strdup (value);
	}

	return TRUE;
}

static gboolean
parse_list_notifies (const gchar  *option_name,
                     const gchar  *value,
                     gpointer      data,
                     GError      **error)
{
	if (!value) {
		list_notifies = g_strdup ("");
	} else {
		list_notifies = g_strdup (value);
	}

	return TRUE;
}

static gboolean
parse_list_indexes (const gchar  *option_name,
                    const gchar  *value,
                    gpointer      data,
                    GError      **error)
{
	if (!value) {
		list_indexes = g_strdup ("");
	} else {
		list_indexes = g_strdup (value);
	}

	return TRUE;
}

static gboolean
parse_tree (const gchar  *option_name,
            const gchar  *value,
            gpointer      data,
            GError      **error)
{
	if (!value) {
		tree = g_strdup ("");
	} else {
		tree = g_strdup (value);
	}

	return TRUE;
}

static gchar *
get_shorthand_str_for_offsets (TrackerNamespaceManager *namespaces,
                               const gchar             *str)
{
	GString *result = NULL;
	gchar **properties;
	gint i;

	if (!str) {
		return NULL;
	}

	properties = g_strsplit (str, ",", -1);
	if (!properties) {
		return NULL;
	}

	for (i = 0; properties[i] != NULL && properties[i + 1] != NULL; i += 2) {
		const gchar *property;
		const gchar *offset;
		gchar *shorthand;

		property = properties[i];
		offset = properties[i + 1];

		if (!property || !offset) {
			g_warning ("Expected property AND offset to be valid for fts:offset results");
			continue;
		}

		shorthand = tracker_namespace_manager_compress_uri (namespaces,
		                                                    property);

		if (!result) {
			result = g_string_new ("");
		} else {
			result = g_string_append_c (result, ' ');
		}

		g_string_append_printf (result, "%s:%s", shorthand, offset);
		g_free (shorthand);
	}

	g_strfreev (properties);

	return result ? g_string_free (result, FALSE) : NULL;
}

static void
print_cursor_with_ftsoffsets (TrackerSparqlCursor *cursor,
                              const gchar         *none_found,
                              const gchar         *heading,
                              gboolean             only_first_col)
{
	if (!cursor) {
		g_print ("%s\n", none_found);
	} else {
		TrackerSparqlConnection *conn;
		TrackerNamespaceManager *namespaces;
		gint count = 0;

		g_print ("%s:\n", heading);

		conn = tracker_sparql_cursor_get_connection (cursor);
		namespaces = tracker_sparql_connection_get_namespace_manager (conn);

		if (only_first_col) {
			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				const gchar *str;
				gchar *shorthand;

				str = tracker_sparql_cursor_get_string (cursor, 0, NULL);
				shorthand = get_shorthand_str_for_offsets (namespaces, str);
				g_print ("  %s\n", shorthand ? shorthand : str);
				g_free (shorthand);
				count++;
			}
		} else {
			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				gint col;

				for (col = 0; col < tracker_sparql_cursor_get_n_columns (cursor); col++) {
					const gchar *str;
					gchar *shorthand;

					str = tracker_sparql_cursor_get_string (cursor, col, NULL);
					shorthand = get_shorthand_str_for_offsets (namespaces, str);
					g_print ("%c %s",
					         col == 0 ? ' ' : ',',
					         shorthand ? shorthand : str);
					g_free (shorthand);
				}

				g_print ("\n");

				count++;
			}
		}

		if (count == 0) {
			/* To translators: This is to say there are no
			 * search results found. We use a "foo: None"
			 * with multiple print statements, where "foo"
			 * may be Music or Images, etc. */
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		g_object_unref (cursor);
	}
}

static void
print_cursor (TrackerSparqlCursor *cursor,
              const gchar         *none_found,
              const gchar         *heading,
              gboolean             only_first_col)
{
	if (!cursor) {
		g_print ("%s\n", none_found);
	} else {
		gint count = 0;

		g_print ("%s:\n", heading);

		if (only_first_col) {
			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				g_print ("  %s\n", tracker_sparql_cursor_get_string (cursor, 0, NULL));
				count++;
			}
		} else {
			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				gint col;

				for (col = 0; col < tracker_sparql_cursor_get_n_columns (cursor); col++) {
					g_print ("%c %s",
					         col == 0 ? ' ' : ',',
					         tracker_sparql_cursor_get_string (cursor, col, NULL));
				}

				g_print ("\n");

				count++;
			}
		}

		if (count == 0) {
			/* To translators: This is to say there are no
			 * search results found. We use a "foo: None"
			 * with multiple print statements, where "foo"
			 * may be Music or Images, etc. */
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		g_object_unref (cursor);
	}
}

static NodeData *
tree_node_data_new (const gchar *class,
                    gboolean     parent_known)
{
	NodeData *data;

	data = g_slice_new0 (NodeData);
	data->class = g_strdup (class);
	data->parent_known = parent_known;

	return data;
}

static void
tree_data_free (gpointer data,
                gpointer user_data)
{
	g_free (data);
}

static void
tree_node_data_free (NodeData *data)
{
	if (data == NULL) {
		return;
	}

	if (data->properties) {
		g_slist_foreach (data->properties, tree_data_free, NULL);
		g_slist_free (data->properties);
	}

	g_free (data->class);
	g_slice_free (NodeData, data);
}

static GNode *
tree_new (void)
{
	return g_node_new (NULL);
}

static gboolean
tree_free_foreach (GNode    *node,
                   gpointer  data)
{
	tree_node_data_free (node->data);
	return FALSE;
}

static void
tree_free (GNode *node)
{
	g_node_traverse (node,
	                 G_POST_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 tree_free_foreach,
	                 NULL);
	g_node_destroy (node);
}

static gboolean
tree_node_find_foreach (GNode    *node,
                        gpointer  user_data)
{
	NodeFindData *data;
	NodeData *node_data;

	if (!node) {
		return FALSE;
	}

	node_data = node->data;

	if (!node_data) {
		return FALSE;
	}

	data = user_data;

	if ((data->func) (data->class, node_data->class)) {
		data->node = node;
		return TRUE;
	}

	return FALSE;
}

static GNode *
tree_node_find (GNode       *node,
                const gchar *class,
                GEqualFunc   func)
{
	NodeFindData data;

	data.class = class;
	data.node = NULL;
	data.func = func;

	g_node_traverse (node,
	                 G_POST_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 tree_node_find_foreach,
	                 &data);

	return data.node;
}

static GNode *
tree_node_lookup (GNode        *tree,
                  const gchar  *class,
                  GNode       **parent_node)
{
	GNode *node;

	node = tree_node_find (tree, class, g_str_equal);

	if (parent_node) {
		if (node) {
			*parent_node = node->parent;
		} else {
			*parent_node = NULL;
		}
	}

	return node;
}

static GNode *
tree_add_class (GNode       *root,
                const gchar *class,
                const gchar *parent,
                gboolean     parent_known)
{
	GNode *node, *parent_node;

	/* Look up class node */
	node = parent_node = NULL;

	/* If there is no class, node is root */
	if (class) {
		node = tree_node_lookup (root, class, &parent_node);
	}

	if (!node) {
		/* Create node */
		NodeData *data;

		data = tree_node_data_new (class, parent_known);
		node = g_node_new (data);

		if (!parent_known || !parent) {
			/* Add to root node */
			g_node_append (root, node);

			/* If node is currently an orphan, add to root
			 * and we will reorder it when we know more...
			 */
		} else {
			/* Lookup parent node and add to that. */
			parent_node = tree_node_lookup (root, parent, NULL);

			if (!parent_node) {
				/* Create parent node. */
				parent_node = tree_add_class (root, parent, NULL, FALSE);
				g_assert (parent_node != NULL);
			}

			g_node_append (parent_node, node);
		}
	} else {
		/* Lookup parent node and add to that. */
		parent_node = tree_node_lookup (root, parent, NULL);

		/* Reparent found node, if we need to */
		if (parent_node) {
			NodeData *parent_data;

			parent_data = parent_node->data;

			if (!parent_data->parent_known) {
				/* Add to right parent. */
				g_node_append (parent_node, node);
				parent_data->parent_known = TRUE;
			} else {
				NodeData *data;

				/* Cater for multiple parents, create
				 * new node if parents differ */
				data = tree_node_data_new (class, TRUE);
				node = g_node_new (data);
				g_node_append (parent_node, node);
			}
		}
	}

	return node;
}

static gboolean
tree_add_property (GNode       *root,
                   const gchar *class,
                   const gchar *property)
{
	GNode *node;
	NodeData *data;

	if (!property) {
		return FALSE;
	}

	node = tree_node_lookup (root, class, NULL);
	if (!node) {
		return FALSE;
	}

	data = node->data;

	data->properties = g_slist_prepend (data->properties,
	                                    g_strdup (property));

	return TRUE;
}

static gchar *
highlight (const gchar *text,
           const gchar *highlight_text)
{
	GString *s;
	gchar *text_down, *highlight_text_down, *p;

	if (!text) {
		return NULL;
	}

	if (!highlight_text) {
		return g_strdup (text);
	}

	s = g_string_new (text);

	text_down = g_utf8_casefold (text, -1);
	highlight_text_down = g_utf8_casefold (highlight_text, -1);
	p = text_down;

	while ((p = strstr (p, highlight_text_down)) != NULL) {
		gint offset_begin, offset_end;

		offset_begin = p - text_down;
		g_string_insert_len (s, offset_begin, SNIPPET_BEGIN, -1);

		offset_end = (p - text_down) + strlen (highlight_text) + strlen (SNIPPET_BEGIN);
		g_string_insert_len (s, offset_end, SNIPPET_END, -1);

		p += offset_end + strlen (SNIPPET_END);
	}

	g_free (highlight_text_down);
	g_free (text_down);

	return g_string_free (s, FALSE);
}

static void
print_link (const gchar *url)
{
	g_print ("\x1B]8;;%s\a" LINK_STR "\x1B]8;;\a", url);
}

static GNode *
get_nth_parent (GNode *node,
                int    n)
{
	while (n && node) {
		node = node->parent;
		n--;
	}

	return node;
}

static inline void
tree_print_properties (GNode         *node,
                       NodePrintData *pd,
                       gint           depth)
{
	NodeData *nd = node->data;
	GSList *l;

	/* Sort first */
	nd->properties = g_slist_sort (nd->properties, (GCompareFunc) g_strcmp0);

	/* Print properties */
	for (l = nd->properties; l; l = l->next) {
		gchar *compressed, *highlighted;
		gint i;

		compressed = tracker_namespace_manager_compress_uri (pd->namespaces, l->data);

		for (i = 1; i <= depth; i++) {
			gboolean has_next;

			if (i == depth) {
				has_next = node->children != NULL;
			} else {
				GNode *parent;

				parent = get_nth_parent (node, depth - i - 1);
				has_next = parent && parent->next;
			}

			if (has_next)
				g_print ("  │");
			else
				g_print ("   ");
		}

		highlighted = highlight (compressed, pd->highlight_text);

		if (l->next)
			g_print ("  ┣");
		else
			g_print ("  ┗");

		g_print ("━ %s", (gchar*) highlighted);
		if (g_str_has_prefix (l->data, "http"))
			print_link (l->data);
		g_print ("\n");
		g_free (highlighted);
		g_free (compressed);
	}
}

static gboolean
tree_print_foreach (GNode    *node,
                    gpointer  user_data)
{
	NodeData *nd;
	NodePrintData *pd;
	gboolean print = TRUE;

	gchar *shorthand, *highlighted;
	const gchar *text;
	gint depth, i;

	nd = node->data;
	pd = user_data;

	if (!nd)
		return FALSE;

	/* Filter based on parent classes */
	if (pd->filter_parents) {
		print = g_hash_table_lookup (pd->filter_parents, nd->class) != NULL;
	}

	if (!print) {
		return FALSE;
	}

	shorthand = tracker_namespace_manager_compress_uri (pd->namespaces, nd->class);

	depth = g_node_depth (node);

	for (i = 1; i < depth; i++) {
		if (i == depth - 1) {
			const gchar *branch = "├";

			if (!node->next) {
				branch = "╰";
			} else if (G_NODE_IS_LEAF (node)) {
				branch = "├";
			}

			g_print ("  %s", branch);
		} else {
			GNode *parent;

			parent = get_nth_parent (node, depth - i - 1);
			if (parent && parent->next)
				g_print ("  │");
			else
				g_print ("   ");
		}
	}

	text = shorthand ? shorthand : nd->class;
	highlighted = highlight (text, pd->highlight_text);
	g_print ("─ %s", highlighted);
	if (g_str_has_prefix (nd->class, "http"))
		print_link (nd->class);
	g_print ("\n");
	g_free (highlighted);
	g_free (shorthand);

	tree_print_properties (node, pd, depth);

	return FALSE;
}

static void
tree_print (GNode                   *node,
            TrackerNamespaceManager *namespaces,
            GHashTable              *filter_parents,
            const gchar             *highlight_text)
{
	NodePrintData data;

	data.namespaces = namespaces;
	data.filter_parents = filter_parents;
	data.highlight_text = highlight_text;

	g_node_traverse (node,
	                 G_PRE_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 tree_print_foreach,
	                 &data);
}

static TrackerSparqlStatement *
load_statement (TrackerSparqlConnection  *conn,
                const gchar              *filename)
{
	TrackerSparqlStatement *stmt;
	gchar *path;
	GError *error = NULL;

	path = g_strconcat ("/org/freedesktop/tracker/sparql/", filename, NULL);
	stmt = tracker_sparql_connection_load_statement_from_gresource (conn,
	                                                                path,
	                                                                NULL, &error);
	g_free (path);

	g_assert_no_error (error);

	return stmt;
}

static gint
tree_get (TrackerSparqlConnection *connection,
          const gchar             *class_lookup,
          const gchar             *highlight_text)
{
	TrackerSparqlCursor *cursor;
	TrackerNamespaceManager *namespaces;
	TrackerSparqlStatement *stmt;
	GHashTable *filter_parents;
	GError *error = NULL;
	gchar *class_lookup_longhand;
	GNode *root, *found_node, *node;

	root = tree_new ();

	namespaces = tracker_sparql_connection_get_namespace_manager (connection);

	class_lookup_longhand = tracker_namespace_manager_expand_uri (namespaces,
	                                                              class_lookup);

	stmt = load_statement (connection, "get-class-hierarchy.rq");
	cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
	g_object_unref (stmt);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not create tree: subclass query failed"),
		            error->message);
		g_error_free (error);
		g_object_unref (connection);
		g_free (class_lookup_longhand);

		return EXIT_FAILURE;
	}

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *parent = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		const gchar *class = tracker_sparql_cursor_get_string (cursor, 1, NULL);

		tree_add_class (root, class, parent, TRUE);
	}

	/* Create filter (so we only deal with properties for classes
	 * we care about) */
	if (class_lookup_longhand && *class_lookup_longhand) {
		found_node = tree_node_lookup (root, class_lookup_longhand, NULL);
		filter_parents = g_hash_table_new_full (g_str_hash,
		                                        g_str_equal,
		                                        NULL,
		                                        NULL);

		for (node = found_node; node; node = node->parent) {
			NodeData *data = node->data;

			if (!data || !data->class) {
				continue;
			}

			g_hash_table_insert (filter_parents,
			                     data->class,
			                     GINT_TO_POINTER(1));
		}
	} else {
		filter_parents = NULL;
	}

	/* Get properties of classes */
	if (list_properties) {
		TrackerSparqlCursor *properties;

		stmt = load_statement (connection, "get-properties.rq");
		properties = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not create tree: class properties query failed"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			g_free (class_lookup_longhand);

			if (filter_parents) {
				g_hash_table_unref (filter_parents);
			}

			g_object_unref (cursor);

			return EXIT_FAILURE;
		}

		while (tracker_sparql_cursor_next (properties, NULL, NULL)) {
			const gchar *class = tracker_sparql_cursor_get_string (properties, 0, NULL);
			const gchar *property = tracker_sparql_cursor_get_string (properties, 1, NULL);

			tree_add_property (root, class, property);
		}

		g_object_unref (properties);
	}

	g_free (class_lookup_longhand);

	/* Print */
	tree_print (root, namespaces, filter_parents, highlight_text);

	if (filter_parents) {
		g_hash_table_unref (filter_parents);
	}

	g_object_unref (cursor);
	tree_free (root);

	return EXIT_SUCCESS;
}

static void
print_namespaces (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
	g_print ("%s: %s\n", (gchar*) key, (gchar*) value);
}

static gboolean
bind_arguments (TrackerSparqlStatement  *stmt,
                gchar                  **args)
{
	int i;

	for (i = 0; args && args[i]; i++) {
		gchar **pair;

		pair = g_strsplit (args[i], ":", 3);
		if (g_strv_length (pair) != 3) {
			g_printerr (_("Invalid argument string %s"), args[i]);
			g_printerr ("\n");
			return FALSE;
		}

		if (strlen (pair[1]) != 1 ||
		    (pair[1][0] != 'i' &&
		     pair[1][0] != 'd' &&
		     pair[1][0] != 'b' &&
		     pair[1][0] != 's')) {
			g_printerr (_("Invalid parameter type for argument %s"), pair[0]);
			g_printerr ("\n");
			g_strfreev (pair);
			return FALSE;
		}

		switch (pair[1][0]) {
		case 'i': {
			gint64 val;

			val = strtol (pair[2], NULL, 10);
			tracker_sparql_statement_bind_int (stmt, pair[0], val);
			break;
		}
		case 'd': {
			gdouble val;

			val = strtod (pair[2], NULL);
			tracker_sparql_statement_bind_int (stmt, pair[0], val);
			break;
		}
		case 'b': {
			gboolean val;

			val = pair[2][0] == 't' || pair[2][0] == 'T' || pair[2][0] == '1';
			tracker_sparql_statement_bind_boolean (stmt, pair[0], val);
			break;
		}
		case 's':
			tracker_sparql_statement_bind_string (stmt, pair[0], pair[2]);
			break;
		}

		g_strfreev (pair);
	}

	return TRUE;
}

static int
sparql_run (void)
{
	TrackerSparqlConnection *connection;
	TrackerNamespaceManager *namespaces;
	TrackerSparqlStatement *stmt;
	TrackerSparqlCursor *cursor = NULL;
	GError *error = NULL;
	gint retval = EXIT_SUCCESS;

	connection = create_connection (&error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	namespaces = tracker_sparql_connection_get_namespace_manager (connection);

	tracker_term_pipe_to_pager ();

	if (list_classes) {
		stmt = load_statement (connection, "get-classes.rq");
		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not list classes"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		print_cursor (cursor, _("No classes were found"), _("Classes"), TRUE);
	}

	if (list_class_prefixes) {
		tracker_namespace_manager_foreach (namespaces,
		                                   print_namespaces,
		                                   NULL);
	}

	if (list_properties && list_properties[0] != '\0') {
		gchar *class_name;

		class_name = tracker_namespace_manager_expand_uri (namespaces,
		                                                   list_properties);

		stmt = load_statement (connection, "get-properties-for-class.rq");
		tracker_sparql_statement_bind_string (stmt, "class", class_name);
		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);
		g_free (class_name);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not list properties"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		print_cursor (cursor, _("No properties were found"), _("Properties"), TRUE);
	}

	if (list_notifies) {
		if (*list_notifies == '\0') {
			stmt = load_statement (connection, "get-notify-properties.rq");
		} else {
			stmt = load_statement (connection, "get-notify-properties-match.rq");
			tracker_sparql_statement_bind_string (stmt, "match", list_notifies);
		}

		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not find notify classes"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		print_cursor (cursor, _("No notifies were found"), _("Notifies"), TRUE);
	}

	if (list_indexes) {
		if (*list_indexes == '\0') {
			stmt = load_statement (connection, "get-indexed-properties.rq");
		} else {
			stmt = load_statement (connection, "get-indexed-properties-match.rq");
			tracker_sparql_statement_bind_string (stmt, "match", list_indexes);
		}

		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not find indexed properties"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		print_cursor (cursor, _("No indexes were found"), _("Indexes"), TRUE);
	}

	if (list_graphs) {
		stmt = load_statement (connection, "get-graphs.rq");
		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not list named graphs"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		print_cursor (cursor, _("No graphs were found"), _("Named graphs"), TRUE);
	}

	if (tree) {
		retval = tree_get (connection, tree, search);
		goto out;
	}

	if (search) {
		/* First list classes */
		stmt = load_statement (connection, "get-classes-match.rq");
		tracker_sparql_statement_bind_string (stmt, "match", search);
		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not search classes"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		print_cursor (cursor, _("No classes were found to match search term"), _("Classes"), TRUE);

		/* Second list properties */
		stmt = load_statement (connection, "get-properties-match.rq");
		tracker_sparql_statement_bind_string (stmt, "match", search);
		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not search properties"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		print_cursor (cursor, _("No properties were found to match search term"), _("Properties"), TRUE);
	}

	if (get_shorthand) {
		gchar *result;

		result = tracker_namespace_manager_compress_uri (namespaces,
		                                                 get_shorthand);
		g_print ("%s\n", result);
		g_free (result);
	}

	if (get_longhand) {
		gchar *result;

		result = tracker_namespace_manager_expand_uri (namespaces,
		                                               get_longhand);
		g_print ("%s\n", result);
		g_free (result);
	}

	if (file) {
		gchar *path_in_utf8;
		gsize size;

		path_in_utf8 = g_filename_to_utf8 (file, -1, NULL, NULL, &error);
		if (!path_in_utf8) {
			g_assert (error != NULL);
			g_printerr ("%s:'%s', %s\n",
			            _("Could not get UTF-8 path from path"),
			            file,
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		if (!g_file_get_contents (path_in_utf8, &query, &size, &error)) {
			g_assert (error != NULL);
			g_printerr ("%s:'%s', %s\n",
			            _("Could not read file"),
			            path_in_utf8,
			            error->message);
			g_error_free (error);
			g_free (path_in_utf8);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		g_free (path_in_utf8);
	}

	if (query) {
		if (G_UNLIKELY (update)) {
			stmt = tracker_sparql_connection_update_statement (connection,
			                                                   query,
			                                                   NULL,
			                                                   &error);

			if (stmt) {
				if (!bind_arguments (stmt, args)) {
					retval = EXIT_FAILURE;
					goto out;
				}

				tracker_sparql_statement_update (stmt, NULL, &error);
			}

			if (error) {
				g_printerr ("%s, %s\n",
				            _("Could not run update"),
				            error->message);
				g_error_free (error);
				retval = EXIT_FAILURE;
				goto out;
			}

			g_print ("%s\n", _("Done"));
		} else {
			stmt = tracker_sparql_connection_query_statement (connection,
			                                                  query,
			                                                  NULL,
			                                                  &error);

			if (stmt) {
				if (!bind_arguments (stmt, args)) {
					retval = EXIT_FAILURE;
					goto out;
				}

				cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
			}

			if (error) {
				g_printerr ("%s, %s\n",
				            _("Could not run query"),
				            error->message);
				g_error_free (error);

				retval = EXIT_FAILURE;
				goto out;
			}

			if (G_UNLIKELY (strstr (query, "fts:offsets")))
				print_cursor_with_ftsoffsets (cursor, _("No results found matching your query"), _("Results"), FALSE);
			else
				print_cursor (cursor, _("No results found matching your query"), _("Results"), FALSE);
		}
	}

	g_object_unref (connection);

out:
	tracker_term_pager_close ();

	return retval;
}

static int
sparql_run_default (void)
{
	GOptionContext *context;
	gchar *help;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);
	help = g_option_context_get_help (context, FALSE, NULL);
	g_option_context_free (context);
	g_printerr ("%s\n", help);
	g_free (help);

	return EXIT_FAILURE;
}

static gboolean
sparql_options_enabled (void)
{
	return SPARQL_OPTIONS_ENABLED ();
}

int
main (int argc, const char **argv)
{
	GOptionContext *context;
	GError *error = NULL;
	const gchar *failed;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tracker sparql";

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	if (file && query) {
		failed = _("File and query can not be used together");
	} else if (list_properties && list_properties[0] == '\0' && !tree) {
		failed = _("The --list-properties argument can only be empty when used with the --tree argument");
	} else {
		failed = NULL;
	}

	if (failed) {
		g_printerr ("%s\n\n", failed);
		return EXIT_FAILURE;
	}

	if (sparql_options_enabled ()) {
		return sparql_run ();
	}

	return sparql_run_default ();
}
