#include <raptor.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>

static gchar	     *ontology_dir = NULL;
static gchar         *ttl_file = NULL;

static GOptionEntry   entries[] = {
	{ "ttl-file", 't', 0, G_OPTION_ARG_FILENAME, &ttl_file,
	  "Turtle file to validate",
	  NULL
	},
	{ "ontology-dir", 'o', 0, G_OPTION_ARG_FILENAME, &ontology_dir,
	  "Directory containing the ontology description files (TTL FORMAT)",
	  NULL
	},
	{ NULL }
};

typedef void (* TurtleTripleCallback) (void *user_data, const raptor_statement *triple);

#define CLASS "http://www.w3.org/2000/01/rdf-schema#Class"
#define PROPERTY "http://www.w3.org/1999/02/22-rdf-syntax-ns#Property"
#define IS "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"

static gboolean error_flag = FALSE;

static void 
raptor_error (void *user_data, raptor_locator* locator, const char *message)
{
	g_message ("RAPTOR parse error: %s:%d:%d: %s\n", 
		   (gchar *) user_data,
		   locator->line,
		   locator->column,
		   message);
        error_flag = TRUE;
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

        if (!g_strcmp0 (turtle_predicate, IS)) {
                known_items = g_list_prepend (known_items, turtle_subject);
        }

}

static void
turtle_statement_handler (void                   *user_data,
                          const raptor_statement *triple) 
{

        gchar *turtle_subject;
        gchar *turtle_predicate;
        char  *turtle_object;

	/* set new statement */
	turtle_subject = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->subject));
	turtle_predicate = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->predicate));
	turtle_object = g_strdup ((const gchar *) triple->object);

        /* Check that predicate exists in the ontology 
        */
        if (!exists_or_already_reported (turtle_predicate)){                        
                g_print ("Unknown property %s\n", turtle_predicate);
                unknown_items = g_list_prepend (unknown_items, turtle_predicate);
                error_flag = TRUE;
        }

        /* And if it is a type... check the object is also there 
         */
        if (!g_strcmp0 (turtle_predicate, IS)) {
                
                if (!exists_or_already_reported (turtle_object)){                        
                        g_print ("Unknown class %s\n", turtle_object);
                        error_flag = TRUE;
                        unknown_items = g_list_prepend (unknown_items, turtle_object);
                }
        }
}


static void 
process_file (const gchar *ttl_file, TurtleTripleCallback handler, void *user_data)
{
        FILE *file;
        raptor_parser *parser;
        raptor_uri *uri, *buri, *base_uri;
	unsigned char  *uri_string;

        file = g_fopen (ttl_file, "r");

	parser = raptor_new_parser ("turtle");
	base_uri = raptor_new_uri ((unsigned char *) "/");

	raptor_set_statement_handler (parser, user_data, 
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
        gint         counter = 0;
        
        f = g_file_new_for_path (services_dir);
        dir_uri = g_file_get_path (f);

        services = g_dir_open (dir_uri, 0, NULL);
        
        conf_file = g_dir_read_name (services);
                        
        while (conf_file) {

                if (!g_str_has_suffix (conf_file, "ontology")) {
                        conf_file = g_dir_read_name (services);
                        continue;
                }
                
                fullpath = g_build_filename (dir_uri, conf_file, NULL);

                process_file (fullpath, turtle_load_ontology, NULL);
                g_free (fullpath);
                counter += 1;
                conf_file = g_dir_read_name (services);
        }

        g_dir_close (services);

        g_list_foreach (files, (GFunc) g_free, NULL);
        g_object_unref (f);
        g_free (dir_uri);
        g_debug ("Loaded %d ontologies\n", counter);
}



gint
main (gint argc, gchar **argv) 
{
        GOptionContext *context;

        g_type_init ();
        raptor_init ();


	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new ("- Validate a turtle file against the ontology");

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

        if (!ontology_dir || !ttl_file) {
		gchar *help;

		g_printerr ("%s\n\n",
			    "Ontology directory and turtle file are mandatory");

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return -1;
        }

        //"/home/ivan/devel/codethink/tracker-ssh/data/services"
        load_ontology_files (ontology_dir);
        process_file (ttl_file, *turtle_statement_handler, NULL);

        if (!error_flag) {
                g_debug ("%s seems OK.", ttl_file);
        }

        raptor_finish ();

        return 0;
}
