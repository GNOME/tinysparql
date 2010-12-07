#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "tomboykeybinder.h"

/* Uncomment the next line to print a debug trace. */
/* #define DEBUG */

#ifdef DEBUG
#  define TRACE(x) x
#else
#  define TRACE(x) do {} while (FALSE);
#endif

typedef struct _Binding {
	TomboyBindkeyHandler  handler;
	gpointer              user_data;
	char                 *keystring;
	uint                  keycode;
	uint                  modifiers;
} Binding;

static GSList *bindings = NULL;
static guint32 last_event_time = 0;
static gboolean processing_event = FALSE;

static void
grab_ungrab_with_ignorable_modifiers (GdkWindow *rootwin,
                                      Binding   *binding,
                                      gboolean   grab)
{
	guint mod_masks [] = {
		0, /* modifier only */
                GDK_MOD2_MASK,
                GDK_LOCK_MASK,
                GDK_MOD2_MASK | GDK_LOCK_MASK
	};
	int i;

	for (i = 0; i < G_N_ELEMENTS (mod_masks); i++) {
		if (grab) {
			XGrabKey (GDK_WINDOW_XDISPLAY (rootwin),
			          binding->keycode,
			          binding->modifiers | mod_masks [i],
			          GDK_WINDOW_XID (rootwin),
			          False,
			          GrabModeAsync,
			          GrabModeAsync);
		} else {
			XUngrabKey (GDK_WINDOW_XDISPLAY (rootwin),
			            binding->keycode,
			            binding->modifiers | mod_masks [i],
			            GDK_WINDOW_XID (rootwin));
		}
	}
}

static gboolean
do_grab_key (Binding *binding)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GdkWindow *rootwin = gdk_get_default_root_window ();
        GdkModifierType modifiers;
	guint keysym = 0;

	if (keymap == NULL || rootwin == NULL)
		return FALSE;

        gtk_accelerator_parse (binding->keystring,
                               &keysym, &modifiers);

        if (keysym == 0)
                return FALSE;

	TRACE (g_print ("Got accel %d, %d\n", keysym, modifiers));

        binding->modifiers = modifiers;
	binding->keycode = XKeysymToKeycode (GDK_WINDOW_XDISPLAY (rootwin),
	                                     keysym);
	if (binding->keycode == 0)
		return FALSE;

	TRACE (g_print ("Got keycode %d\n", binding->keycode));

	gdk_error_trap_push ();

	grab_ungrab_with_ignorable_modifiers (rootwin,
	                                      binding,
	                                      TRUE /* grab */);

	gdk_flush ();

	if (gdk_error_trap_pop ()) {
		g_warning ("Binding '%s' failed!\n", binding->keystring);
		return FALSE;
	}

	return TRUE;
}

static gboolean
do_ungrab_key (Binding *binding)
{
	GdkWindow *rootwin = gdk_get_default_root_window ();

	TRACE (g_print ("Removing grab for '%s'\n", binding->keystring));

	grab_ungrab_with_ignorable_modifiers (rootwin,
	                                      binding,
	                                      FALSE /* ungrab */);

	return TRUE;
}

static GdkFilterReturn
filter_func (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	GdkFilterReturn return_val = GDK_FILTER_CONTINUE;
	XEvent *xevent = (XEvent *) gdk_xevent;
	guint event_mods;
	GSList *iter;

	TRACE (g_print ("Got Event! %d, %d\n", xevent->type, event->type));

	switch (xevent->type) {
	case KeyPress:
		TRACE (g_print ("Got KeyPress! keycode: %d, modifiers: %d\n",
		                xevent->xkey.keycode,
		                xevent->xkey.state));

		/*
		 * Set the last event time for use when showing
		 * windows to avoid anti-focus-stealing code.
		 */
		processing_event = TRUE;
		last_event_time = xevent->xkey.time;

		event_mods = xevent->xkey.state & ~(GDK_MOD2_MASK | GDK_LOCK_MASK);

		for (iter = bindings; iter != NULL; iter = iter->next) {
			Binding *binding = (Binding *) iter->data;

			if (binding->keycode == xevent->xkey.keycode &&
			    binding->modifiers == event_mods) {

				TRACE (g_print ("Calling handler for '%s'...\n",
				                binding->keystring));

				(binding->handler) (binding->keystring,
				                    binding->user_data);
			}
		}

		processing_event = FALSE;
		break;
	case KeyRelease:
		TRACE (g_print ("Got KeyRelease! \n"));
		break;
	}

	return return_val;
}

static void
keymap_changed (GdkKeymap *map)
{
	GSList *iter;

	TRACE (g_print ("Keymap changed! Regrabbing keys..."));

	for (iter = bindings; iter != NULL; iter = iter->next) {
		Binding *binding = (Binding *) iter->data;
		do_ungrab_key (binding);
	}

	for (iter = bindings; iter != NULL; iter = iter->next) {
		Binding *binding = (Binding *) iter->data;
		do_grab_key (binding);
	}
}

void
tomboy_keybinder_init (void)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GdkWindow *rootwin = gdk_get_default_root_window ();

	gdk_window_add_filter (rootwin,
	                       filter_func,
	                       NULL);

	g_signal_connect (keymap,
	                  "keys_changed",
	                  G_CALLBACK (keymap_changed),
	                  NULL);
}

void
tomboy_keybinder_bind (const char           *keystring,
                       TomboyBindkeyHandler  handler,
                       gpointer              user_data)
{
	Binding *binding;
	gboolean success;

	binding = g_new0 (Binding, 1);
	binding->keystring = g_strdup (keystring);
	binding->handler = handler;
	binding->user_data = user_data;

	/* Sets the binding's keycode and modifiers */
	success = do_grab_key (binding);

	if (success) {
		bindings = g_slist_prepend (bindings, binding);
	} else {
		g_free (binding->keystring);
		g_free (binding);
	}
}

void
tomboy_keybinder_unbind (const char           *keystring,
                         TomboyBindkeyHandler  handler)
{
	GSList *iter;

	for (iter = bindings; iter != NULL; iter = iter->next) {
		Binding *binding = (Binding *) iter->data;

		if (strcmp (keystring, binding->keystring) != 0 ||
		    handler != binding->handler)
			continue;

		do_ungrab_key (binding);

		bindings = g_slist_remove (bindings, binding);

		g_free (binding->keystring);
		g_free (binding);
		break;
	}
}

guint32
tomboy_keybinder_get_current_event_time (void)
{
	if (processing_event)
		return last_event_time;
	else
		return GDK_CURRENT_TIME;
}
