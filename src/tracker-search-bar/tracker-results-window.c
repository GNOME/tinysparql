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

typedef struct {
	GtkWidget *window;

	GtkWidget *vbox;
	GtkWidget *treeview;
	GObject *store;

	GtkIconTheme *icon_theme;

	TrackerClient *client;
} TrackerResultsWindow;

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
	COL_COUNT
};

struct SearchHashTables {
	GHashTable *resources;
	GHashTable *titles;
};

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
	    const gchar          *urn)
{
	const gchar *attributes;
	GFile *file;
	GFileInfo *info;
        GIcon *icon;
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;
	
	file = g_file_new_for_uri (urn);

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
		icon_info = gtk_icon_theme_choose_icon (window->icon_theme,
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
	TrackerCategory category = CATEGORY_NONE;
	gchar *urn;
	GdkPixbuf *pixbuf = NULL;

	gtk_tree_model_get (model, iter,
			    COL_IMAGE, &pixbuf,
			    -1);

	/* If a pixbuf, use it */
	if (pixbuf) {
		g_object_set (cell,
			      "visible", TRUE,
			      "pixbuf", pixbuf,
			      NULL);
		g_object_unref (pixbuf);

		return;
	}

	gtk_tree_model_get (model, iter,
			    COL_CATEGORY_ID, &category,
			    COL_URN, &urn,
			    -1);
	
	/* FIXME: Should use category */
	pixbuf = pixbuf_get (window, urn);
	g_free (urn);
	
	g_object_set (cell,
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL);
	
	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static void
model_set_up (TrackerResultsWindow *window)
{
	GtkTreeView *view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkListStore *store;
	GtkCellRenderer *cell;

	view = GTK_TREE_VIEW (window->treeview);

	/* Store */
	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_INT,            /* Category ID */
				    G_TYPE_STRING,         /* Category */
				    GDK_TYPE_PIXBUF,       /* Image */
				    G_TYPE_STRING,         /* URN */
				    G_TYPE_STRING);        /* Title */

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

	/* Save */
	window->store = G_OBJECT (store);
}

static void
model_add (TrackerResultsWindow *window,
	   TrackerCategory       category,
	   const gchar          *urn,
	   const gchar          *title) 
{
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	const gchar *category_str;

	/* Get Pixbuf for category */
	pixbuf = NULL;
	
	/* Get category for id */
	category_str = category_to_string (category);

	gtk_list_store_append (GTK_LIST_STORE (window->store), &iter);
	gtk_list_store_set (GTK_LIST_STORE (window->store), &iter,
			    COL_CATEGORY_ID, category,
			    COL_CATEGORY, category_str,
			    COL_IMAGE, pixbuf ? pixbuf : NULL,
			    COL_URN, urn,
			    COL_TITLE, title,
			    -1);

	/* gtk_tree_selection_select_iter (selection, &iter); */
}

static gboolean
window_key_press_event_cb (GtkWidget   *widget,
			   GdkEventKey *event,
			   gpointer     user_data)
{
	TrackerApplet *applet = user_data;
	
	if (event->keyval == GDK_Escape) {
		gtk_widget_hide (widget);
		
		return TRUE;
	} else if ((event->keyval == GDK_l) && (event->state & GDK_CONTROL_MASK)) {
		gtk_widget_grab_focus (applet->entry);
		
		return TRUE;
	}
	
	return FALSE;
}

static void
window_show_cb (GtkWidget            *widget_to_show,
		TrackerResultsWindow *window)
{
	GtkWidget *widget;
	gint width, height;
	GdkScreen *screen;
	gint monitor_num;
	GtkRequisition req;
	GdkRectangle monitor;

	/* if (!priv->window) { */
	/* 	return; */
	/* } */
  
	widget = window->window;

#if 0
	gint font_size;

	/* Size based on the font size */
	font_size = pango_font_description_get_size (defbox->style->font_desc);
	font_size = PANGO_PIXELS (font_size);

	width = font_size * WINDOW_NUM_COLUMNS;
	height = font_size * WINDOW_NUM_ROWS;
#endif

	/* Use at least the requisition size of the window... */
	gtk_widget_size_request (widget, &req);
	width = MAX (width, req.width);
	height = MAX (height, req.height);

	/* ... but make it no larger than half the monitor size */
	screen = gtk_widget_get_screen (widget);
	monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);

	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	width = MIN (width, monitor.width / 2);
	height = MIN (height, monitor.height / 2);

	/* Set size */
	gtk_widget_set_size_request (GTK_WIDGET (window->vbox), width, height);
}

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent  *event,
			gpointer   user_data)
{
	TrackerResultsWindow *window = user_data;

	window = user_data;
	
	if (window->client) {
		tracker_disconnect (window->client);
	}

	g_free (window);

	return FALSE;
}

inline static void
search_get_foreach (gpointer value, 
		    gpointer user_data)
{
	struct SearchHashTables *sht;
	gchar **metadata;
	gpointer p;
	guint new_categories, old_categories;
	const gchar *urn, *type, *title;

	sht = user_data;
	metadata = value;

	urn = metadata[0];
	type = metadata[1];
	title = metadata[2];
	
	p = g_hash_table_lookup (sht->resources, urn);
	new_categories = old_categories = GPOINTER_TO_UINT (p);

	category_from_string (type, &new_categories);

	if (old_categories != new_categories) {
		g_hash_table_replace (sht->resources, 
				      g_strdup (urn),
				      GUINT_TO_POINTER (new_categories));
	}

	g_hash_table_replace (sht->titles, 
			      g_strdup (urn),
			      g_strdup (title));

	g_print ("title:'%s' found with categories:%d\n", title, new_categories);
}

static gboolean
search_get (TrackerResultsWindow *window,
	    const gchar          *search_phrase,
	    gint                  search_offset,
	    gint                  search_limit,
	    gboolean              detailed_results)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *query;

	if (detailed_results) {
		query = g_strdup_printf ("SELECT ?s ?type ?title WHERE { ?s fts:match \"%s*\" ; rdf:type ?type; nie:title ?title }"
					 "OFFSET %d LIMIT %d",
					 search_phrase, 
					 search_offset, 
					 search_limit);
		/* query = g_strdup_printf ("SELECT ?s ?type ?mimeType WHERE { ?s fts:match \"%s\" ; rdf:type ?type . " */
		/* 			 "OPTIONAL { ?s nie:mimeType ?mimeType } } OFFSET %d LIMIT %d", */
		/* 			 search_phrase,  */
		/* 			 search_offset,  */
		/* 			 search_limit); */
	} else {
		query = g_strdup_printf ("SELECT ?s WHERE { ?s fts:match \"%s\" } OFFSET %d LIMIT %d",
					 search_phrase, 
					 search_offset, 
					 search_limit);
	}

	results = tracker_resources_sparql_query (window->client, query, &error);
	g_free (query);

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
		struct SearchHashTables sht;
		GHashTableIter iter;
		gpointer key, value;

		g_print ("Results: %d\n", results->len);

		sht.resources = g_hash_table_new_full (g_str_hash,
						       g_str_equal,
						       (GDestroyNotify) g_free,
						       NULL);
		sht.titles = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    (GDestroyNotify) g_free,
						    (GDestroyNotify) g_free);
		
		g_ptr_array_foreach (results,
				     search_get_foreach,
				     &sht);

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);

		g_hash_table_iter_init (&iter, sht.resources);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			const gchar *urn;
			const gchar *title;
			guint categories;

			urn = key;
			title = g_hash_table_lookup (sht.titles, urn);
			categories = GPOINTER_TO_UINT (value);
			
			model_add (window, categories, urn, title);
		}

		g_hash_table_unref (sht.resources);
		g_hash_table_unref (sht.titles);
	}

	return TRUE;
}

GtkWidget *
tracker_results_window_new (TrackerApplet *applet,
			    const gchar   *query)
{
	TrackerResultsWindow *window;
	GtkWidget *scrolled_window;
	GdkScreen *screen;
	
	g_return_val_if_fail (applet != NULL, NULL);

	window = g_new0 (TrackerResultsWindow, 1);

	window->client = tracker_connect (FALSE, G_MAXINT);

	/* window->window = gtk_builder_get_object (builder, "window_results"); */
	
	window->window = tracker_aligned_window_new (applet->parent);

	window->vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (window->window), window->vbox);
	gtk_widget_set_size_request (window->vbox, 400, 250);
	
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (window->vbox), scrolled_window);

	window->treeview = gtk_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (scrolled_window), window->treeview);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (window->treeview), FALSE);

	g_signal_connect (window->window, "key-press-event",
			  G_CALLBACK (window_key_press_event_cb),
			  window);
	g_signal_connect (window->window, "show",
			  G_CALLBACK (window_show_cb),
			  window);
	g_signal_connect (window->window, "delete-event", 
			  G_CALLBACK (window_delete_event_cb), window);
	
	screen = gtk_widget_get_screen (window->treeview);
	window->icon_theme = gtk_icon_theme_get_for_screen (screen);

	model_set_up (window);

	gtk_widget_show_all (GTK_WIDGET (window->window));

	search_get (window, 
		    query,
		    0,
		    512,
		    TRUE);

	return GTK_WIDGET (window->window);
}
