/* Yuck */

#include "config.h"

#include <gio/gio.h>
#include <tracker-sparql.h>
#include <dlfcn.h>

#define LIBSOUP_2_SONAME "libsoup-2.4." G_MODULE_SUFFIX

static gboolean initialized = FALSE;

GType (* remote_endpoint_get_type) (void) = NULL;

TrackerEndpoint * (* remote_endpoint_new) (TrackerSparqlConnection  *sparql_connection,
                                           guint                     port,
                                           GTlsCertificate          *certificate,
                                           GCancellable             *cancellable,
                                           GError                  **error) = NULL;
TrackerSparqlConnection * (* remote_connection_new) (const gchar *url_base) = NULL;

static void
tracker_init_remote (void)
{
	const char *modules[3] = { 0 };
	gpointer handle = NULL;
	gint i = 0;

	if (initialized)
		return;

	g_assert (g_module_supported ());

#ifdef HAVE_RTLD_NOLOAD
	if ((handle = dlopen (LIBSOUP_2_SONAME, RTLD_NOW | RTLD_NOLOAD))) {
		/* Force load of soup2 module */
		modules[0] = "libtracker-remote-soup2." G_MODULE_SUFFIX;
	} else
#endif
	{
		modules[0] = "libtracker-remote-soup3." G_MODULE_SUFFIX;
		modules[1] = "libtracker-remote-soup2." G_MODULE_SUFFIX;
	}

	g_clear_pointer (&handle, dlclose);

	for (i = 0; modules[i]; i++) {
		GModule *remote_module;
		gchar *module_path;

		if (g_strcmp0 (g_get_current_dir (), BUILDROOT) == 0) {
			/* Detect in-build runtime of this code, this may happen
			 * building introspection information or running tests.
			 * We want the in-tree modules to be loaded then.
			 */
			module_path = g_strdup_printf (BUILD_LIBDIR "/%s", modules[i]);
		} else {
			module_path = g_strdup_printf (PRIVATE_LIBDIR "/%s", modules[i]);
		}

		remote_module = g_module_open (module_path,
		                               G_MODULE_BIND_LAZY |
		                               G_MODULE_BIND_LOCAL);
		g_free (module_path);

		if (!remote_module)
			continue;

		if (!g_module_symbol (remote_module, "tracker_endpoint_http_get_type", (gpointer *) &remote_endpoint_get_type) ||
		    !g_module_symbol (remote_module, "tracker_endpoint_http_new", (gpointer *) &remote_endpoint_new) ||
		    !g_module_symbol (remote_module, "tracker_remote_connection_new", (gpointer *) &remote_connection_new)) {
			g_clear_pointer (&remote_module, g_module_close);
			continue;
		}

		g_module_make_resident (remote_module);
		g_module_close (remote_module);
		initialized = TRUE;
		return;
	}

	g_assert_not_reached ();
}

GType
tracker_endpoint_http_get_type (void)
{
	tracker_init_remote ();

	return remote_endpoint_get_type ();
}

TrackerEndpointHttp *
tracker_endpoint_http_new (TrackerSparqlConnection  *sparql_connection,
                           guint                     port,
                           GTlsCertificate          *certificate,
                           GCancellable             *cancellable,
                           GError                  **error)
{
	tracker_init_remote ();

	return (TrackerEndpointHttp *) remote_endpoint_new (sparql_connection,
	                                                    port,
	                                                    certificate,
	                                                    cancellable,
	                                                    error);
}

TrackerSparqlConnection *
tracker_sparql_connection_remote_new (const gchar *url_base)
{
	tracker_init_remote ();

	return remote_connection_new (url_base);
}
