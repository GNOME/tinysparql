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

static gchar *
link_from_shortname (const gchar *shortname)
{
	const gchar *sep;
	gchar *link, *prefix;

	sep = strstr (shortname, ":");
	g_assert (sep != NULL);

	prefix = g_strndup (shortname, sep - shortname);
	link = g_strdup_printf ("%s-ontology.html#%s", prefix, shortname);
	g_free (prefix);

	return link;
}

static void
describe_class (FILE                 *f,
                TrackerOntologyModel *model,
                TrackerOntologyClass *klass,
                gboolean              link)
{
	g_fprintf (f, "node [shape=\"box\", style=\"rounded\", border=\"0\", fontname=\"sans-serif\"");

	if (link) {
		g_autofree gchar *link = NULL;

		link = link_from_shortname (klass->shortname);
		g_fprintf (f, ", class=\"link\", href=\"%s\"", link);
	}

	g_fprintf (f, "]; \"%s\"\n", klass->shortname);
}

static void
print_superclasses (FILE                 *f,
                    TrackerOntologyModel *model,
                    TrackerOntologyClass *klass,
                    GHashTable           *visited,
                    gboolean              link)
{
	GList *l;

	if (g_hash_table_contains (visited, klass))
	    return;

	describe_class (f, model, klass, link);

	for (l = klass->superclasses; l; l = l->next) {
		TrackerOntologyClass *superclass;

		superclass = tracker_ontology_model_get_class (model, l->data);
		print_superclasses (f, model, superclass, visited, TRUE);
		g_fprintf (f, "\"%s\" -- \"%s\"\n", superclass->shortname, klass->shortname);
	}

	g_hash_table_add (visited, klass);
}

static void
ttl_generate_class_hierarchy_dot (TrackerOntologyDescription *description,
                                  TrackerOntologyModel       *model,
                                  TrackerOntologyClass       *klass,
                                  GFile                      *output_location)
{
	gchar *path, *filename;
	GFile *file;
	g_autoptr(GHashTable) visited = NULL;
	FILE *f;
	GList *l;

	if (!klass->subclasses && !klass->superclasses)
		return;

	filename = g_strdup_printf ("%s-hierarchy.dot", klass->shortname);
	file = g_file_get_child (output_location, filename);
	g_free (filename);

	path = g_file_get_path (file);
	f = fopen (path, "w");
	g_assert (f != NULL);
	g_free (path);

	g_fprintf (f, "graph G {\n");
	g_fprintf (f, "bgcolor=\"transparent\";\n");
	g_fprintf (f, "rankdir=TB;\n");

	visited = g_hash_table_new (NULL, NULL);

	print_superclasses (f, model, klass, visited, FALSE);

	for (l = klass->subclasses; l; l = l->next) {
		TrackerOntologyClass *subclass;

		subclass = tracker_ontology_model_get_class (model, l->data);
		describe_class (f, model, subclass, TRUE);
		g_fprintf (f, "\"%s\" -- \"%s\"\n", klass->shortname, subclass->shortname);
	}

	g_fprintf (f, "}");

	g_object_unref (file);
	fclose (f);
}

static void
ttl_generate_rdf_diagram_dot (TrackerOntologyDescription *description,
                              TrackerOntologyModel       *model,
                              TrackerOntologyClass       *klass,
                              GFile                      *output_location)
{
	gchar *path, *filename;
	GFile *file;
	g_autoptr(GHashTable) visited = NULL;
	FILE *f;
	GList *l;

	if (!klass->subclasses && !klass->superclasses)
		return;

	filename = g_strdup_printf ("%s-diagram.dot", klass->shortname);
	file = g_file_get_child (output_location, filename);
	g_free (filename);

	path = g_file_get_path (file);
	f = fopen (path, "w");
	g_assert (f != NULL);
	g_free (path);

	g_fprintf (f, "digraph G {\n");
	g_fprintf (f, "bgcolor=\"transparent\";\n");
	g_fprintf (f, "rankdir=LR;\n");

	visited = g_hash_table_new (NULL, NULL);
	describe_class (f, model, klass, FALSE);

	for (l = klass->in_domain_of; l; l = l->next) {
		TrackerOntologyProperty *prop;
		TrackerOntologyClass *range;
		g_autofree gchar *link = NULL;

		prop = tracker_ontology_model_get_property (model, l->data);
		range = tracker_ontology_model_get_class (model, prop->range->data);

		if (g_hash_table_contains (visited, prop))
			continue;

		if (g_str_has_prefix (range->shortname, "xsd:") ||
		    g_str_equal (range->shortname, "rdfs:Literal") ||
		    g_str_equal (range->shortname, "rdf:langString"))
			continue;

		if (!g_hash_table_contains (visited, range)) {
			describe_class (f, model, range, TRUE);
			g_hash_table_add (visited, range);
		}

		link = link_from_shortname (prop->shortname);
		g_fprintf (f, "\"%s\" -> \"%s\" [label=\"%s\", fontname=\"sans-serif\", fontsize=10, href=\"%s\"]\n",
		           klass->shortname, range->shortname, prop->shortname,
		           link);
		g_hash_table_add (visited, prop);
	}

	for (l = klass->in_range_of; l; l = l->next) {
		TrackerOntologyProperty *prop;
		TrackerOntologyClass *domain;
		g_autofree gchar *link = NULL;

		prop = tracker_ontology_model_get_property (model, l->data);
		domain = tracker_ontology_model_get_class (model, prop->domain->data);

		if (g_hash_table_contains (visited, prop))
			continue;

		if (!g_hash_table_contains (visited, domain)) {
			describe_class (f, model, domain, TRUE);
			g_hash_table_add (visited, domain);
		}

		link = link_from_shortname (prop->shortname);
		g_fprintf (f, "\"%s\" -> \"%s\" [label=\"%s\", fontname=\"sans-serif\", fontsize=10, href=\"%s\"]\n",
		           domain->shortname, klass->shortname, prop->shortname,
		           link);
		g_hash_table_add (visited, prop);
	}

	g_fprintf (f, "}");

	g_object_unref (file);
	fclose (f);
}

void
ttl_generate_dot_files (TrackerOntologyDescription *description,
                        TrackerOntologyModel       *model,
                        const gchar                *prefix,
                        GFile                      *output_location)
{
	g_autoptr(GList) classes = NULL;
	GList *l;

	classes = tracker_ontology_model_list_classes (model, prefix);

	for (l = classes; l; l = l->next) {
		TrackerOntologyClass *klass;

		klass = tracker_ontology_model_get_class (model, l->data);

		ttl_generate_class_hierarchy_dot (description, model, klass, output_location);
		ttl_generate_rdf_diagram_dot (description, model, klass, output_location);
	}
}
