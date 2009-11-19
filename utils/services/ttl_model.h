#ifndef __TTL_MODEL_H__
#define __TTL_MODEL_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
        gchar *classname;
        GList *superclasses;
        GList *subclasses;
        GList *in_domain_of;
        GList *in_range_of;
        gchar *description;
} OntologyClass;

typedef struct {
        gchar *propertyname;
        GList *type;
        GList *domain;
        GList *range;
        GList *superproperties;
        GList *subproperties;
        gchar *max_cardinality;
        gchar *description;
} OntologyProperty;

typedef struct {
        gchar *title;
        GList *authors;
        GList *editors;
        GList *contributors;
        gchar *gitlog;
        gchar *upstream;
        gchar *copyright;
        gchar *baseUrl;
        gchar *localPrefix;
        gchar *relativePath;
} OntologyDescription;

typedef struct {
        GHashTable *classes;
        GHashTable *properties;
} Ontology;


OntologyClass * ttl_model_class_new (const gchar *classname);
void            ttl_model_class_free (OntologyClass *klass);

OntologyDescription *ttl_model_description_new (void);
void                 ttl_model_description_free (OntologyDescription *desc);

OntologyProperty *ttl_model_property_new (const gchar *propname);
void              ttl_model_property_free (OntologyProperty *property);

G_END_DECLS

#endif /* __TRACKER_TTL_MODEL_H__ */
