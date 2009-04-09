#include <raptor.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>

static gchar	     *ontology_dir = NULL;

static GOptionEntry   entries[] = {
	{ "ontology-dir", 'o', 0, G_OPTION_ARG_FILENAME, &ontology_dir,
	  "Directory containing the ontology description files (TTL FORMAT)",
	  NULL
	},
	{ NULL }
};

typedef void (* TurtleTripleCallback) (void *user_data, const raptor_statement *triple);

#define RDFS_CLASS "http://www.w3.org/2000/01/rdf-schema#Class"
#define RDF_PROPERTY "http://www.w3.org/1999/02/22-rdf-syntax-ns#Property"
#define RDFS_SUBCLASSOF  "http://www.w3.org/2000/01/rdf-schema#subClassOf"
#define RDFS_TYPE "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
#define TRACKER_NAMESPACE "http://www.tracker-project.org/ontologies/tracker#Namespace"
#define RDFS_RANGE "http://www.w3.org/2000/01/rdf-schema#range"
#define RDFS_DOMAIN "http://www.w3.org/2000/01/rdf-schema#domain"

static void 
raptor_error (void *user_data, raptor_locator* locator, const char *message)
{
	g_message ("RAPTOR parse error: %s:%d:%d: %s\n", 
		   (gchar *) user_data,
		   locator->line,
		   locator->column,
		   message);
}

static GList *unknown_items = NULL;
static GList *known_items = NULL;

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
turtle_load_ontology (void                   *user_data,
                      const raptor_statement *triple) 
{

        gchar *turtle_subject;
        gchar *turtle_predicate;
        char  *turtle_object;

	/* set new statement */
	turtle_subject = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->subject));
	turtle_predicate = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->predicate));
	turtle_object = g_strdup ((const gchar *) triple->object);

        /* nmo:Email a rdfs:Class 
         *  If rdfs:Class exists, add nmo:Email to the known_items 
         **/
        if (!g_strcmp0 (turtle_predicate, RDFS_TYPE)) {

                if (!g_strcmp0 (turtle_object, TRACKER_NAMESPACE)) {
                        /* Ignore the internal tracker namespace definitions */
                        return;
                }

                /* Check the class is already defined */
                if (!exists_or_already_reported (turtle_object)) {
                        g_error ("Class %s is subclass of %s but %s is not defined",
                                 turtle_subject, turtle_object, turtle_object);
                } else {
                        known_items = g_list_prepend (known_items, turtle_subject);
                }
        }

        /*
         * nmo:Message rdfs:subClassOf nie:InformationElement
         *  Check nie:InformationElement is defined
         */
        if (!g_strcmp0 (turtle_predicate, RDFS_SUBCLASSOF)
            || !g_strcmp0 (turtle_predicate, RDFS_RANGE)
            || !g_strcmp0 (turtle_predicate, RDFS_DOMAIN)) {
                /* Check the class is already defined */
                if (!exists_or_already_reported (turtle_object)) {
                        g_error ("Class %s refears to %s but it is not defined",
                                 turtle_subject, turtle_object);
                }
        }


}

static void 
process_file (const gchar *ttl_file, TurtleTripleCallback handler)
{
        FILE *file;
        raptor_parser *parser;
        raptor_uri *uri, *buri, *base_uri;
	unsigned char  *uri_string;

        g_print ("Processing %s\n", ttl_file);
        file = g_fopen (ttl_file, "r");

	parser = raptor_new_parser ("turtle");
	base_uri = raptor_new_uri ((unsigned char *) "/");

	raptor_set_statement_handler (parser, NULL, 
                                      handler);
	raptor_set_fatal_error_handler (parser, (void *)file, raptor_error);
	raptor_set_error_handler (parser, (void *)file, raptor_error);
	raptor_set_warning_handler (parser, (void *)file, raptor_error);

	uri_string = raptor_uri_filename_to_uri_string (ttl_file);
	uri = raptor_new_uri (uri_string);
	buri = raptor_new_uri ((unsigned char *) base_uri);

	raptor_parse_file (parser, uri, buri);

	raptor_free_uri (uri);
	raptor_free_parser (parser);
	fclose (file);
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
        raptor_init ();


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

        raptor_finish ();

        return 0;
}
