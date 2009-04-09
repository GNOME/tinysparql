#ifndef __TTL_LOADER_H__
#define __TTL_LOADER_H__

#include <glib.h>
#include "ttl_model.h"

G_BEGIN_DECLS

void      ttl_loader_init (void);
void      ttl_loader_shutdown (void);

Ontology    *ttl_loader_load_ontology (const gchar *filename);
OntologyDescription *ttl_loader_load_description (const gchar *filename);

void      ttl_loader_free_ontology (Ontology *ontology);
void      ttl_loader_free_description (OntologyDescription *desc);


G_END_DECLS

#endif /* __TTL_LOADER_H__ */
