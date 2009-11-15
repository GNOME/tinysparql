#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdio.h>
#include "ttl_loader.h"
#include "ttl_model.h"
#include "ttl_html.h"

static gchar *desc_file = NULL;
static gchar *output_file = NULL;
static gchar *class_location_file = NULL;
static gchar *explanation_file = NULL;

static GOptionEntry   entries[] = {
	{ "desc", 'd', 0, G_OPTION_ARG_FILENAME, &desc_file,
	  "TTL file with the ontology description and documentation",
	  NULL
	},
	{ "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_file,
	  "File to write the output (default stdout)",
	  NULL
	},
        { "links", 'l', 0, G_OPTION_ARG_FILENAME, &class_location_file,
          "File with pairs: (prefix where the class is defined, class)",
          NULL
        },
        { "explanation", 'e', 0, G_OPTION_ARG_FILENAME, &explanation_file,
          "Verbosy explanation file in HTML format to include in the webpage",
          NULL
        },
	{ NULL }
};

gint
main (gint argc, gchar **argv) 
{
        GOptionContext *context;
        Ontology *ontology = NULL;
        OntologyDescription *description = NULL;
        gchar *ttl_file = NULL;
        gchar *dirname = NULL;
        FILE *f = NULL;

        g_type_init ();

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new ("- Generates HTML doc for a TTL file");

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

        if (!desc_file) {
		gchar *help;

		g_printerr ("%s\n\n",
			    "Description file is mandatory");

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return -1;
        }

        if (output_file) {
                f = fopen (output_file, "w");
        } else {
                f = stdout;
        }
        g_assert (f != NULL);

        description = ttl_loader_load_description (desc_file);

        dirname = g_path_get_dirname (desc_file);
        ttl_file = g_build_filename (dirname,
                                     description->relativePath, 
                                     NULL);

        ontology = ttl_loader_load_ontology (ttl_file);
        g_free (ttl_file);
        g_free (dirname);

        ttl_html_print (description, ontology, f, class_location_file, explanation_file);

        ttl_loader_free_ontology (ontology);
        ttl_loader_free_description (description);

        g_option_context_free (context);
        
        fclose (f);

        return 0;
}
