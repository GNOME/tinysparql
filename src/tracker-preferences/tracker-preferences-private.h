#ifndef __TRACKER_PREFERENCES_PRIVATE_H__
#define __TRACKER_PREFERENCES_PRIVATE_H__

#include <glade/glade.h>

#define TRACKER_PREFERENCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_PREFERENCES, TrackerPreferencesPrivate))

typedef struct _TrackerPreferencesPrivate {
	GladeXML *gxml;
	TrackerConfiguration *prefs;
} TrackerPreferencesPrivate;

static void
tracker_preferences_class_init (TrackerPreferencesClass * klass);

static void
tracker_preferences_init (GTypeInstance * instance, gpointer g_class);

static void
tracker_preferences_finalize (GObject * object);

static void
setup_page_general (TrackerPreferences * preferences);

static void
setup_page_files (TrackerPreferences * preferences);

static void
setup_page_emails (TrackerPreferences * preferences);


static void
setup_page_ignored_files (TrackerPreferences * preferences);

static void
setup_page_performance (TrackerPreferences * preferences);

static void
dlgPreferences_Quit (GtkWidget * widget, GdkEvent * event, gpointer data);

static void
cmdHelp_Clicked (GtkWidget * widget, gpointer data);

static void
cmdClose_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddIndexPath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveIndexPath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddCrawledPath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveCrawledPath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddIndexMailbox_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveIndexMailbox_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddIgnorePath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveIgnorePath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddIgnorePattern_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveIgnorePattern_Clicked (GtkWidget * widget, gpointer data);


static void
append_item_to_list (TrackerPreferences * dialog, const gchar * const item,
		     const gchar * const widget);

static void
remove_selection_from_list (TrackerPreferences * dialog,
			    const gchar * const widget);

static GSList *
treeview_get_values (GtkTreeView * treeview);

static gint
_strcmp (gconstpointer a, gconstpointer b);

static void
initialize_listview (GtkWidget * treeview);

static void
populate_list (GtkWidget * treeview, GSList * list);

#endif
