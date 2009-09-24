/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <panel-applet-gconf.h>

#include <libtracker-client/tracker.h>

#include "tracker-results-window.h"
#include "tracker-aligned-window.h"

static void results_window_constructed (GObject *object);
static void results_window_finalize    (GObject *object);

static void results_window_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec);
static void results_window_get_property (GObject      *object,
					 guint         prop_id,
					 GValue       *value,
					 GParamSpec   *pspec);

static gboolean results_window_key_press_event (GtkWidget      *widget,
						GdkEventKey    *event);
static gboolean results_window_button_press_event (GtkWidget      *widget,
						   GdkEventButton *event);
static void     results_window_size_request    (GtkWidget      *widget,
						GtkRequisition *requisition);
static void     results_window_screen_changed  (GtkWidget      *widget,
						GdkScreen      *prev_screen);

static void     model_set_up      (TrackerResultsWindow *window);
static gboolean search_get        (TrackerResultsWindow *window,
				   const gchar          *query);


#define MUSIC_SEARCH    "SELECT ?urn ?type ?title ?belongs WHERE { ?urn a nmm:MusicPiece ; rdf:type ?type ; nfo:fileName ?title ; nfo:belongsToContainer ?belongs . ?urn fts:match \"%s*\" } OFFSET 0 LIMIT 500"
#define PHOTO_SEARCH    "SELECT ?urn ?type ?title ?belongs WHERE { ?urn a nmm:Photo ; rdf:type ?type ; nfo:fileName ?title ; nfo:belongsToContainer ?belongs . ?urn fts:match \"%s*\" } OFFSET 0 LIMIT 500"
#define VIDEO_SEARCH    "SELECT ?urn ?type ?title ?belongs WHERE { ?urn a nmm:Video ; rdf:type ?type ; nfo:fileName ?title ; nfo:belongsToContainer ?belongs . ?urn fts:match \"%s*\" } OFFSET 0 LIMIT 500"
#define DOCUMENT_SEARCH "SELECT ?urn ?type ?title ?belongs WHERE { ?urn a nfo:PaginatedTextDocument ; rdf:type ?type ; nfo:fileName ?title ; nfo:belongsToContainer ?belongs . ?urn fts:match \"%s*\" } OFFSET 0 LIMIT 500"
#define FOLDER_SEARCH   "SELECT ?urn ?type ?title ?belongs WHERE { ?urn a nfo:Folder ; rdf:type ?type ; nfo:fileName ?title ; nfo:belongsToContainer ?belongs . ?urn fts:match \"%s*\" } OFFSET 0 LIMIT 500"

#define GENERAL_SEARCH  "SELECT ?s ?type ?title WHERE { ?s fts:match \"%s*\" ; rdf:type ?type . OPTIONAL { ?s nie:title ?title } } OFFSET %d LIMIT %d"

#define TRACKER_RESULTS_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_RESULTS_WINDOW, TrackerResultsWindowPrivate))

typedef struct {
	GtkWidget *frame;
	GtkWidget *treeview;
	GObject *store;

	GtkIconTheme *icon_theme;

	TrackerClient *client;
	gchar *query;
} TrackerResultsWindowPrivate;

typedef enum {
	CATEGORY_NONE                  = 1 << 0,
	CATEGORY_CONTACT               = 1 << 1,
	CATEGORY_TAG                   = 1 << 2,
	CATEGORY_EMAIL_ADDRESS         = 1 << 3,
	CATEGORY_DOCUMENT              = 1 << 4,
	CATEGORY_APPLICATION           = 1 << 5,
	CATEGORY_IMAGE                 = 1 << 6,
	CATEGORY_AUDIO                 = 1 << 7,
	CATEGORY_FOLDER                = 1 << 8,
	CATEGORY_FONT                  = 1 << 9,
	CATEGORY_VIDEO                 = 1 << 10,
	CATEGORY_ARCHIVE               = 1 << 11,
	CATEGORY_WEBSITE               = 1 << 12
} TrackerCategory;

enum {
	COL_CATEGORY_ID,
	COL_CATEGORY,
	COL_IMAGE,
	COL_URN,
	COL_TITLE,
	COL_BELONGS,
	COL_COUNT
};

typedef struct {
	gchar *urn;
	gchar *type;
	gchar *title;
	gchar *belongs;
	guint categories;
} ItemData;

struct FindCategory {
	const gchar *category_str;
	gboolean found;
};

enum {
	PROP_0,
	PROP_QUERY
};

G_DEFINE_TYPE (TrackerResultsWindow, tracker_results_window, TRACKER_TYPE_ALIGNED_WINDOW)

static void
tracker_results_window_class_init (TrackerResultsWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = results_window_constructed;
	object_class->finalize = results_window_finalize;
	object_class->set_property = results_window_set_property;
	object_class->get_property = results_window_get_property;

	widget_class->key_press_event = results_window_key_press_event;
	widget_class->button_press_event = results_window_button_press_event;
	widget_class->size_request = results_window_size_request;
	widget_class->screen_changed = results_window_screen_changed;

	g_object_class_install_property (object_class,
					 PROP_QUERY,
					 g_param_spec_string ("query",
							      "Query",
							      "Query",
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (TrackerResultsWindowPrivate));
}

static void
launch_application_for_uri (GtkWidget   *widget,
			    const gchar *uri)
{
	GdkAppLaunchContext *launch_context;
	GdkScreen *screen;
	GError *error = NULL;

	launch_context = gdk_app_launch_context_new ();

	screen = gtk_widget_get_screen (widget);
	gdk_app_launch_context_set_screen (launch_context, screen);

	g_app_info_launch_default_for_uri (uri,
					   G_APP_LAUNCH_CONTEXT (launch_context),
					   &error);

	if (error) {
		g_critical ("Could not launch application for uri '%s': %s",
			    uri, error->message);
		g_error_free (error);
	}

	g_object_unref (launch_context);
}

static void
tree_view_row_activated_cb (GtkTreeView       *treeview,
			    GtkTreePath       *path,
			    GtkTreeViewColumn *column,
			    gpointer           user_data)
{
	TrackerResultsWindowPrivate *priv;
	TrackerResultsWindow *window;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *urn;

	window = user_data;
	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (window);
	model = GTK_TREE_MODEL (priv->store);

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return;
	}

	gtk_tree_model_get (model, &iter,
			    COL_URN, &urn,
			    -1);

	if (!urn) {
		return;
	}

	launch_application_for_uri (GTK_WIDGET (window), urn);
	gtk_widget_hide (GTK_WIDGET (window));

	g_free (urn);
}

static void
tracker_results_window_init (TrackerResultsWindow *window)
{
	TrackerResultsWindowPrivate *priv;
	GtkWidget *vbox;
	GtkWidget *scrolled_window;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (window);

	priv->client = tracker_connect (FALSE, G_MAXINT);

	priv->frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (window), priv->frame);
	gtk_frame_set_shadow_type (GTK_FRAME (priv->frame), GTK_SHADOW_IN);
	gtk_widget_set_size_request (priv->frame, 500, 300);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (priv->frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (vbox), scrolled_window);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	priv->treeview = gtk_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (scrolled_window), priv->treeview);
	g_signal_connect (priv->treeview, "row-activated",
			  G_CALLBACK (tree_view_row_activated_cb), window);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->treeview), FALSE);

	priv->icon_theme = gtk_icon_theme_get_default ();

	model_set_up (window);

	gtk_widget_show_all (priv->frame);
}

static void
results_window_constructed (GObject *object)
{
	TrackerResultsWindowPrivate *priv;
	TrackerResultsWindow *window;
	gchar *sparql;

	window = TRACKER_RESULTS_WINDOW (object);
	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (window);

	sparql = g_strdup_printf (MUSIC_SEARCH, priv->query);
	search_get (window, sparql);
	g_free (sparql);

	sparql = g_strdup_printf (PHOTO_SEARCH, priv->query);
	search_get (window, sparql);
	g_free (sparql);

	sparql = g_strdup_printf (VIDEO_SEARCH, priv->query);
	search_get (window, sparql);
	g_free (sparql);

	sparql = g_strdup_printf (DOCUMENT_SEARCH, priv->query);
	search_get (window, sparql);
	g_free (sparql);

	sparql = g_strdup_printf (FOLDER_SEARCH, priv->query);
	search_get (window, sparql);
	g_free (sparql);
}

static void
results_window_finalize (GObject *object)
{
	TrackerResultsWindowPrivate *priv;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (object);

	g_free (priv->query);

	if (priv->client) {
		tracker_disconnect (priv->client);
	}

	G_OBJECT_CLASS (tracker_results_window_parent_class)->finalize (object);
}

static void
results_window_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	TrackerResultsWindowPrivate *priv;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_QUERY:
		g_free (priv->query);
		priv->query = g_value_dup_string (value);
		break;
	default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
	}
}

static void
results_window_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	TrackerResultsWindowPrivate *priv;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_QUERY:
		g_value_set_string (value, priv->query);
		break;
	default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
	}
}

static gboolean
results_window_key_press_event (GtkWidget   *widget,
				GdkEventKey *event)
{
	TrackerResultsWindowPrivate *priv;

	if (event->keyval == GDK_Escape) {
		gtk_widget_hide (widget);

		return TRUE;
	}

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (widget);

	if (GTK_WIDGET_CLASS (tracker_results_window_parent_class)->key_press_event (widget, event)) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
results_window_button_press_event (GtkWidget      *widget,
				   GdkEventButton *event)
{
	if (event->x < 0 || event->x > widget->allocation.width ||
	    event->y < 0 || event->y > widget->allocation.height) {
		/* Click happened outside window, pop it down */
		gtk_widget_hide (widget);
		return TRUE;
	}

	if (GTK_WIDGET_CLASS (tracker_results_window_parent_class)->button_press_event (widget, event)) {
		return TRUE;
	}

	return FALSE;
}

static void
results_window_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
	GtkRequisition child_req;
	guint border_width;

	gtk_widget_size_request (GTK_BIN (widget)->child, &child_req);
	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

	requisition->width = child_req.width + (2 * border_width);
	requisition->height = child_req.height + (2 * border_width);

	if (GTK_WIDGET_REALIZED (widget)) {
		GdkScreen *screen;
		GdkRectangle monitor_geom;
		guint monitor_num;

		/* make it no larger than half the monitor size */
		screen = gtk_widget_get_screen (widget);
		monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);

		gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor_geom);

		requisition->width = MIN (requisition->width, monitor_geom.width / 2);
		requisition->height = MIN (requisition->height, monitor_geom.height / 2);
	}
}

static void
results_window_screen_changed (GtkWidget *widget,
			       GdkScreen *prev_screen)
{
	TrackerResultsWindowPrivate *priv;
	GdkScreen *screen;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (widget);

	if (priv->icon_theme) {
		priv->icon_theme = NULL;
	}

	screen = gtk_widget_get_screen (widget);

	if (screen) {
		priv->icon_theme = gtk_icon_theme_get_for_screen (screen);
		/* FIXME: trigger the model to update icons */
	}

	GTK_WIDGET_CLASS (tracker_results_window_parent_class)->screen_changed (widget, prev_screen);
}

static ItemData *
item_data_new (const gchar *urn,
	       const gchar *type,
	       const gchar *title,
	       const gchar *belongs,
	       guint        categories)
{
	ItemData *id;

	id = g_slice_new0 (ItemData);

	id->urn = g_strdup (urn);
	id->type = g_strdup (type);
	id->title = g_strdup (title);
	id->belongs = g_strdup (belongs);
	id->categories = categories;

	return id;
}

static void
item_data_free (ItemData *id)
{
	g_free (id->urn);
	g_free (id->type);
	g_free (id->title);
	g_free (id->belongs);
	
	g_slice_free (ItemData, id);
}

static gchar *
category_to_string (TrackerCategory category)
{
	switch (category) {
	case CATEGORY_CONTACT: return _("Contacts");
	case CATEGORY_TAG: return _("Tags");
	case CATEGORY_EMAIL_ADDRESS: return _("Email Addresses");
	case CATEGORY_DOCUMENT: return _("Documents");
	case CATEGORY_IMAGE: return _("Images");
	case CATEGORY_AUDIO: return _("Audio");
	case CATEGORY_FOLDER: return _("Folders");
	case CATEGORY_FONT: return _("Fonts");
	case CATEGORY_VIDEO: return _("Videos");
	case CATEGORY_ARCHIVE: return _("Archives");
	case CATEGORY_WEBSITE: return _("Websites");

	default:
		break;
	}

	return _("Other");
}

inline static void
category_from_string (const gchar *type,
		      guint       *categories)
{
	if (g_str_has_suffix (type, "nao#Tag")) {
		*categories |= CATEGORY_TAG;
	}

	if (g_str_has_suffix (type, "nfo#TextDocument") ||
	    g_str_has_suffix (type, "nfo#PaginatedTextDocument")) {
		*categories |= CATEGORY_DOCUMENT;
	}

	if (g_str_has_suffix (type, "nco#Contact")) {
		*categories |= CATEGORY_CONTACT;
	}

	if (g_str_has_suffix (type, "nco#EmailAddress")) {
		*categories |= CATEGORY_EMAIL_ADDRESS;
	}

	if (g_str_has_suffix (type, "nfo#Image") || 
	    g_str_has_suffix (type, "nfo#RosterImage") ||
	    g_str_has_suffix (type, "nfo#VectorImage") ||
	    g_str_has_suffix (type, "nfo#FilesystemImage")) {
		*categories |= CATEGORY_IMAGE;
	}

	if (g_str_has_suffix (type, "nfo#Audio") || 
	    g_str_has_suffix (type, "nmm#MusicPiece")) {
		*categories |= CATEGORY_AUDIO;
	}

	if (g_str_has_suffix (type, "nfo#Folder")) {
		*categories |= CATEGORY_FOLDER;
	}

	if (g_str_has_suffix (type, "nfo#Font")) {
		*categories |= CATEGORY_FONT;
	}

	if (g_str_has_suffix (type, "nfo#Video") ||
	    g_str_has_suffix (type, "nmm#Video")) {
		*categories |= CATEGORY_VIDEO;
	}

	if (g_str_has_suffix (type, "nfo#Archive")) {
		*categories |= CATEGORY_ARCHIVE;
	}

	if (g_str_has_suffix (type, "nfo#Website")) {
		*categories |= CATEGORY_WEBSITE;
	}

/*   http://www.semanticdesktop.org/ontologies/2007/01/19/nie#DataObject */
/*   http://www.semanticdesktop.org/ontologies/2007/01/19/nie#DataSource */
/*   http://www.semanticdesktop.org/ontologies/2007/01/19/nie#InformationElement */
/*   http://www.semanticdesktop.org/ontologies/2007/08/15/nao#Tag */
/*   http://www.semanticdesktop.org/ontologies/2007/08/15/nao#Property */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#Role */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#Affiliation */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#Contact */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#ContactGroup */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#ContactList */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#ContactMedium */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#EmailAddress */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#IMAccount */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#OrganizationContact */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#PersonContact */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#PhoneNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#PostalAddress */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#ModemNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#MessagingNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#PagerNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#Gender */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#VoicePhoneNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#VideoTelephoneNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#IsdnNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#ParcelDeliveryAddress */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#AudioIMAccount */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#FaxNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#CarPhoneNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#ContactListDataObject */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#PcsNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#InternationalDeliveryAddress */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#VideoIMAccount */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#BbsNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#CellPhoneNumber */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nco#DomesticDeliveryAddress */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Document */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#FileDataObject */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Software */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Media */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Visual */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Image */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#RasterImage */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#DataContainer */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#RemotePortAddress */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#MediaFileListEntry */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#VectorImage */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Audio */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#CompressionType */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Icon */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#TextDocument */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#PlainTextDocument */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#HtmlDocument */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#OperatingSystem */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#MediaList */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Executable */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Folder */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Font */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Filesystem */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#SoftwareService */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#SoftwareItem */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Presentation */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#RemoteDataObject */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#PaginatedTextDocument */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Video */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Spreadsheet */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Trash */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#FileHash */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#SourceCode */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Application */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#EmbeddedFileDataObject */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Attachment */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#ArchiveItem */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Archive */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#MindMap */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#MediaStream */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#BookmarkFolder */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#FilesystemImage */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#HardDiskPartition */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Cursor */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Bookmark */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#DeletedResource */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Website */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#WebHistory */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#SoftwareCategory */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#SoftwareApplication */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Orientation */
/*   http://www.tracker-project.org/ontologies/poi#ObjectOfInterest */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#MimePart */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#Multipart */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#Message */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#Email */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#Attachment */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#Mailbox */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#MailboxDataObject */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#MessageHeader */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#IMMessage */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#CommunicationChannel */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#PermanentChannel */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#TransientChannel */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#VOIPCall */
/*   http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#MailFolder */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#UnionParentClass */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#RecurrenceIdentifier */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#AttachmentEncoding */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#EventStatus */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#RecurrenceFrequency */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Attachment */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#AccessClassification */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#CalendarDataObject */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#JournalStatus */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#RecurrenceIdentifierRange */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#AttendeeOrOrganizer */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#AlarmAction */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#RecurrenceRule */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#TodoStatus */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#TimeTransparency */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#NcalTimeEntity */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#CalendarScale */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#AttendeeRole */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#BydayRulePart */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Weekday */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Trigger */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#FreebusyType */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#CalendarUserType */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#ParticipationStatus */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#RequestStatus */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#NcalDateTime */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#TimezoneObservance */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Organizer */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Attendee */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#NcalPeriod */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Calendar */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#FreebusyPeriod */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#TriggerRelation */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Alarm */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Event */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Todo */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Freebusy */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Journal */
/*   http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#Timezone */
/*   http://www.tracker-project.org/temp/scal#Calendar */
/*   http://www.tracker-project.org/temp/scal#CalendarItem */
/*   http://www.tracker-project.org/temp/scal#Attendee */
/*   http://www.tracker-project.org/temp/scal#AttendanceStatus */
/*   http://www.tracker-project.org/temp/scal#Event */
/*   http://www.tracker-project.org/temp/scal#Todo */
/*   http://www.tracker-project.org/temp/scal#Journal */
/*   http://www.tracker-project.org/temp/scal#CalendarAlarm */
/*   http://www.tracker-project.org/temp/scal#TimePoint */
/*   http://www.tracker-project.org/temp/scal#AccessLevel */
/*   http://www.tracker-project.org/temp/scal#RecurrenceRule */
/*   http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#ID3Audio */
/*   http://www.tracker-project.org/temp/nmm#MusicPiece */
/*   http://www.tracker-project.org/temp/nmm#SynchronizedText */
/*   http://www.tracker-project.org/temp/nmm#MusicAlbum */
/*   http://www.tracker-project.org/temp/nmm#Video */
/*   http://www.tracker-project.org/temp/nmm#Artist */
/*   http://www.tracker-project.org/temp/nmm#ImageList */
/*   http://www.tracker-project.org/temp/nmm#Photo */
/*   http://www.tracker-project.org/temp/nmm#Flash */
/*   http://www.tracker-project.org/temp/nmm#MeteringMode */
/*   http://www.tracker-project.org/temp/nmm#WhiteBalance */
/*   http://www.tracker-project.org/temp/nmm#RadioStation */
/*   http://www.tracker-project.org/temp/nmm#DigitalRadio */
/*   http://www.tracker-project.org/temp/nmm#AnalogRadio */
/*   http://www.tracker-project.org/temp/nmm#RadioModulation */
/*   http://www.tracker-project.org/temp/mto#TransferElement */
/*   http://www.tracker-project.org/temp/mto#Transfer */
/*   http://www.tracker-project.org/temp/mto#UploadTransfer */
/*   http://www.tracker-project.org/temp/mto#DownloadTransfer */
/*   http://www.tracker-project.org/temp/mto#SyncTransfer */
/*   http://www.tracker-project.org/temp/mto#State */
/*   http://www.tracker-project.org/temp/mto#TransferMethod */
/*   http://www.tracker-project.org/temp/mlo#GeoPoint */
/*   http://www.tracker-project.org/temp/mlo#PointOfInterest */
/*   http://www.tracker-project.org/temp/mlo#LocationBoundingBox */
/*   http://www.tracker-project.org/temp/mlo#Route */
/*   http://www.tracker-project.org/temp/mfo#FeedElement */
/*   http://www.tracker-project.org/temp/mfo#FeedChannel */
/*   http://www.tracker-project.org/temp/mfo#FeedMessage */
/*   http://www.tracker-project.org/temp/mfo#Enclosure */
/*   http://www.tracker-project.org/temp/mfo#FeedSettings */
/*   http://www.tracker-project.org/temp/mfo#Action */
/*   http://www.tracker-project.org/temp/mfo#FeedType */
/*   http://www.tracker-project.org/ontologies/tracker#Volume */
/*   http://maemo.org/ontologies/tracker#SoftwareWidget */
/*   http://maemo.org/ontologies/tracker#SoftwareApplet */
/*   http://maemo.org/ontologies/tracker#DesktopBookmark */

}

static GdkPixbuf *
pixbuf_get (TrackerResultsWindow *window,
	    const gchar          *urn,
	    gboolean              is_image)
{
	TrackerResultsWindowPrivate *priv;
	const gchar *attributes;
	GFile *file;
	GFileInfo *info;
        GIcon *icon;
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (window);
	file = g_file_new_for_uri (urn);

	if (is_image) {
		gchar *path;

		path = g_file_get_path (file);
		pixbuf = gdk_pixbuf_new_from_file_at_size (path, 24, 24, &error);
		g_free (path);

		if (error) {
			g_printerr ("Couldn't get pixbuf for urn:'%s', %s\n", 
				    urn,
				    error->message);
			g_error_free (error);
		} else {
			g_object_unref (file);
			return pixbuf;
		}

		/* In event of failure, get generic icon */
	}


	attributes = 
		G_FILE_ATTRIBUTE_STANDARD_ICON;
	
	info = g_file_query_info (file,
				  attributes,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  &error);


        if (error) {
		g_printerr ("Couldn't get pixbuf for urn:'%s', %s\n", 
			    urn,
			    error->message);
		g_object_unref (file);
                g_error_free (error);

                return NULL;
        }

        icon = g_file_info_get_icon (info);

        if (icon && G_IS_THEMED_ICON (icon)) {
                GtkIconInfo *icon_info;
		const gchar **names;

		names = (const gchar**) g_themed_icon_get_names (G_THEMED_ICON (icon));
		icon_info = gtk_icon_theme_choose_icon (priv->icon_theme,
                                                        names,
                                                        24,
							GTK_ICON_LOOKUP_USE_BUILTIN);

                if (icon_info) {
                        pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
                        gtk_icon_info_free (icon_info);
                }
        }

	g_object_unref (info);
	g_object_unref (file);

	return pixbuf;
}

static void
model_pixbuf_cell_data_func (GtkTreeViewColumn    *tree_column,
			     GtkCellRenderer      *cell,
			     GtkTreeModel         *model,
			     GtkTreeIter          *iter,
			     TrackerResultsWindow *window)
{
	GdkPixbuf *pixbuf = NULL;

	gtk_tree_model_get (model, iter,
			    COL_IMAGE, &pixbuf,
			    -1);

	if (!pixbuf) {
		TrackerCategory category = CATEGORY_NONE;
		gchar *urn;

		gtk_tree_model_get (model, iter,
				    COL_CATEGORY_ID, &category,
				    COL_URN, &urn,
				    -1);

		/* FIXME: Should use category */
		pixbuf = pixbuf_get (window, urn, (category & CATEGORY_IMAGE));
		g_free (urn);

		/* Cache it in the store */
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    COL_IMAGE, pixbuf,
				    -1);
	}

	g_object_set (cell,
		      "visible", (pixbuf != NULL),
		      "pixbuf", pixbuf,
		      NULL);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static void
model_set_up (TrackerResultsWindow *window)
{
	TrackerResultsWindowPrivate *priv;
	GtkTreeView *view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkListStore *store;
	GtkCellRenderer *cell;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (window);
	view = GTK_TREE_VIEW (priv->treeview);

	/* Store */
	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_INT,            /* Category ID */
				    G_TYPE_STRING,         /* Category */
				    GDK_TYPE_PIXBUF,       /* Image */
				    G_TYPE_STRING,         /* URN */
				    G_TYPE_STRING,         /* Title */
				    G_TYPE_STRING);        /* Belongs */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	/* Selection */ 
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	/* Column: Category */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Category"), cell, 
							   "text", COL_CATEGORY, 
							   NULL);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_sort_column_id (column, COL_CATEGORY_ID);
	gtk_tree_view_append_column (view, column);

	/* Column: Icon + Title */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 model_pixbuf_cell_data_func,
						 window,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", 4,
		      "ypad", 1,
		      NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column, cell, "text", COL_TITLE);

	gtk_tree_view_column_set_title (column, _("Title"));
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_column_set_sort_column_id (column, COL_TITLE);
	gtk_tree_view_append_column (view, column);

	/* Sorting */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_CATEGORY_ID,
					      GTK_SORT_ASCENDING);

	/* Tooltips */
	gtk_tree_view_set_tooltip_column (view, COL_BELONGS);

	/* Save */
	priv->store = G_OBJECT (store);
}

static gboolean
model_add_category_foreach (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    gpointer      data)
{
	struct FindCategory *fc = data;
	gchar *str;

	gtk_tree_model_get (model, iter, COL_CATEGORY, &str, -1);
	
	if (str && strcmp (fc->category_str, str) == 0) {
		fc->found = TRUE;
	}

	g_free (str);

	return fc->found;
}

static void
model_add (TrackerResultsWindow *window,
	   TrackerCategory       category,
	   const gchar          *urn,
	   const gchar          *title,
	   const gchar          *belongs)
{
	TrackerResultsWindowPrivate *priv;
	struct FindCategory fc;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	const gchar *category_str;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (window);
	pixbuf = NULL;

	/* Get category for id */
	category_str = category_to_string (category);

	/* Hack: */
	fc.category_str = category_str;
	fc.found = FALSE;

	gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
				model_add_category_foreach,
				&fc);

	gtk_list_store_append (GTK_LIST_STORE (priv->store), &iter);
	gtk_list_store_set (GTK_LIST_STORE (priv->store), &iter,
			    COL_CATEGORY_ID, category,
			    COL_CATEGORY, fc.found ? NULL : category_str,
			    COL_IMAGE, pixbuf ? pixbuf : NULL,
			    COL_URN, urn,
			    COL_TITLE, title,
			    COL_BELONGS, belongs,
			    -1);

	/* path = gtk_tree_model_get_path (GTK_TREE_MODEL (window->store), &iter); */
	/* gtk_tree_view_set_tooltip_row (GTK_TREE_VIEW (window->treeview), tooltip, path); */
	/* gtk_tree_path_free (path); */
		
	/* gtk_tree_selection_select_iter (selection, &iter); */
}

inline static void
search_get_foreach (gpointer value, 
		    gpointer user_data)
{
	GHashTable *resources;
	ItemData *id;
	gchar **metadata;
	const gchar *urn, *type, *title, *belongs;

	resources = user_data;
	metadata = value;

	urn = metadata[0];
	type = metadata[1];
	title = metadata[2];
	belongs = metadata[3];

	if (!title) {
		title = urn;
	}
	
	id = g_hash_table_lookup (resources, urn);
	if (!id) {
		g_print ("urn:'%s' found\n", urn);
		g_print ("  title:'%s'\n", title);
		g_print ("  belongs to:'%s'\n", belongs);

		id = item_data_new (urn, type, title, belongs, 0);
		g_hash_table_insert (resources, g_strdup (urn), id);
	}

	category_from_string (type, &id->categories);

	g_print ("  type:'%s', new categories:%d\n", type, id->categories);
}

static gboolean
search_get (TrackerResultsWindow *window,
	    const gchar          *query)
{
	TrackerResultsWindowPrivate *priv;
	GError *error = NULL;
	GPtrArray *results;

	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (window);
	results = tracker_resources_sparql_query (priv->client, query, &error);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get search results"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No results were found matching your query"));
	} else {
		GHashTable *resources;
		GHashTableIter iter;
		gpointer key, value;

		g_print ("Results: %d\n", results->len);

		resources = g_hash_table_new_full (g_str_hash,
						   g_str_equal,
						   (GDestroyNotify) g_free,
						   (GDestroyNotify) item_data_free);
		
		g_ptr_array_foreach (results,
				     search_get_foreach,
				     resources);

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);

		g_hash_table_iter_init (&iter, resources);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			ItemData *id;

			id = value;
			
			model_add (window, 
				   id->categories, 
				   id->urn, 
				   id->title,
				   id->belongs);
		}

		g_hash_table_unref (resources);
	}

	return TRUE;
}

GtkWidget *
tracker_results_window_new (GtkWidget   *parent,
			    const gchar *query)
{
	return g_object_new (TRACKER_TYPE_RESULTS_WINDOW,
			     "align-widget", parent,
			     "query", query,
			     NULL);
}

static gboolean
grab_popup_window (TrackerResultsWindow *window)
{
	TrackerResultsWindowPrivate *priv;
	GdkGrabStatus status;
	GtkWidget *widget;
	guint32 time;

	widget = GTK_WIDGET (window);
	time = gtk_get_current_event_time ();
	priv = TRACKER_RESULTS_WINDOW_GET_PRIVATE (window);

	/* Grab pointer */
	status = gdk_pointer_grab (widget->window,
				   TRUE,
				   GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
				   NULL, NULL,
				   time);

	if (status == GDK_GRAB_SUCCESS) {
		status = gdk_keyboard_grab (widget->window, TRUE, time);
	}

	if (status == GDK_GRAB_SUCCESS) {
		gtk_widget_grab_focus (priv->treeview);
	} else if (status == GDK_GRAB_NOT_VIEWABLE) {
		/* window is not viewable yet, retry */
		return TRUE;
	} else {
		gtk_widget_hide (widget);
	}

	return FALSE;
}

void
tracker_results_window_popup (TrackerResultsWindow *window)
{
	g_return_if_fail (TRACKER_IS_RESULTS_WINDOW (window));

	gtk_widget_show (GTK_WIDGET (window));

	g_idle_add ((GSourceFunc) grab_popup_window, window);
}
