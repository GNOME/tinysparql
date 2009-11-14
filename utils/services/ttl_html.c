#include "ttl_html.h"
#include <glib/gprintf.h>
#include "qname.h"

typedef struct {
        Ontology *ontology;
        OntologyDescription *description;
        FILE *output;
} CallbackInfo;


static void
print_author (gpointer item, gpointer user_data) {
        FILE *f = (FILE *)user_data;
        g_fprintf (f,"<dd>%s</dd>", (gchar *)item);
}

static void
print_reference (gpointer item, gpointer user_data)
{
        gchar *shortname;
        gchar *link;
        FILE *f = (FILE *)user_data;

        shortname = qname_to_shortname ((gchar *)item);
        link = qname_to_link ((gchar *)item);

        g_fprintf (f,"<a href=\"%s\">%s</a>, ", link , shortname);

        g_free (shortname);
        g_free (link);
}



static void
print_references_list (FILE *f, GList *list)
{
        g_fprintf (f,"<td>");
        if (list == NULL) {
                g_fprintf (f,"--");
        } else {
                g_list_foreach (list, print_reference, f);
        }
        g_fprintf (f,"</td>");
}


static void
print_html_header (FILE *f, OntologyDescription *desc)
{
        g_fprintf (f,"<html>\n");
        g_fprintf (f,"<head>\n");
        g_fprintf (f,"\t<link rel=\"stylesheet\" type=\"text/css\"");
        g_fprintf (f," href=\"../resources/nie-maemo.css\" />\n");
        g_fprintf (f,"<title>%s</title>\n", desc->title);
        g_fprintf (f,"</head>\n");
        g_fprintf (f,"<body>\n");
        g_fprintf (f,"<div class=\"head\">\n");
        g_fprintf (f," <div class=\"nav\">\n");
        g_fprintf (f," <a href=\"http://www.maemo.org\"> <img alt=\"MAEMO logo\" ");
        g_fprintf (f," src=\"../resources/maemo-logo.gif\" /></a>\n");
        g_fprintf (f,"</div>\n");
// style="border: 0px solid ; width: 180px; height: 88px;"  /> </a> </div>

        g_fprintf (f,"<h1>%s</h1>\n", desc->title);
        g_fprintf (f," <dl>\n");
        g_fprintf (f,"  <dt>Latest Version</dt><dd>FIXME</dd>\n");
        g_fprintf (f,"  <dt></dt>\n");
        g_fprintf (f,"  <dt>This Version</dt><dd>FIXME</dd>\n");
        g_fprintf (f,"  <dt></dt>\n");
        g_fprintf (f,"  <dt>Authors:</dt>\n");
        g_list_foreach (desc->authors, print_author, f);
        g_fprintf (f,"  <dt>Editors:</dt>\n");
        g_list_foreach (desc->editors, print_author, f);
        g_fprintf (f,"  <dt>Contributors:</dt>\n");
        g_list_foreach (desc->contributors, print_author, f);
        g_fprintf (f,"  <dt>Changelog:</dt>\n");
        g_fprintf (f,"  <dd><a href=\"%s\">Tracker changes</a>", 
                   (desc->gitlog ? desc->gitlog : "#"));
        g_fprintf (f," </dl>\n");
        g_fprintf (f,"</div>\n");
        g_fprintf (f,"<p class=\"copyright\"> Copyright &copy; 2009 <a href=\"http://www.nokia.com/\">Nokia</a><sup>&reg;</sup> The ontologies are made available under the terms of FIXME<a href=\"LICENSE.txt\">software license</a></p>\n");

        g_fprintf (f,"<hr />\n");


}

static void
print_html_footer (FILE *f)
{
        g_fprintf (f,"</body>\n");
        g_fprintf (f,"</html>\n");
}


static void
print_ontology_class (gpointer key, gpointer value, gpointer user_data) 
{
        OntologyClass *def = (OntologyClass *)value;
        gchar *name, *anchor;
        FILE *f = (FILE *)user_data;

        g_return_if_fail (f != NULL);

        name = qname_to_shortname (def->classname);
        anchor = qname_to_link (def->classname);
        
        /* Anchor without initial '#' */
        g_fprintf (f,"<a name=\"%s\">\n", &anchor[1]); 
        g_free (anchor);

        g_fprintf (f,"<h3>%s</h3>\n", name);
        g_free (name);

        g_fprintf (f,"<table class=\"doctable\">\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Superclasses</td>");
        print_references_list (f, def->superclasses);
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Subclasses</td>");
        print_references_list (f, def->subclasses);
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">In domain of</td>");
        print_references_list (f, def->in_domain_of);
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">In range of</td>");
        print_references_list (f, def->in_range_of);
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Description</td>");
        g_fprintf (f,"<td>%s</td>\n", (def->description ? def->description : "--"));
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"</table>\n\n");

}

static void
print_ontology_property (gpointer key, gpointer value, gpointer user_data) 
{
        OntologyProperty *def = (OntologyProperty *)value;
        gchar *name, *anchor;
        FILE *f = (FILE *)user_data;

        g_return_if_fail (f != NULL);

        name = qname_to_shortname (def->propertyname);
        anchor = qname_to_link (def->propertyname);
        
        /* Anchor without initial '#' */
        g_fprintf (f,"<a name=\"%s\">", &anchor[1]); 
        g_free (anchor);

        g_fprintf (f,"<h3>%s</h3>\n", name);
        g_free (name);
        g_fprintf (f,"<table class=\"doctable\">\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Type</td>");
        print_references_list (f, def->type);
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Domain</td>");
        print_references_list (f, def->domain);
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Range</td>");
        print_references_list (f, def->range);
        g_fprintf (f,"</tr>\n");
        
        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Superproperties</td>");
        print_references_list (f, def->superproperties);
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Subproperties</td>");
        print_references_list (f, def->subproperties);
        g_fprintf (f,"</tr>\n");

        if (def->max_cardinality) {
                g_fprintf (f,"<tr>");
                g_fprintf (f,"<td class=\"rowheader\">Maximal cardinality</td>");
                g_fprintf (f,"<td>%s</td", def->max_cardinality);
                g_fprintf (f,"</tr>\n");
        }

        g_fprintf (f,"<tr>");
        g_fprintf (f,"<td class=\"rowheader\">Description</td>");
        g_fprintf (f,"<td>%s</td>\n", (def->description ? def->description : "--"));
        g_fprintf (f,"</tr>\n");

        g_fprintf (f,"</table>\n\n");
}



void 
ttl_html_print (OntologyDescription *description,
                Ontology *ontology,
                FILE *f,
                const gchar *class_location_file)
{

        qname_init (description->baseUrl, description->localPrefix, class_location_file);
        print_html_header (f, description);
        g_fprintf (f,"<h2>Ontology Classes Descriptions</h2>");
        g_hash_table_foreach (ontology->classes, print_ontology_class, f);
        g_fprintf (f,"<h2>Ontology Properties Descriptions</h2>");
        g_hash_table_foreach (ontology->properties, print_ontology_property, f);

        print_html_footer (f);
}
