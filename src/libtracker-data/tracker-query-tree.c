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

/* These defines are required to allow lrintf() without warnings when
 * building.
 */
#define _ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#define __USE_ISOC9X	1
#define __USE_ISOC99	1

#include <string.h>
#include <math.h>

#include <glib-object.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-index-item.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-interface.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-query-tree.h"

#define TRACKER_QUERY_TREE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_QUERY_TREE, TrackerQueryTreePrivate))

#define MAX_HIT_BUFFER 480000
#define SCORE_MULTIPLIER 100000

typedef enum   OperationType OperationType;
typedef enum   TreeNodeType TreeNodeType;
typedef struct TreeNode TreeNode;
typedef struct TrackerQueryTreePrivate TrackerQueryTreePrivate;
typedef struct ComposeHitsData ComposeHitsData;
typedef struct SearchHitData SearchHitData;

enum OperationType {
	OP_NONE,
	OP_AND,
	OP_OR
};

struct TreeNode {
	TreeNode      *left;
	TreeNode      *right;
	OperationType  op;
	gchar	      *term;
};

struct TrackerQueryTreePrivate {
	gchar		*query_str;
	TreeNode	*tree;
	TrackerConfig	*config;
	TrackerLanguage *language;
};

struct ComposeHitsData {
	OperationType  op;
	GHashTable    *other_table;
	GHashTable    *dest_table;
};

struct SearchHitData {
	guint32 score;
};

enum {
	PROP_0,
	PROP_QUERY
};

static void tracker_query_tree_finalize     (GObject		   *object);
static void tracker_query_tree_set_property (GObject		   *object,
					     guint		    prop_id,
					     const GValue	   *value,
					     GParamSpec		   *pspec);
static void tracker_query_tree_get_property (GObject		   *object,
					     guint		    prop_id,
					     GValue		   *value,
					     GParamSpec		   *pspec);

G_DEFINE_TYPE (TrackerQueryTree, tracker_query_tree, G_TYPE_OBJECT)

static void
tracker_query_tree_class_init (TrackerQueryTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_query_tree_finalize;
	object_class->set_property = tracker_query_tree_set_property;
	object_class->get_property = tracker_query_tree_get_property;

	g_object_class_install_property (object_class,
					 PROP_QUERY,
					 g_param_spec_string ("query",
							      "Query",
							      "Query",
							      NULL,
							      G_PARAM_READWRITE));
	g_type_class_add_private (object_class,
				  sizeof (TrackerQueryTreePrivate));
}

static void
tracker_query_tree_init (TrackerQueryTree *tree)
{
}

static TreeNode *
tree_node_leaf_new (const gchar *term)
{
	TreeNode *node;

	node = g_slice_new0 (TreeNode);
	node->term = g_strdup (term);
	node->op = OP_NONE;

	return node;
}

static TreeNode *
tree_node_operator_new (OperationType op)
{
	TreeNode *node;

	node = g_slice_new0 (TreeNode);
	node->op = op;

	return node;
}

static void
tree_node_free (TreeNode *node)
{
	if (!node)
		return;

	/* Free string if any */
	g_free (node->term);

	/* free subnodes */
	tree_node_free (node->left);
	tree_node_free (node->right);

	g_slice_free (TreeNode, node);
}

static void
tracker_query_tree_finalize (GObject *object)
{
	TrackerQueryTreePrivate *priv;

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (object);

	tree_node_free (priv->tree);
	g_free (priv->query_str);

	G_OBJECT_CLASS (tracker_query_tree_parent_class)->finalize (object);
}

static void
tracker_query_tree_set_property (GObject      *object,
				 guint	       prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_QUERY:
		tracker_query_tree_set_query (TRACKER_QUERY_TREE (object),
					      g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_query_tree_get_property (GObject      *object,
				 guint	       prop_id,
				 GValue       *value,
				 GParamSpec   *pspec)
{
	TrackerQueryTreePrivate *priv;

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_QUERY:
		g_value_set_string (value, priv->query_str);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

TrackerQueryTree *
tracker_query_tree_new (const gchar	*query_str,
			TrackerConfig	*config,
			TrackerLanguage *language)
{
	TrackerQueryTree	*object;
	TrackerQueryTreePrivate *priv;

	g_return_val_if_fail (query_str != NULL, NULL);
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);
	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	/* NOTE: The "query" has to come AFTER the "config" and
	 * "language" properties since setting the query actually
	 * uses the priv->config and priv->language settings.
	 * Changing this order results in warnings.
	 */

	object = g_object_new (TRACKER_TYPE_QUERY_TREE, NULL);

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (object);

	priv->config = g_object_ref (config);
	priv->language = g_object_ref (language);

	tracker_query_tree_set_query (object, query_str);

	return object;
}

#if 0
static void
print_tree (TreeNode *node)
{
	if (!node) {
		g_print ("NULL ");
		return;
	}

	switch (node->op) {
	case OP_AND:
		g_print ("AND ");
		print_tree (node->left);
		print_tree (node->right);
		break;
	case OP_OR:
		g_print ("OR ");
		print_tree (node->left);
		print_tree (node->right);
		break;
	default:
		g_print ("%s ", node->term);
	}
}
#endif

static void
create_query_tree (TrackerQueryTree *tree)
{
	TrackerQueryTreePrivate *priv;
	TreeNode *node, *stack_node;
	GQueue *queue, *stack;
	gboolean last_element_is_term = FALSE;
	gchar *parsed_str, **strings;
	gint i;

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (tree);

	strings = g_strsplit (priv->query_str, " ", -1);
	queue = g_queue_new ();
	stack = g_queue_new ();

	/* Create a parse tree that recognizes queries such as:
	 * "foo"
	 * "foo and bar"
	 * "foo or bar"
	 * "foo and bar or baz"
	 * "foo or bar and baz"
	 *
	 * The operator "and" will have higher priority than the operator "or",
	 * and will also be assumed to exist if there is no operator between two
	 * search terms.
	 */

	/* Step 1. Create polish notation for the search term, the
	 * stack will be used to store operators temporarily
	 */
	for (i = 0; strings[i]; i++) {
		OperationType op;

		if (!strings[i] || !*strings[i])
			continue;

		/* get operator type */
		if (strcmp (strings[i], "and") == 0) {
			op = OP_AND;
		} else if (strcmp (strings [i], "or") == 0) {
			op = OP_OR;
		} else {
			if (last_element_is_term) {
				/* last element was a search term, assume the "and"
				 * operator between these two elements, and wait
				 * for actually parsing the second term */
				op = OP_AND;
				i--;
			} else {
				op = OP_NONE;
			}
		}

		/* create node for the operation type */
		switch (op) {
		case OP_AND:
			node = tree_node_operator_new (OP_AND);
			stack_node = g_queue_peek_head (stack);

			/* push in the queue operators with fewer priority, "or" in this case */
			while (stack_node && stack_node->op == OP_OR) {
				stack_node = g_queue_pop_head (stack);
				g_queue_push_head (queue, stack_node);

				stack_node = g_queue_peek_head (stack);
			}

			g_queue_push_head (stack, node);
			last_element_is_term = FALSE;
			break;
		case OP_OR:
			node = tree_node_operator_new (OP_OR);
			g_queue_push_head (stack, node);
			last_element_is_term = FALSE;
			break;
		default:
		case OP_NONE:
			/* search term */
			parsed_str = tracker_parser_text_to_string (strings[i],
								    priv->language,
								    tracker_config_get_max_word_length (priv->config),
								    tracker_config_get_min_word_length (priv->config),
								    TRUE,
								    FALSE,
								    FALSE);
			node = tree_node_leaf_new (g_strstrip (parsed_str));
			g_queue_push_head (queue, node);
			last_element_is_term = TRUE;

			g_free (parsed_str);
		}
	}

	/* we've finished parsing, queue all remaining operators in the stack */
	while ((stack_node = g_queue_pop_head (stack)) != NULL) {
		g_queue_push_head (queue, stack_node);
	}

	/* step 2: run through the reverse polish notation and connect nodes to
	 * create a tree, the stack will be used to store temporarily nodes
	 * until they're connected to a parent node */
	while ((node = g_queue_pop_tail (queue)) != NULL) {
		switch (node->op) {
		case OP_AND:
		case OP_OR:
			node->left = g_queue_pop_head (stack);
			node->right = g_queue_pop_head (stack);
			g_queue_push_head (stack, node);
			break;
		default:
		case OP_NONE:
			g_queue_push_head (stack, node);
			break;
		}

		priv->tree = node;
	}

	g_strfreev (strings);
	g_queue_free (stack);
	g_queue_free (queue);
}

void
tracker_query_tree_set_query (TrackerQueryTree *tree,
			      const gchar      *query_str)
{
	TrackerQueryTreePrivate *priv;
	gchar *str;

	g_return_if_fail (TRACKER_IS_QUERY_TREE (tree));
	g_return_if_fail (query_str != NULL);

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (tree);

	str = g_strdup (query_str);
	g_free (priv->query_str);
	priv->query_str = str;

	/* construct the parse tree */
	create_query_tree (tree);

	g_object_notify (G_OBJECT (tree), "query");
}

G_CONST_RETURN gchar *
tracker_query_tree_get_query (TrackerQueryTree *tree)
{
	TrackerQueryTreePrivate *priv;

	g_return_val_if_fail (TRACKER_IS_QUERY_TREE (tree), NULL);

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (tree);
	return priv->query_str;
}

static void
get_tree_words (TreeNode *node, GSList **list)
{
	if (!node)
		return;

	if (node->op == OP_NONE)
		*list = g_slist_prepend (*list, node->term);
	else {
		get_tree_words (node->left, list);
		get_tree_words (node->right, list);
	}
}

static gint
get_idf_score (TrackerDBIndexItem *details,
	       gfloat		   idf)
{
	guint32 score;
	gfloat	f;

	score = tracker_db_index_item_get_score (details);
	f = idf * score * SCORE_MULTIPLIER;

	return (f > 1.0) ? lrintf (f) : 1;
}

static void
search_hit_data_free (gpointer data)
{
	g_slice_free (SearchHitData, data);
}

static void
add_search_term_hits_to_hash_table (TrackerDBIndexItem *items,
				    guint		item_count,
				    GHashTable	       *result)
{
	guint i;

	for (i = 0; i < item_count; i++) {
		SearchHitData *data;

		data = g_slice_new (SearchHitData);
		data->score = get_idf_score (&items[i], (float) 1 / item_count);

		g_hash_table_insert (result, GINT_TO_POINTER (items[i].id), data);
	}
}

static GHashTable *
get_search_term_hits (TrackerQueryTree *tree,
		      const gchar      *term)
{
	TrackerQueryTreePrivate *priv;
	TrackerDBIndex *index;
	TrackerDBIndexItem	*items;
	guint			 item_count;
	GHashTable		*result;

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (tree);

	result = g_hash_table_new_full (NULL,
					NULL,
					NULL,
					search_hit_data_free);

	index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_RESOURCES);

	items = tracker_db_index_get_word_hits (index, term, &item_count);

	if (items) {
		add_search_term_hits_to_hash_table (items,
						    item_count,
						    result);
		g_free (items);
	}

	return result;
}

static void
compose_hits_foreach (gpointer key,
		      gpointer value,
		      gpointer user_data)
{
	SearchHitData	*hit1, *hit2, *hit;
	ComposeHitsData *data;

	data = (ComposeHitsData *) user_data;
	hit1 = (SearchHitData *) value;
	hit2 = g_hash_table_lookup (data->other_table, key);

	if (data->op == OP_OR) {
		/* compose both scores in the same entry */
		hit = g_slice_dup (SearchHitData, hit1);

		if (hit2)
			hit->score += hit2->score;

		g_hash_table_insert (data->dest_table, key, hit);
	} else if (data->op == OP_AND) {
		/* only compose if the key is in both tables */
		if (hit2) {
			hit = g_slice_dup (SearchHitData, hit1);
			hit->score += hit2->score;
			g_hash_table_insert (data->dest_table, key, hit);
		}
	} else {
		g_assert_not_reached ();
	}
}

static GHashTable *
compose_hits (OperationType  op,
	      GHashTable    *left_table,
	      GHashTable    *right_table)
{
	ComposeHitsData data;
	GHashTable *foreach_table;

	data.op = op;

	/* try to run the foreach in the table with less hits */
	if (g_hash_table_size (left_table) < g_hash_table_size (right_table)) {
		foreach_table = left_table;
		data.other_table = right_table;
	} else {
		foreach_table = right_table;
		data.other_table = left_table;
	}

	if (op == OP_OR) {
		data.dest_table = g_hash_table_ref (data.other_table);
	} else {
		data.dest_table = g_hash_table_new_full (NULL,
							 NULL,
							 NULL,
							 search_hit_data_free);
	}

	g_hash_table_foreach (foreach_table, (GHFunc) compose_hits_foreach, &data);

	return data.dest_table;
}

static GHashTable *
get_node_hits (TrackerQueryTree *tree,
	       TreeNode		*node)
{
	GHashTable *left_table, *right_table, *result;

	if (!node)
		return NULL;

	switch (node->op) {
	case OP_NONE:
		result = get_search_term_hits (tree, node->term);
		break;
	case OP_AND:
	case OP_OR:
		left_table = get_node_hits (tree, node->left);
		right_table = get_node_hits (tree, node->right);
		result = compose_hits (node->op, left_table, right_table);

		g_hash_table_unref (left_table);
		g_hash_table_unref (right_table);
		break;
	default:
		g_assert_not_reached ();
	}

	return result;
}

static void
get_hits_foreach (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
	GArray *array;
	TrackerDBIndexItemRank rank;
	SearchHitData *hit_data;

	array = (GArray *) user_data;
	hit_data = (SearchHitData *) value;

	rank.service_id = GPOINTER_TO_UINT (key);
	rank.score = hit_data->score;

	g_array_append_val (array, rank);
}

static gint
compare_search_hits (gconstpointer a,
		     gconstpointer b)
{
	TrackerDBIndexItemRank *ap, *bp;

	ap = (TrackerDBIndexItemRank *) a;
	bp = (TrackerDBIndexItemRank *) b;

	return (bp->score - ap->score);
}

GSList *
tracker_query_tree_get_words (TrackerQueryTree *tree)
{
	TrackerQueryTreePrivate *priv;
	GSList *list = NULL;

	g_return_val_if_fail (TRACKER_IS_QUERY_TREE (tree), NULL);

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (tree);
	get_tree_words (priv->tree, &list);

	return list;
}

GArray *
tracker_query_tree_get_hits (TrackerQueryTree *tree,
			     guint	       offset,
			     guint	       limit)
{
	TrackerQueryTreePrivate *priv;
	GHashTable *table;
	GArray *array;

	g_return_val_if_fail (TRACKER_IS_QUERY_TREE (tree), NULL);

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (tree);

	g_return_val_if_fail (priv->tree != NULL, NULL);

	table = get_node_hits (tree, priv->tree);
	array = g_array_sized_new (TRUE, TRUE, sizeof (TrackerDBIndexItemRank),
				   g_hash_table_size (table));

	g_hash_table_foreach (table, (GHFunc) get_hits_foreach, array);
	g_array_sort (array, compare_search_hits);

	if (offset > 0) {
		g_array_remove_range (array, 0, CLAMP (offset, 0, array->len));
	}

	if (limit > 0 && limit < array->len) {
		g_array_remove_range (array, limit, array->len - limit);
	}

	g_hash_table_destroy (table);

	return array;
}

gint
tracker_query_tree_get_hit_count (TrackerQueryTree *tree)
{
	TrackerQueryTreePrivate *priv;
	GHashTable *table;
	gint count;

	g_return_val_if_fail (TRACKER_IS_QUERY_TREE (tree), 0);

	priv = TRACKER_QUERY_TREE_GET_PRIVATE (tree);
	table = get_node_hits (tree, priv->tree);

	if (!table)
		return 0;

	count = g_hash_table_size (table);
	g_hash_table_destroy (table);

	return count;
}

GArray *
tracker_query_tree_get_hit_counts (TrackerQueryTree *tree)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	GArray *hits, *counts;
	guint i;
	GString *sql;

	g_return_val_if_fail (TRACKER_IS_QUERY_TREE (tree), NULL);

	iface = tracker_db_manager_get_db_interface ();

	hits = tracker_query_tree_get_hits (tree, 0, 0);
	counts = g_array_sized_new (TRUE, TRUE, sizeof (TrackerHitCount), 10);

	/* count RDF types for each text search result */
	sql = g_string_new ("SELECT ");
	g_string_append (sql, "(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdf:type\"), COUNT(*) ");
	g_string_append (sql, "FROM (SELECT * FROM \"rdfs:Resource_rdf:type\" ");
	g_string_append (sql, "WHERE ID IN (");

	for (i = 0; i < hits->len; i++) {
		TrackerDBIndexItemRank rank;

		rank = g_array_index (hits, TrackerDBIndexItemRank, i);

		if (i > 0) {
			g_string_append (sql, ",");
		}
		g_string_append_printf (sql, "%d", rank.service_id);
	}

	g_string_append (sql, ")) GROUP BY \"rdf:type\"");

	result_set = tracker_db_interface_execute_query (iface, NULL, "%s", sql->str);

	if (result_set) {
		gboolean valid = TRUE;
		while (valid) {
			TrackerHitCount count;
			TrackerClass *class;
			gchar *class_uri;

			tracker_db_result_set_get (result_set, 0, &class_uri, 1, &count.count, -1);

			class = tracker_ontology_get_class_by_uri (class_uri);
			count.class = class;

			g_array_append_val (counts, count);

			g_free (class_uri);

			valid = tracker_db_result_set_iter_next (result_set);
		}
		g_object_unref (result_set);
	}

	g_array_free (hits, TRUE);
	g_string_free (sql, TRUE);

	return counts;
}
