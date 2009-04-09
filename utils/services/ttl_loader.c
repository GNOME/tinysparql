#include "ttl_loader.h"
#include <raptor.h>
#include <glib/gstdio.h>

typedef void (* TurtleTripleCallback) (void *user_data, const raptor_statement *triple);

/* Ontology classes */
#define RDFS_CLASS "http://www.w3.org/2000/01/rdf-schema#Class"
#define RDF_PROPERTY "http://www.w3.org/1999/02/22-rdf-syntax-ns#Property"
#define RDFS_SUBCLASSOF  "http://www.w3.org/2000/01/rdf-schema#subClassOf"
#define RDFS_TYPE "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
#define RDFS_RANGE "http://www.w3.org/2000/01/rdf-schema#range"
#define RDFS_DOMAIN "http://www.w3.org/2000/01/rdf-schema#domain"
#define RDFS_COMMENT "http://www.w3.org/2000/01/rdf-schema#comment"
#define RDFS_LABEL "http://www.w3.org/2000/01/rdf-schema#label" 
#define RDFS_SUBPROPERTYOF "http://www.w3.org/2000/01/rdf-schema#subPropertyOf"

#define NRL_MAX_CARDINALITY "http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#maxCardinality"

#define TRACKER_NAMESPACE "http://www.tracker-project.org/ontologies/tracker#Namespace"

/* Ontology description */
#define DSC_PREFIX "http://www.tracker-project.org/temp/dsc#"

#define DSC_ONTOLOGY DSC_PREFIX "Ontology"
#define DSC_TITLE DSC_PREFIX "title"
#define DSC_AUTHOR DSC_PREFIX "author"
#define DSC_EDITOR DSC_PREFIX "editor"
#define DSC_CONTRIBUTOR DSC_PREFIX "contributor"
#define DSC_BASEURI DSC_PREFIX "baseUrl"
#define DSC_RELPATH DSC_PREFIX "relativePath"

static gboolean initialized = FALSE;

void
ttl_loader_init (void)
{
        if (!initialized) {
                raptor_init ();
                initialized = TRUE;
        }
}

void
ttl_loader_shutdown (void)
{
        if (initialized) {
                raptor_finish ();
                initialized = FALSE;
        }
}


static void 
raptor_error (void *user_data, raptor_locator* locator, const char *message)
{
	g_message ("RAPTOR parse error: %s:%d:%d: %s\n", 
		   (gchar *) user_data,
		   locator->line,
		   locator->column,
		   message);
}



static void
load_in_memory (void                   *user_data,
                const raptor_statement *triple) 
{

        gchar *turtle_subject;
        gchar *turtle_predicate;
        char  *turtle_object;
        Ontology *ontology = (Ontology *)user_data;

        g_return_if_fail (ontology != NULL);

	/* set new statement */
	turtle_subject = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->subject));
	turtle_predicate = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->predicate));
	turtle_object = g_strdup ((const gchar *) triple->object);

        if (!g_strcmp0 (turtle_predicate, RDFS_TYPE)) {
                /* It is a definition of class or property */
                if (!g_strcmp0 (turtle_object, RDFS_CLASS)) {
                        g_hash_table_insert (ontology->classes, 
                                             g_strdup (turtle_subject), 
                                             ttl_model_class_new (turtle_subject));

                } else if (!g_strcmp0 (turtle_object, RDF_PROPERTY)) {
                        g_hash_table_insert (ontology->properties,
                                             g_strdup (turtle_subject), 
                                             ttl_model_property_new (turtle_subject));

                } else {
                        /* g_print ("FIXME Ignoring %s %s %s\n",
                                 turtle_subject, turtle_predicate, turtle_object);
                        */
                }

        } else if (!g_strcmp0 (turtle_predicate, RDFS_SUBCLASSOF)) {
                /*
                 * A subclass of B: 
                 *  - Add B in A->superclasses list 
                 *  - Add A in B->subclasses list (if B is in this ontology!)
                 */
                OntologyClass *def;

                def = g_hash_table_lookup (ontology->classes, turtle_subject);
                if (!def) {
                        g_error ("Something wrong");
                }

                def->superclasses = g_list_prepend (def->superclasses, 
                                                    g_strdup (turtle_object));

                def = g_hash_table_lookup (ontology->classes, turtle_object);
                if (def) {
                        def->subclasses = g_list_prepend (def->subclasses, 
                                                          g_strdup (turtle_subject));
                }
        } else if (!g_strcmp0 (turtle_predicate, RDFS_COMMENT)) {
                OntologyClass *klass;
                OntologyProperty *prop;

                klass = g_hash_table_lookup (ontology->classes, turtle_subject);
                if (klass) {
                        klass->description = g_strdup (turtle_object);
                } else {
                        prop = g_hash_table_lookup (ontology->properties, turtle_subject);
                        if (prop) {
                                prop->description = g_strdup (turtle_object);
                        } else {
                                g_error ("UHUMMM %s", turtle_subject);
                        }
                }

        } else if (!g_strcmp0 (turtle_predicate, RDFS_DOMAIN)) {
                /*
                 * (prop A) has domain (class B)
                 *  -> add B in A->domain
                 *  -> add A in B->in_domain_of (if B is defined in this ontology!)
                 */
                OntologyProperty *prop;
                OntologyClass *klass;

                prop = g_hash_table_lookup (ontology->properties, turtle_subject);
                if (!prop) {
                        g_error ("Strange error in domain (%s doesnt exist!)",
                                 turtle_subject);
                }
                prop->domain = g_list_prepend (prop->domain, g_strdup (turtle_object));

                klass = g_hash_table_lookup (ontology->classes, turtle_object);
                if (klass) {
                        klass->in_domain_of = g_list_prepend (klass->in_domain_of,
                                                              g_strdup (turtle_subject));
                }
                
        } else if (!g_strcmp0 (turtle_predicate, RDFS_RANGE)) {
                /*
                 * (prop A) has range (class B)
                 *  -> add B in A->range
                 *  -> add A in B->in_range_of (if B is defined in this ontology!)
                 */
                OntologyProperty *prop;
                OntologyClass *klass;

                prop = g_hash_table_lookup (ontology->properties, turtle_subject);
                if (!prop) {
                        g_error ("Strange error in domain (%s doesnt exist!)",
                                 turtle_subject);
                }
                prop->range = g_list_prepend (prop->range, g_strdup (turtle_object));

                klass = g_hash_table_lookup (ontology->classes, turtle_object);
                if (klass) {
                        klass->in_range_of = g_list_prepend (klass->in_range_of,
                                                              g_strdup (turtle_subject));
                }
        } else if (!g_strcmp0 (turtle_predicate, NRL_MAX_CARDINALITY)) {
                OntologyProperty *prop;

                prop = g_hash_table_lookup (ontology->properties, turtle_subject);
                if (!prop) {
                        g_error ("Strange error in max cardinality (%s doesnt exist!)",
                                 turtle_subject);
                }
                prop->max_cardinality = g_strdup (turtle_object);

        } else if (!g_strcmp0 (turtle_predicate, RDFS_SUBPROPERTYOF)) {
                /*
                 * (prop A) is subproperty of (prop B)
                 *  -> add B in A->superproperties
                 *  -> add A in B->subproperties (if B is in this ontology)
                 */
                OntologyProperty *propA, *propB;
                
                propA = g_hash_table_lookup (ontology->properties, turtle_subject);
                if (!propA) {
                        g_error ("Strange error in subpropertyof (%s doesnt exist!)",
                                 turtle_subject);
                }
                propA->superproperties = g_list_prepend (propA->superproperties,
                                                         g_strdup (turtle_object));

                propB = g_hash_table_lookup (ontology->properties, turtle_object);
                if (propB) {
                        propB->subproperties = g_list_prepend (propB->subproperties,
                                                               g_strdup (turtle_subject));
                }
                                                         



        } else if (!g_strcmp0 (turtle_predicate, RDFS_LABEL)) {
                /* Intentionalyy ignored */
        } else {
                /* DEBUG 
                g_print ("UNHANDLED %s %s %s\n", 
                         turtle_subject, turtle_predicate, turtle_object);
                */
        }

        g_free (turtle_subject);
        g_free (turtle_predicate);
        g_free (turtle_object);

}

static void
load_description (void                   *user_data,
                  const raptor_statement *triple) 
{
        gchar *turtle_subject;
        gchar *turtle_predicate;
        char  *turtle_object;
        OntologyDescription *desc = (OntologyDescription *)user_data;

	/* set new statement */
	turtle_subject = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->subject));
	turtle_predicate = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->predicate));
	turtle_object = g_strdup ((const gchar *) triple->object);

        if (!g_strcmp0 (turtle_predicate, RDFS_TYPE)) {
                g_assert (!g_strcmp0 (turtle_object, DSC_ONTOLOGY));
        } else if (!g_strcmp0 (turtle_predicate, DSC_TITLE)) {
                desc->title = g_strdup (turtle_object);
        } else if (!g_strcmp0 (turtle_predicate, DSC_AUTHOR)) {
                desc->authors = g_list_prepend (desc->authors, g_strdup (turtle_object));
        } else if (!g_strcmp0 (turtle_predicate, DSC_EDITOR)) {
                desc->editors = g_list_prepend (desc->editors, g_strdup (turtle_object));
        } else if (!g_strcmp0 (turtle_predicate, DSC_CONTRIBUTOR)) {
                desc->contributors = g_list_prepend (desc->contributors, 
                                                     g_strdup (turtle_object));
        } else if (!g_strcmp0 (turtle_predicate, DSC_BASEURI)) {
                desc->baseUrl = g_strdup (turtle_object);
        } else if (!g_strcmp0 (turtle_predicate, DSC_RELPATH)) {
                desc->relativePath = g_strdup (turtle_object);
        } else {
                g_critical ("Unhandled element %s", turtle_predicate);
        }

        g_free (turtle_subject);
        g_free (turtle_predicate);
        g_free (turtle_object);
}


static void 
process_file (const gchar *ttl_file, 
              TurtleTripleCallback handler,
              gpointer user_data)
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

        raptor_free_uri (base_uri);
        raptor_free_uri (buri);
	raptor_free_uri (uri);
	raptor_free_parser (parser);
        g_free (uri_string);

	fclose (file);
}


Ontology *
ttl_loader_load_ontology (const gchar *ttl_file)
{
        Ontology *ontology;

        ontology = g_new0 (Ontology, 1);
        ontology->classes = g_hash_table_new_full (g_str_hash, 
                                                   g_str_equal,
                                                   g_free, 
                                                   (GDestroyNotify)ttl_model_class_free);

        ontology->properties = g_hash_table_new_full (g_str_hash, 
                                                      g_str_equal,
                                                      g_free, 
                                                      (GDestroyNotify)ttl_model_property_free);

        if (ttl_file) {
                process_file (ttl_file, load_in_memory, ontology);
        } else {
                g_warning ("Unable to open '%s'", ttl_file);
        }

        return ontology;
}

OntologyDescription *
ttl_loader_load_description (const gchar *filename)
{
        OntologyDescription *desc;
        
        desc = ttl_model_description_new ();

        process_file (filename, load_description, desc);

        return desc;
}


void 
ttl_loader_free_ontology (Ontology *ontology) 
{
        g_hash_table_destroy (ontology->classes);
        g_hash_table_destroy (ontology->properties);
        g_free (ontology);
}

void
ttl_loader_free_description (OntologyDescription *desc)
{
        ttl_model_description_free (desc);
}
