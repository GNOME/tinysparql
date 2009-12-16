#ifndef __TTL_SGML_H__
#define __TTL_SGML_H__

#include <gio/gio.h>
#include "ttl_model.h"
#include <stdio.h>

G_BEGIN_DECLS

void ttl_sgml_print (OntologyDescription *description,
                     Ontology *ontology,
                     FILE *output,
                     const gchar *class_location,
                     const gchar *explanation_file);



G_END_DECLS

#endif
