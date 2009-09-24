#ifndef __TTL_GRAPHVIZ_H__
#define __TTL_GRAPHVIZ_H__

#include <gio/gio.h>
#include "ttl_model.h"
#include <stdio.h>

G_BEGIN_DECLS

void ttl_graphviz_print (OntologyDescription *description,
                         Ontology *ontology,
                         FILE *output);



G_END_DECLS

#endif
