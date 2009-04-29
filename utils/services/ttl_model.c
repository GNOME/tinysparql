#include "ttl_model.h"

OntologyClass *
ttl_model_class_new (const gchar *classname)
{
        OntologyClass *def = NULL;

        def = g_new0 (OntologyClass, 1);

        def->classname = g_strdup (classname);
        def->superclasses = NULL;
        def->subclasses = NULL;
        def->in_domain_of = NULL;
        def->in_range_of = NULL;
        def->description = NULL;

        return def;
}

void
ttl_model_class_free (OntologyClass *def) 
{
        if (def->classname) {
                g_free (def->classname);
        }

        g_list_foreach (def->superclasses, (GFunc) g_free, NULL);
        g_list_foreach (def->subclasses, (GFunc) g_free, NULL);
        g_list_foreach (def->in_domain_of, (GFunc) g_free, NULL);
        g_list_foreach (def->in_range_of, (GFunc) g_free, NULL);

        if (def->description) {
                g_free (def->description);
        }

        g_free (def);
}

OntologyProperty *
ttl_model_property_new (gchar *propname)
{
        OntologyProperty *prop;

        prop = g_new0 (OntologyProperty, 1);

        prop->propertyname = g_strdup (propname);
        prop->type = NULL;
        prop->domain = NULL;
        prop->range = NULL;
        prop->superproperties = NULL;
        prop->subproperties = NULL;
        prop->max_cardinality = NULL;
        prop->description = NULL;

        return prop;
}

void
ttl_model_property_free (OntologyProperty *def)
{
        if (def->propertyname) {
                g_free (def->propertyname);
        }
        
        g_list_foreach (def->type, (GFunc) g_free, NULL);
        g_list_foreach (def->domain, (GFunc) g_free, NULL);
        g_list_foreach (def->range, (GFunc) g_free, NULL);
        g_list_foreach (def->superproperties, (GFunc) g_free, NULL);
        g_list_foreach (def->subproperties, (GFunc) g_free, NULL);

        if (def->max_cardinality) {
                g_free (def->max_cardinality);
        }

        if (def->description) {
                g_free (def->description);
        }

        g_free (def);
}

OntologyDescription *
ttl_model_description_new (void)
{
        OntologyDescription *desc;

        desc = g_new0 (OntologyDescription, 1);
        desc->title = NULL;
        desc->authors = NULL;
        desc->editors = NULL;
        desc->contributors = NULL;
        desc->baseUrl = NULL;
        desc->relativePath = NULL;

        return desc;
}

void
ttl_model_description_free (OntologyDescription *desc)
{
        if (desc->title) {
                g_free (desc->title);
        }

        g_list_foreach (desc->authors, (GFunc)g_free, NULL);
        g_list_foreach (desc->editors, (GFunc)g_free, NULL);
        g_list_foreach (desc->contributors, (GFunc)g_free, NULL);

        if (desc->baseUrl) {
                g_free (desc->baseUrl);
        }

        if (desc->relativePath) {
                g_free (desc->relativePath);
        }

        g_free (desc);
}
