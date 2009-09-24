#ifndef __TTL_QNAME_H__
#define __TTL_QNAME_H__

#include <glib.h>

G_BEGIN_DECLS

void     qname_init          (const gchar *local_uri);
void     qname_shutdown      (void);

gchar *  qname_to_link       (const gchar *qname);
gchar *  qname_to_shortname  (const gchar *qname);

gboolean qname_is_basic_type (const gchar *qname);


G_END_DECLS

#endif
