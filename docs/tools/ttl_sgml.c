#include <glib/gprintf.h>
#include <string.h>

#include "ttl_sgml.h"
#include "qname.h"

#define DEFAULT_COPYRIGHT "Copyright &copy; 2009 <a href=\"http://www.nokia.com/\">Nokia</a>"
#define SIGNALS_DOC "http://live.gnome.org/Tracker/Documentation/SignalsOnChanges"

typedef struct {
	Ontology *ontology;
	OntologyDescription *description;
	FILE *output;
} CallbackInfo;


static void
print_someone (FILE *f,
               const gchar *role,
               const gchar *who)
{
        gchar **details;

        details = g_strsplit (who, ",", 3);

        g_fprintf (f, "<%s>\n", role);
        g_fprintf (f, "<firstname>%s</firstname>\n", g_strstrip (details[0]));

        if (details[1] || details[2]) {
                g_fprintf (f, "<affiliation>\n");

                if (details[1]) {
                        g_fprintf (f, "<orgname>%s</orgname>\n", g_strstrip (details[1]));
                }

                if (details[1] && details[2]) {
                        g_fprintf (f, "<address><email>%s</email></address>\n", g_strstrip (details[2]));
                }

                g_fprintf (f, "</affiliation>\n");
        }

        g_fprintf (f, "</%s>\n", role);
        g_strfreev (details);
}

static void
print_author (gpointer item, gpointer user_data) {
	FILE *f = (FILE *)user_data;
        print_someone (f, "author", (gchar *) item);
}

static void
print_editor (gpointer item, gpointer user_data) {
	FILE *f = (FILE *)user_data;
        print_someone (f, "editor", (gchar *) item);
}

static void
print_collab (gpointer item, gpointer user_data) {
	FILE *f = (FILE *)user_data;
        print_someone (f, "collab", (gchar *) item);
}

static gchar *
shortname_to_id (const gchar *name)
{
        gchar *id, *p;

        id = g_strdup (name);
        p = strchr (id, ':');

        if (p) {
                *p = '-';
        }

        return id;
}

static void
print_reference (gpointer item, gpointer user_data)
{
	gchar *shortname, *id;
	FILE *f = (FILE *)user_data;

	shortname = qname_to_shortname ((gchar *) item);
        id = shortname_to_id (shortname);

	g_fprintf (f,"<link linkend='%s'>%s</link>, ", id , shortname);

	g_free (shortname);
	g_free (id);
}

static void
print_variablelist_entry (FILE        *f,
                          const gchar *param,
                          const gchar *value)
{
        g_fprintf (f, "<varlistentry>\n");
        g_fprintf (f, "<term><parameter>%s</parameter>&#160;:</term>\n", param);
        g_fprintf (f, "<listitem><simpara>%s</simpara></listitem>\n", (value) ? value : "--");
        g_fprintf (f, "</varlistentry>\n");
}

static void
print_variablelist_entry_list (FILE        *f,
                               const gchar *param,
                               GList       *list)
{
        g_fprintf (f, "<varlistentry>\n");
        g_fprintf (f, "<term><parameter>%s</parameter>&#160;:</term>\n", param);
        g_fprintf (f, "<listitem><simpara>");

        if (list) {
                g_list_foreach (list, print_reference, f);
        } else {
                g_fprintf (f, "--");
        }

        g_fprintf (f, "</simpara></listitem>\n");
        g_fprintf (f, "</varlistentry>\n");
}

static void
print_list (FILE *f, GList *list)
{
	GList *it;
	gchar *shortname;

	g_fprintf (f, "<td>");
	for (it = list; it != NULL; it = it->next) {
		shortname = qname_to_shortname ((gchar *)it->data);
		g_fprintf (f, "%s%s", shortname, (it->next ? ", " : ""));
		g_free (shortname);
	}
	g_fprintf (f, "</td>");
}

static void
print_deprecated_message (FILE *f)
{
	g_fprintf (f,"<tr>");
	g_fprintf (f,"<td class=\"deprecated\" colspan=\"2\">This item is deprecated.</td>\n");
	g_fprintf (f,"</tr>\n");
}

static void
print_sgml_header (FILE *f, OntologyDescription *desc)
{
        gchar *upper_name;

        g_fprintf (f, "<?xml version='1.0' encoding='UTF-8'?>\n");

        g_fprintf (f, "<chapter id='%s-ontology'>\n", desc->localPrefix);

        upper_name = g_ascii_strup (desc->localPrefix, -1);
        g_fprintf (f, "<title>%s Ontology</title>\n", upper_name);
        g_free (upper_name);

        /* FIXME: get rid of "<>" */
#if 0
        /* Ontology authors */
        g_fprintf (f, "<authorgroup>\n");
	g_list_foreach (desc->authors, print_author, f);
	g_list_foreach (desc->editors, print_editor, f);
        g_list_foreach (desc->contributors, print_collab, f);
        g_fprintf (f, "</authorgroup>\n");
#endif

        /* FIXME: upstream version, gitlog, copyright */

#if 0
        g_fprintf (f,"<html>\n");
	g_fprintf (f,"<head>\n");
	g_fprintf (f,"\t<link rel=\"stylesheet\" type=\"text/css\"");
	g_fprintf (f," href=\"../resources/nie-maemo.css\" />\n");
	g_fprintf (f,"<title>%s</title>\n", desc->title);
	g_fprintf (f,"</head>\n");
	g_fprintf (f,"<body>\n");
	g_fprintf (f,"<div class=\"head\">\n");
	g_fprintf (f," <div class=\"nav\">\n");

	/* Three logos at the top. Tracker, maemo, nepomuk */
	g_fprintf (f, " <a href=\"http://www.tracker-project.org\">");
	g_fprintf (f, "<img alt=\"Tracker logo\" src=\"../resources/tracker-logo.png\" /></a> \n");
	g_fprintf (f, " <a href=\"http://www.maemo.org\"> <img alt=\"MAEMO logo\" ");
	g_fprintf (f, " src=\"../resources/maemo-logo.gif\" /></a>\n");
	g_fprintf (f, " <a href=\"http://nepomuk.semanticdesktop.org\"> ");
	g_fprintf (f, "<img alt=\"Nepomuk logo\"  src=\"../resources/nepomuk-logo.png\"/></a>\n");

	g_fprintf (f,"</div>\n");
	g_fprintf (f,"</div>\n");

	g_fprintf (f,"<h1>%s</h1>\n", desc->title);
	g_fprintf (f," <dl>\n");
	if (desc->upstream) {
		g_fprintf (f,"  <dt>Upstream:</dt><dd><a href=\"%s\">Upstream version</a></dd>\n",
		           desc->upstream);
	} else {
		g_fprintf (f,"  <dt>Upstream:</dt><dd>Not available</dd>\n");
	}
	g_fprintf (f,"  <dt></dt>\n");
	g_fprintf (f,"  <dt></dt>\n");
	g_fprintf (f, "</dl>\n <dl>\n");
	g_fprintf (f,"  <dt>Authors:</dt>\n");
	g_list_foreach (desc->authors, print_author, f);
	g_fprintf (f, "</dl>\n <dl>\n");
	g_fprintf (f,"  <dt>Editors:</dt>\n");
	g_list_foreach (desc->editors, print_author, f);
	if (desc->contributors) {
		g_fprintf (f, "</dl>\n <dl>\n");
		g_fprintf (f,"  <dt>Contributors:</dt>\n");
		g_list_foreach (desc->contributors, print_author, f);
	}
	g_fprintf (f, "</dl>\n <dl>\n");
	g_fprintf (f,"  <dt>Changelog:</dt>\n");
	g_fprintf (f,"  <dd><a href=\"%s\">Tracker changes</a>",
	           (desc->gitlog ? desc->gitlog : "#"));
	g_fprintf (f," </dl>\n");
	g_fprintf (f,"</div>\n");
	g_fprintf (f,"<p class=\"copyright\">%s</p>\n",
	           (desc->copyright ? desc->copyright : DEFAULT_COPYRIGHT));

	g_fprintf (f,"<hr />\n");
#endif
}

static void
print_sgml_explanation (FILE *f, const gchar *explanation_file)
{
	gchar *raw_content;
	gint   length;

	if (explanation_file && g_file_test (explanation_file, G_FILE_TEST_EXISTS)) {
		if (!g_file_get_contents (explanation_file, &raw_content, &length, NULL)) {
			g_error ("Unable to load '%s'", explanation_file );
		}
		g_fprintf (f, "%s", raw_content);
	}
}

static void
print_sgml_footer (FILE *f)
{
	g_fprintf (f,"</chapter>\n");
}

static void
print_ontology_class (gpointer key, gpointer value, gpointer user_data)
{
	OntologyClass *def = (OntologyClass *)value;
	gchar *name, *id;
	FILE *f = (FILE *)user_data;

	g_return_if_fail (f != NULL);

	name = qname_to_shortname (def->classname);

        id = shortname_to_id (name);
        g_fprintf (f, "<refsect2 id='%s'>\n", id);
        g_free (id);

        g_fprintf (f, "<title>%s</title>\n", name);

        if (def->description) {
                g_fprintf (f, "<para>%s</para>\n", def->description);
        }

        g_fprintf (f, "<variablelist>\n");

        print_variablelist_entry_list (f, "Superclasses", def->superclasses);
        print_variablelist_entry_list (f, "Subclasses", def->subclasses);
        print_variablelist_entry_list (f, "In domain of", def->in_domain_of);
        print_variablelist_entry_list (f, "In range of", def->in_range_of);

        if (def->instances) {
                print_variablelist_entry_list (f, "Predefined instances", def->instances);
        }

        g_fprintf (f, "</variablelist>\n");

        if (def->notify) {
                g_fprintf (f, "<note>\n");
                g_fprintf (f, "<title>Note:</title>\n");
                g_fprintf (f, "<para>This class notifies about changes</para>\n");
                g_fprintf (f, "</note>\n");
        }

        g_fprintf (f, "</refsect2>\n\n");

        g_free (name);
}

static void
print_ontology_property (gpointer key, gpointer value, gpointer user_data)
{
	OntologyProperty *def = (OntologyProperty *) value;
	gchar *name, *id;
	FILE *f = (FILE *) user_data;

	g_return_if_fail (f != NULL);

	name = qname_to_shortname (def->propertyname);
        id = shortname_to_id (name);

        g_fprintf (f, "<refsect2 id='%s'>\n", id);
        g_free (id);

        g_fprintf (f, "<title>%s</title>\n", name);

        if (def->description) {
                g_fprintf (f, "<para>%s</para>\n", def->description);
        }


        g_fprintf (f, "<variablelist>\n");

        print_variablelist_entry_list (f, "Type", def->type);
        print_variablelist_entry_list (f, "Domain", def->domain);
        print_variablelist_entry_list (f, "Range", def->range);
        print_variablelist_entry_list (f, "Superproperties", def->superproperties);
        print_variablelist_entry_list (f, "Subproperties", def->subproperties);

        if (def->max_cardinality) {
                print_variablelist_entry (f, "Cardinality", def->max_cardinality);
        }

        g_fprintf (f, "</variablelist>\n");
        g_fprintf (f, "</refsect2>\n\n");
}

void
ttl_sgml_print (OntologyDescription *description,
                Ontology *ontology,
                FILE *f,
                const gchar *class_location_file,
                const gchar *explanation_file)
{
        gchar *upper_name;

        upper_name = g_ascii_strup (description->localPrefix, -1);

        qname_init (description->baseUrl, description->localPrefix, class_location_file);
	print_sgml_header (f, description);

        /* FIXME: make desc files sgml */
	print_sgml_explanation (f, explanation_file);

        g_fprintf (f, "<section id='%s-classes'>\n", description->localPrefix);
	g_fprintf (f, "<title>%s Ontology Classes</title>\n", upper_name);
	g_hash_table_foreach (ontology->classes, print_ontology_class, f);
        g_fprintf (f, "</section>\n");

        g_fprintf (f, "<section id='%s-properties'>\n", description->localPrefix);
	g_fprintf (f, "<title>%s Ontology Properties</title>\n", upper_name);
	g_hash_table_foreach (ontology->properties, print_ontology_property, f);
        g_fprintf (f, "</section>\n");

	print_sgml_footer (f);

        g_free (upper_name);
}
