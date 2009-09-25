#ifndef __TTL_HTML_H__
#define __TTL_HTML_H__

#include <gio/gio.h>
#include "ttl_model.h"
#include <stdio.h>

G_BEGIN_DECLS

void ttl_html_print (OntologyDescription *description,
                     Ontology *ontology,
                     FILE *output,
                     const gchar *class_location);



G_END_DECLS

#endif
