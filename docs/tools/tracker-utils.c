/*
 * Copyright (C) 2015, Carlos Garnacho
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 */

#include <glib-object.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include <stdlib.h>

#include "tracker-utils.h"

enum {
	INSERT_BEFORE,
	INSERT_AFTER
};

static void
class_get_parent_hierarchy (TrackerOntologyModel  *model,
                            const gchar           *class_name,
                            GList                **list)
{
	TrackerOntologyClass *klass;
	GList *l, *elem;

	/* Ensure we only got the same class there once */
	elem = g_list_find_custom (*list, class_name,
	                           (GCompareFunc) g_strcmp0);
	if (elem)
		*list = g_list_delete_link (*list, elem);
	*list = g_list_prepend (*list, (gpointer) class_name);

	klass = tracker_ontology_model_get_class (model, class_name);
	if (!klass)
		return;


	for (l = klass->superclasses; l; l = l->next) {
		class_get_parent_hierarchy (model, l->data, list);
	}
}

static GList *
class_get_hierarchy (TrackerOntologyModel *model,
                     TrackerOntologyClass *klass)
{
	GList *hierarchy = NULL, *l;

	/* Order is: parents, this class, and children. Guaranteed
	 * to be in an order where all parents are before a child.
	 * We first prepend all children, and then find out the
	 * upwards hierarchy recursively.
	 */
	for (l = klass->subclasses; l; l = l->next) {
		hierarchy = g_list_prepend (hierarchy, l->data);
	}

	class_get_parent_hierarchy (model, klass->classname, &hierarchy);

	return hierarchy;
}

typedef struct {
	TrackerOntologyClass *class;
	GList *hierarchy;
	GHashTable *resolved_children;
	GHashTable *resolved_parents;
	GHashTable *placed;
	GPtrArray *strings;
} HierarchyContext;

static HierarchyString *
hierarchy_string_new (void)
{
	HierarchyString *str;

	str = g_new0 (HierarchyString, 1);
	str->before = g_string_new ("");
	str->after = g_string_new ("");
	str->link_label = g_string_new ("");

	return str;
}

static void
hierarchy_string_free (HierarchyString *str)
{
	g_string_free (str->before, TRUE);
	g_string_free (str->after, TRUE);
	g_string_free (str->link_label, TRUE);
	g_free (str);
}

static void
hierarchy_string_append (HierarchyString *str,
                         int              pos,
                         const gchar     *substr)
{
	if (pos == INSERT_BEFORE)
		g_string_append (str->before, substr);
	else
		g_string_append (str->after, substr);

	str->visible_len += g_utf8_strlen (substr, -1);
}

static void
hierarchy_string_append_link (HierarchyString      *str,
                              TrackerOntologyClass *klass)
{
	g_string_append (str->link_label, klass->shortname);
	str->class = klass;
	str->visible_len += g_utf8_strlen (klass->shortname, -1);
}

static GList *
list_filter (GList *original,
             GList *valid)
{
	GList *l, *filtered = NULL;

	for (l = original; l; l = l->next) {
		if (!g_list_find_custom (valid, l->data,
		                         (GCompareFunc) g_strcmp0))
			continue;

		filtered = g_list_prepend (filtered, l->data);
	}

	return filtered;
}

static HierarchyContext *
hierarchy_context_new (TrackerOntologyClass *klass,
                       TrackerOntologyModel *model)
{
	HierarchyContext *context;
	GList *l;

	context = g_new0 (HierarchyContext, 1);
	context->class = klass;
	context->hierarchy = class_get_hierarchy (model, klass);
	context->placed = g_hash_table_new (g_str_hash, g_str_equal);
	context->resolved_parents = g_hash_table_new_full (g_str_hash,
	                                                   g_str_equal,
	                                                   NULL,
	                                                   (GDestroyNotify) g_list_free);
	context->resolved_children = g_hash_table_new_full (g_str_hash,
	                                                    g_str_equal,
	                                                    NULL,
	                                                    (GDestroyNotify) g_list_free);
	context->strings = g_ptr_array_new_with_free_func ((GDestroyNotify) hierarchy_string_free);

	for (l = context->hierarchy; l; l = l->next) {
		TrackerOntologyClass *cl = tracker_ontology_model_get_class (model, l->data);

		g_hash_table_insert (context->resolved_parents,
		                     cl->classname,
		                     list_filter (cl->superclasses,
		                                  context->hierarchy));
		g_hash_table_insert (context->resolved_children,
		                     cl->classname,
		                     list_filter (cl->subclasses,
		                                  context->hierarchy));

		g_ptr_array_add (context->strings, hierarchy_string_new ());
	}

	return context;
}

static void
hierarchy_context_free (HierarchyContext *context)
{
	g_list_free (context->hierarchy);
	g_ptr_array_unref (context->strings);
	g_hash_table_unref (context->placed);
	g_hash_table_unref (context->resolved_parents);
	g_hash_table_unref (context->resolved_children);
	g_free (context);
}

static GList *
hierarchy_context_get_single_parented_children (HierarchyContext     *context,
                                                TrackerOntologyClass *klass,
                                                TrackerOntologyModel *model)
{
	GList *filtered = NULL, *children, *l;

	children = g_hash_table_lookup (context->resolved_children,
	                                klass->classname);

	for (l = children; l; l = l->next) {
		GList *parents;

		parents = g_hash_table_lookup (context->resolved_parents, l->data);

		if (g_list_length (parents) == 1)
			filtered = g_list_prepend (filtered, l->data);
	}

	return filtered;
}

static void
fill_padding (HierarchyString *str,
              gint             pos,
              gint             max_len,
              gchar           *substr)
{
	gint padding = max_len - str->visible_len;

	if (padding <= 0)
		return;

	while (padding > 0) {
		hierarchy_string_append (str, pos, substr);
		padding--;
	}
}

static gboolean
check_parents_placed (GList      *parents,
                      GHashTable *ht)
{
	GList *l;

	for (l = parents; l; l = l->next) {
		if (!g_hash_table_lookup (ht, l->data))
			return FALSE;
	}

	return TRUE;
}

static void
hierarchy_context_resolve_class (HierarchyContext     *context,
                                 TrackerOntologyClass *klass,
                                 TrackerOntologyModel *model)
{
	GList *l = g_list_find_custom (context->hierarchy, klass->classname,
				       (GCompareFunc) g_strcmp0);
	gint pos = g_list_position (context->hierarchy, l);
	GList *children, *parents;
	HierarchyString *str;
	gboolean is_child;

	if (pos < 0)
		return;

	parents = g_hash_table_lookup (context->resolved_parents,
	                               klass->classname);

	if (g_list_length (parents) > 1) {
		gboolean first_pending = TRUE;
		gint i, max_len = 0;

		/* This class has more than one parent in the current graph,
		 * those paint the lines on the right side.
		 */

		/* Step 1: Find the longest string */
		first_pending = TRUE;
		for (i = 0, l = context->hierarchy; i < pos && l; i++, l = l->next) {
			is_child = g_list_find_custom (klass->superclasses, l->data,
			                               (GCompareFunc) g_strcmp0) != NULL;
			if (!is_child && first_pending)
				continue;

			str = g_ptr_array_index (context->strings, i);
			max_len = MAX (max_len, str->visible_len);
		}

		/* Step 2: append the line art, we must fill in some padding if
		 * necessary.
		 */
		first_pending = TRUE;
		for (i = 0, l = context->hierarchy; i < pos && l; i++, l = l->next) {
			is_child = g_list_find_custom (klass->superclasses, l->data,
			                               (GCompareFunc) g_strcmp0) != NULL;

			if (!is_child && first_pending)
				continue;

			str = g_ptr_array_index (context->strings, i);

			fill_padding (str, INSERT_AFTER, max_len, is_child ? "─" : " ");

			if (first_pending) {
				hierarchy_string_append (str, INSERT_AFTER, "──┐");
				first_pending = FALSE;
			} else if (is_child) {
				hierarchy_string_append (str, INSERT_AFTER, "──┤");
			} else {
				hierarchy_string_append (str, INSERT_AFTER, "  │");
			}
		}

		/* Step 3: Finally, print the current class */
		str = g_ptr_array_index (context->strings, pos);
		fill_padding (str, INSERT_BEFORE, max_len - 1, " ");

		hierarchy_string_append (str, INSERT_BEFORE, "   └── ");
		hierarchy_string_append_link (str, klass);
		hierarchy_string_append (str, INSERT_AFTER, " ");
	} else {
		/* The current class has only 1 parent, lineart for those is
		 * displayed on the left side.
		 */

		/* Step 1: Print the current class */
		str = g_ptr_array_index (context->strings, pos);
		hierarchy_string_append_link (str, klass);
		hierarchy_string_append (str, INSERT_AFTER, " ");

		/* Step 2: Modify all strings downwards, adding the lineart
		 * necessary for all children of this class.
		 */
		children = hierarchy_context_get_single_parented_children (context, klass, model);
		l = l->next;
		pos++;

		for (; l; l = l->next, pos++) {
			guint len = g_list_length (children);
			GList *cur;

			str = g_ptr_array_index (context->strings, pos);
			cur = g_list_find_custom (children, l->data,
			                          (GCompareFunc) g_strcmp0);
			is_child = (cur != NULL);

			if (is_child) {
				if (len > 1)
					hierarchy_string_append (str, INSERT_BEFORE, "├── ");
				else if (len == 1)
					hierarchy_string_append (str, INSERT_BEFORE, "╰── ");

				children = g_list_delete_link (children, cur);
				g_hash_table_insert (context->placed,
				                     l->data, GINT_TO_POINTER (TRUE));
			} else {
				if (len > 0)
					hierarchy_string_append (str, INSERT_BEFORE, "│   ");
				else if (len == 0 &&
					 !g_hash_table_lookup (context->placed, l->data)) {
					GList *cl_parents;

					cl_parents = g_hash_table_lookup (context->resolved_parents, l->data);

					if (g_list_length (cl_parents) == 1 ||
					    !check_parents_placed (cl_parents, context->placed)) {
						hierarchy_string_append (str, INSERT_BEFORE, "    ");
					}
				}
			}
		}
	}
}

GPtrArray *
class_get_parent_hierarchy_strings (TrackerOntologyClass *klass,
                                    TrackerOntologyModel *model)
{
	HierarchyContext *context;
	GList *c;
	GPtrArray *strings = NULL;

	context = hierarchy_context_new (klass, model);

	/* Proceed from parent to child classes, populating the
	 * context->strings array.
	 */
	for (c = context->hierarchy; c; c = c->next) {
		TrackerOntologyClass *cl = tracker_ontology_model_get_class (model, c->data);
		hierarchy_context_resolve_class (context, cl, model);
	}

	strings = g_ptr_array_ref (context->strings);
	hierarchy_context_free (context);

	return strings;
}
