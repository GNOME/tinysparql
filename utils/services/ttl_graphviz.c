/*
 * Copyright (C) 2009, Nokia
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
 */

#include "ttl_graphviz.h"
#include <glib.h>
#include <glib/gprintf.h>
#include "qname.h"

static void
generate_class_nodes (gpointer key, gpointer value, gpointer user_data)
{
	FILE *output = (FILE *)user_data;
	OntologyClass *klass = (OntologyClass *)value;
	GList *it = NULL;

	g_fprintf (output, "\t \"%s\" [fillcolor=\"greenyellow\"];\n",
	           qname_to_shortname (klass->classname));

	for (it = klass->superclasses; it != NULL; it = it->next) {
		g_fprintf (output,
		           "\t \"%s\" -> \"%s\" [arrowhead=\"empty\"];\n",
		           qname_to_shortname (klass->classname),
		           qname_to_shortname (it->data));

	}
}

static void
generate_property_edges (gpointer key, gpointer value, gpointer user_data)
{
	FILE *output = (FILE *)user_data;
	OntologyProperty *prop = (OntologyProperty *)value;
	static gint counter = 0;

	g_assert (g_list_length (prop->domain) == 1);
	g_assert (g_list_length (prop->range) == 1);

	if (qname_is_basic_type (prop->range->data)) {
		/*
		  "_str_1" [label="_str_", shape="box"];
		  "nfo:Video" ->  "_str_1" [label="name"];
		*/
		g_fprintf (output,
		           "\t \"_str_%d\" [label=\"%s\", shape=\"box\", fontsize=\"8\", height=\"0.2\"];\n",
		           counter,
		           qname_to_shortname (prop->range->data));
		g_fprintf (output,
		           "\t \"%s\" -> \"_str_%d\" [label=\"%s\"];\n",
		           qname_to_shortname (prop->domain->data),
		           counter,
		           qname_to_shortname (prop->propertyname));
		counter += 1;

	} else {
		g_fprintf (output,
		           "\t \"%s\" -> \"%s\" [label=\"%s\"];\n",
		           qname_to_shortname (prop->domain->data),
		           qname_to_shortname (prop->range->data),
		           qname_to_shortname (prop->propertyname));
	}
}


void
ttl_graphviz_print (OntologyDescription *description,
                    Ontology *ontology,
                    FILE *output)
{
	qname_init (description->baseUrl, description->localPrefix, NULL);
	g_fprintf (output, "digraph \"%s\" {\n",  description->title);
	g_fprintf (output, "    label=\"%s\";\n", description->title);
	g_fprintf (output, "    rankdir=BT;\n");
	g_fprintf (output, "    node [style=filled];\n");

	g_hash_table_foreach (ontology->classes,
	                      generate_class_nodes,
	                      output);

	g_hash_table_foreach (ontology->properties,
	                      generate_property_edges,
	                      output);

	g_fprintf (output, "}\n");

}
