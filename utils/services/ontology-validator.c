#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>

#include <libtracker-data/tracker-sparql-query.h>

static gchar	     *ontology_dir = NULL;

static GOptionEntry   entries[] = {
	{ "ontology-dir", 'o', 0, G_OPTION_ARG_FILENAME, &ontology_dir,
	  "Directory containing the ontology description files (TTL FORMAT)",
	  NULL
	},
	{ NULL }
};

#define RDFS_CLASS "http://www.w3.org/2000/01/rdf-schema#Class"
#define RDF_PROPERTY "http://www.w3.org/1999/02/22-rdf-syntax-ns#Property"
#define RDFS_SUBCLASSOF  "http://www.w3.org/2000/01/rdf-schema#subClassOf"
#define RDFS_SUBPROPERTYOF  "http://www.w3.org/2000/01/rdf-schema#subPropertyOf"
#define RDFS_TYPE "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
#define TRACKER_NS "http://www.tracker-project.org/ontologies/tracker#Namespace"
#define TRACKER_PREFIX "http://www.tracker-project.org/ontologies/tracker#prefix"
#define RDFS_RANGE "http://www.w3.org/2000/01/rdf-schema#range"
#define RDFS_DOMAIN "http://www.w3.org/2000/01/rdf-schema#domain"


static GList *unknown_items = NULL;
static GList *known_items = NULL;
static GList *unknown_predicates = NULL;

static gboolean
exists_or_already_reported (const gchar *item)
{
        if (!g_list_find_custom (known_items, 
                                 item, 
                                 (GCompareFunc) g_strcmp0)){
                if (!g_list_find_custom (unknown_items,
                                         item,
                                         (GCompareFunc) g_strcmp0)) {
                        return FALSE;
                }
        }
        return TRUE;
}

static void
turtle_load_ontology (const gchar *turtle_subject,
                      const gchar *turtle_predicate,
                      const gchar *turtle_object)
{

        /* nmo:Email a rdfs:Class 
         *  If rdfs:Class exists, add nmo:Email to the known_items 
         **/
        if (!g_strcmp0 (turtle_predicate, RDFS_TYPE)) {

                if (!g_strcmp0 (turtle_object, TRACKER_NS)) {
                        /* Ignore the internal tracker namespace definitions */
                        return;
                }

                /* Check the nmo:Email hasn't already be defined
                 *  (ignoring rdfs:Class and rdf:Property for bootstraping reasons)
                 */
                if (exists_or_already_reported (turtle_subject)
                    && g_strcmp0 (turtle_subject, RDFS_CLASS)
                    && g_strcmp0 (turtle_subject, RDF_PROPERTY)) {
                        g_error ("%s is already defined", turtle_subject);
                        return;
                }

                /* Check the object class is already defined */
                if (!exists_or_already_reported (turtle_object)) {
                        g_error ("%s is a %s but %s is not defined",
                                 turtle_subject, turtle_object, turtle_object);
                } else {
                        /* A new type defined... if it is a predicate, 
                         *  remove it from the maybe list! 
                         */
                        if (!g_strcmp0 (turtle_object, RDF_PROPERTY)) {
                                GList *link;
                                link = g_list_find_custom (unknown_predicates, 
                                                           turtle_subject, 
                                                           (GCompareFunc)g_strcmp0);
                                if (link) {
                                        unknown_predicates = g_list_remove_link (unknown_predicates,
                                                                                 link);
                                } 
                        }
                        known_items = g_list_prepend (known_items, g_strdup (turtle_subject));
                }
                return;
        }

        /*
         * nmo:Message rdfs:subClassOf nie:InformationElement
         *  Check nie:InformationElement is defined
         */
        if (!g_strcmp0 (turtle_predicate, RDFS_SUBCLASSOF)
            || !g_strcmp0 (turtle_predicate, RDFS_SUBPROPERTYOF)
            || !g_strcmp0 (turtle_predicate, RDFS_RANGE)
            || !g_strcmp0 (turtle_predicate, RDFS_DOMAIN)) {
                /* Check the class is already defined */
                if (!exists_or_already_reported (turtle_object)) {
                        g_error ("Class %s refers to %s but it is not defined",
                                 turtle_subject, turtle_object);
                }
                return;
        }

        /*
         * The predicate is not type, subclass, range, domain... 
         *   Put it in maybe
         */
        if (!exists_or_already_reported (turtle_predicate)
            && !g_list_find_custom (unknown_predicates, 
                                    turtle_predicate, 
                                    (GCompareFunc) g_strcmp0)) {
                unknown_predicates = g_list_prepend (unknown_predicates, 
                                                     g_strdup (turtle_predicate));
        }
}

static void 
process_file (const gchar *ttl_file)
{
	TrackerTurtleReader *reader;
	GError *error = NULL;

	g_print ("Processing %s\n", ttl_file);

	reader = tracker_turtle_reader_new (ttl_file);

	while (error == NULL && tracker_turtle_reader_next (reader, &error)) {
		turtle_load_ontology (tracker_turtle_reader_get_subject (reader),
		                      tracker_turtle_reader_get_predicate (reader),
		                      tracker_turtle_reader_get_object (reader));
	}

	g_object_unref (reader);

	if (error) {
		g_message ("Turtle parse error: %s", error->message);
		g_error_free (error);
	}
}

static void
load_ontology_files (const gchar *services_dir)
{
        GList       *files = NULL;
        GDir        *services;
        const gchar *conf_file;
        GFile       *f;
        gchar       *dir_uri, *fullpath;
        
        f = g_file_new_for_path (services_dir);
        dir_uri = g_file_get_path (f);

        g_print ("dir_uri %s\n", dir_uri);
        services = g_dir_open (dir_uri, 0, NULL);
        
        conf_file = g_dir_read_name (services);
                        
        while (conf_file) {

                if (!g_str_has_suffix (conf_file, "ontology")) {
                        conf_file = g_dir_read_name (services);
                        continue;
                }
                
                fullpath = g_build_filename (dir_uri, conf_file, NULL);
                files = g_list_insert_sorted (files, fullpath, (GCompareFunc) g_strcmp0);
                conf_file = g_dir_read_name (services);
        }

        g_dir_close (services);

        //process_file (fullpath, turtle_load_ontology, NULL);
        g_list_foreach (files, (GFunc) process_file, turtle_load_ontology);

        g_list_foreach (files, (GFunc) g_free, NULL);
        g_object_unref (f);
        g_free (dir_uri);
}

static void
load_basic_classes ()
{
        known_items = g_list_prepend (known_items, RDFS_CLASS);
        known_items = g_list_prepend (known_items, RDF_PROPERTY);
}

gint
main (gint argc, gchar **argv) 
{
        GOptionContext *context;

        g_type_init ();


	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new ("- Validate the ontology consistency");

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

        if (!ontology_dir) {
		gchar *help;

		g_printerr ("%s\n\n",
			    "Ontology directory is mandatory");

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return -1;
        }

        load_basic_classes ();
        //"/home/ivan/devel/codethink/tracker-ssh/data/services"
        load_ontology_files (ontology_dir);

        GList *it;
        for (it = unknown_predicates; it != NULL; it = it->next) {
                g_error ("Predicate '%s' is used in the ontology, but it is not defined\n",
                         (gchar *)it->data);
        }

        return 0;
}
