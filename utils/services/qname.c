#include "qname.h"

static gchar *local_uri = NULL;

typedef struct {
        const gchar *namespace;
        const gchar *uri;
} Namespace;

Namespace NAMESPACES [] = {
        {"dc", "http://purl.org/dc/elements/1.1/"},
        {"xsd", "http://www.w3.org/2001/XMLSchema#"},
        {"fts", "http://www.tracker-project.org/ontologies/fts#"},
        {"mto", "http://www.tracker-project.org/temp/mto#"},
        {"mlo", "http://www.tracker-project.org/temp/mlo#"},
        {"nao", "http://www.semanticdesktop.org/ontologies/2007/08/15/nao#"},
        {"ncal", "http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#"},
        {"nco", "http://www.semanticdesktop.org/ontologies/2007/03/22/nco#"},
        {"nfo", "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#"},
        {"nid3", "http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#"},
        {"nie", "http://www.semanticdesktop.org/ontologies/2007/01/19/nie#"},
        {"nmm", "http://www.tracker-project.org/temp/nmm#"},
        {"nmo", "http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#"},
        {"nrl", "http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#"},
        {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
        {"rdfs", "http://www.w3.org/2000/01/rdf-schema#"},
        {"tracker", "http://www.tracker-project.org/ontologies/tracker#"},
        {NULL, NULL}
};


void   
qname_init (const gchar *luri)
{
        if (local_uri) {
                g_warning ("Reinitializing qname_module");
                g_free (local_uri);
        }
        local_uri = g_strdup (luri);
}

void   
qname_shutdown (void)
{
        g_free (local_uri);
}

gchar *
qname_to_link (const gchar *qname)
{
        gchar **pieces;
        gchar *name;
        g_debug ("link for qname '%s' (local_uri = '%s')\n", qname, local_uri);
        if (local_uri) {
                /* There is a local URI! */
                if (g_str_has_prefix (qname, local_uri)) {
                        pieces = g_strsplit (qname, "#", 2);
                        g_assert (g_strv_length (pieces) == 2);
                        name = g_strdup_printf ("#%s", pieces[1]);
                        g_strfreev (pieces);
                        return name;
                }
        }
        
        return g_strdup (qname);
}

gchar *
qname_to_shortname (const gchar *qname)
{
        gchar **pieces;
        gchar  *name = NULL;
        gint    i;

        for (i = 0; NAMESPACES[i].namespace != NULL; i++) {
                if (g_str_has_prefix (qname, NAMESPACES[i].uri)) {
                        pieces = g_strsplit (qname, "#", 2);
                        if (g_strv_length (pieces) != 2) {
                                g_warning ("Unable to get the shortname for %s", qname);
                                break;
                        }

                        name = g_strdup_printf ("%s:%s", 
                                                NAMESPACES[i].namespace, 
                                                pieces[1]);
                        g_strfreev (pieces);
                        break;
                }
        }

        if (!name) {
                return g_strdup (qname);
        } else {
                return name;
        }
}

gboolean 
qname_is_basic_type (const gchar *qname)
{
        gint i; 
        /* dc: or xsd: are basic types */
        for (i = 0; NAMESPACES[i].namespace != NULL && i < 3; i++) {
                if (g_str_has_prefix (qname, NAMESPACES[i].uri)) {
                        return TRUE;
                }
        }
        return FALSE;
}
