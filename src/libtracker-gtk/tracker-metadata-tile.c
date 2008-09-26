/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2007 Neil Jagdish Patel <njpatel@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author : Neil Jagdish Patel <njpatel@gmail.com>
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib/gi18n-lib.h>

#include "tracker-metadata-tile.h"
#include "tracker-tag-bar.h"

#define TRACKER_METADATA_TILE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_METADATA_TILE, TrackerMetadataTilePrivate))

G_DEFINE_TYPE (TrackerMetadataTile, tracker_metadata_tile, GTK_TYPE_EVENT_BOX)

struct TrackerMetadataTilePrivate {
	TrackerClient *client;

	gboolean visible;
	gboolean lock;

	gboolean expanded;
	gchar *type;
	GdkPixbuf *preview;

	GtkWidget *align;

	GtkWidget *image;
	GtkWidget *size;

	GtkWidget *expander;
	GtkWidget *arrow;
	GtkWidget *title;

	GtkWidget *table;

	GtkWidget *info1;
	GtkWidget *info2;

	GtkWidget *info3;
	GtkWidget *info4;

	GtkWidget *info5;
	GtkWidget *info6;

	GtkWidget *tag_bar;
};

static GObjectClass *parent_class = NULL;

#define KILOBYTE_FACTOR 1024.0
#define MEGABYTE_FACTOR (1024.0 * 1024.0)
#define GIGABYTE_FACTOR (1024.0 * 1024.0 * 1024.0)

/* forward declerations */
static void  tracker_metadata_tile_class_init	    (TrackerMetadataTileClass *class);
static void  tracker_metadata_tile_init		    (TrackerMetadataTile      *tile);
static gboolean tracker_metadata_tile_expose_event(GtkWidget *widget, GdkEventExpose *event);
static void tracker_metadata_tile_show (TrackerMetadataTile *tile);
static void _property_to_label (GtkWidget *label, const char *prop, const char *string);
static void _date_to_label (GtkWidget *label, const char *prop, const char *string);
static void _year_to_label (GtkWidget *label, const char *prop, const char *string);
static void _size_to_label (GtkWidget *label, const char *prop, const char *string);
static void _dimensions_to_label (GtkWidget *label, const char *width, const char *height, const char *string);
static void _seconds_to_label (GtkWidget *label, const char *prop, const char *string);
static void _bitrate_to_label (GtkWidget *label, const char *prop, const char *string);
static void _int_to_label (GtkWidget *label, const char *prop, const char *string);

static inline gboolean is_empty_string (const char *s);

/* structs & enums */

static char *default_keys[] =
{
	"File:Name",
	"File:Path",
	"File:Modified",
	"File:Size",
	"File:Accessed",
	"File:Mime",
	NULL
};

enum {
	DEFAULT_NAME,
	DEFAULT_PATH,
	DEFAULT_MODIFIED,
	DEFAULT_SIZE,
	DEFAULT_ACCESSED,
	DEFAULT_MIME,
	DEFAULT_N_KEYS
};

static char *doc_keys[] =
{
	"File:Name",
	"Doc:Subject",
	"Doc:Author",
	"Doc:Comments",
	"Doc:PageCount",
	"Doc:WordCount",
	"Doc:Created",
	NULL
};

enum {
	DOC_NAME,
	DOC_SUBJECT,
	DOC_AUTHOR,
	DOC_COMMENTS,
	DOC_PAGECOUNT,
	DOC_WORDCOUNT,
	DOC_CREATED,
	DOC_N_KEYS
};



static char *email_keys[] =
{
	"Email:Sender",
	"Email:Subject",
	"Email:Date",
	"Email:SentTo",
	"Email:CC",
	"Email:Attachments",
	NULL
};

enum {
	EMAIL_SENDER,
	EMAIL_SUBJECT,
	EMAIL_DATE,
	EMAIL_SENTTO,
	EMAIL_CC,
	EMAIL_ATTACHMENTS,
	EMAIL_N_KEYS
};

static char *webhistory_keys[] =
{
	"Doc:URL",
	"Doc:Title",
	"File:Size",
	"File:Mime",
	"Doc:Keywords",
	NULL
};

enum {
	WEBHISTORY_URL,
	WEBHISTORY_TITLE,
	WEBHISTORY_SIZE,
	WEBHISTORY_MIME,
	WEBHISTORY_KEYWORDS,
	WEBHISTORY_N_KEYS
};


static char *app_keys[] =
{
	"App:DisplayName",
	"App:GenericName",
	"App:Comment",
	"App:Categories",
	NULL
};

enum {
	APP_DISPLAYNAME,
	APP_GENERIC_NAME,
	APP_COMMENT,
	APP_CATEGORIES,
	APP_N_KEYS
};


static char *audio_keys[] =
{
	"Audio:Title",
	"Audio:Artist",
	"Audio:Album",
	"Audio:Duration",
	"Audio:Genre",
	"Audio:Bitrate",
	"Audio:ReleaseDate",
	"Audio:Codec",
	"File:Size",
	NULL
};

enum {
	AUDIO_TITLE,
	AUDIO_ARTIST,
	AUDIO_ALBUM,
	AUDIO_DURATION,
	AUDIO_GENRE,
	AUDIO_BITRATE,
	AUDIO_RELEASEDATE,
	AUDIO_CODEC,
	AUDIO_SIZE,
	AUDIO_N_KEYS
};

static char *image_keys[] =
{
	"File:Name",
	"Image:Height",
	"Image:Width",
	"Image:Date",
	"Image:CameraMake",
	"Image:CameraModel",
	"Image:Orientation",
	"Image:Flash",
	"Image:FocalLength",
	"Image:ExposureTime",
	NULL
};

enum {
	IMAGE_TITLE,
	IMAGE_HEIGHT,
	IMAGE_WIDTH,
	IMAGE_DATE,
	IMAGE_CAMERA,
	IMAGE_MODEL,
	IMAGE_ORIENT,
	IMAGE_FLASH,
	IMAGE_FOCAL,
	IMAGE_EXPO,
	IMAGE_N_KEYS
};

static char *video_keys[] =
{
	"File:Name",
	"Video:Height",
	"Video:Width",
	"Video:Author",
	"Video:FrameRate",
	"Video:Codec",
	"Video:Bitrate",
	"Video:Duration",
	NULL
};

enum {
	VIDEO_TITLE,
	VIDEO_HEIGHT,
	VIDEO_WIDTH,
	VIDEO_AUTHOR,
	VIDEO_FRAMERATE,
	VIDEO_CODEC,
	VIDEO_BITRATE,
	VIDEO_DURATION,
	VIDEO_N_KEYS
};

static inline gboolean
is_empty_string (const char *s)
{
	return s == NULL || s[0] == '\0';
}


static void
_show_labels (TrackerMetadataTile *tile, gboolean label_visible)
{
	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	if (!label_visible) {
		gtk_widget_hide (priv->info1);
		gtk_widget_hide (priv->info2);
		gtk_widget_hide (priv->info3);
		gtk_widget_hide (priv->info4);
		gtk_widget_hide (priv->info5);
		gtk_widget_hide (priv->info6);
	} else {
		gtk_widget_show (priv->info1);
		gtk_widget_show (priv->info2);
		gtk_widget_show (priv->info3);
		gtk_widget_show (priv->info4);
		gtk_widget_show (priv->info5);
		gtk_widget_show (priv->info6);
	}
}

/* populates the metadata tile for a default file */
static void
_tile_tracker_populate_default (char **array, GError *error, TrackerMetadataTile *tile )
{
	if (error) {
		g_print ("Error : %s\n", error->message);
		g_clear_error (&error);
		gtk_widget_hide (GTK_WIDGET(tile));
		return;
	}

	/* create title */

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);	/* create title */

	_property_to_label ( priv->title, array[DEFAULT_NAME] , "<span size='large'><b>%s</b></span>");

	/* then set the remaining properties */
	_property_to_label ( priv->info1, array[DEFAULT_PATH] , _("Path : <b>%s</b>"));
	_date_to_label ( priv->info2, array[DEFAULT_MODIFIED] , _("Modified : <b>%s</b>"));
	_size_to_label ( priv->info3, array[DEFAULT_SIZE] , _("Size : <b>%s</b>"));
	_date_to_label ( priv->info4, array[DEFAULT_ACCESSED] , _("Accessed : <b>%s</b>"));
	_property_to_label ( priv->info5, array[DEFAULT_MIME] , _("Mime : <b>%s</b>"));
	_property_to_label ( priv->info6, " " , "%s");



	tracker_metadata_tile_show (tile);
	g_strfreev (array);
}


static void
_tile_tracker_populate_blank (TrackerMetadataTile *tile)
{
	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	_show_labels (tile, FALSE);

	/* create title */
	_property_to_label ( priv->title, " " , "%s");
	_property_to_label ( priv->info1, " " , "%s");
	_property_to_label ( priv->info2, " " , "%s");
	_property_to_label ( priv->info3, " " , "%s");
	_property_to_label ( priv->info4, " " , "%s");
	_property_to_label ( priv->info5, " " , "%s");
	_property_to_label ( priv->info6, " " , "%s");

	tracker_metadata_tile_show (tile);
}


static void
_tile_tracker_populate_email (char **array, GError *error, TrackerMetadataTile *tile)
{
	/* format properties */
	if (error) {
		g_print ("META_TILE_ERROR : %s", error->message);
		g_clear_error (&error);
		/* hide widget */
		gtk_widget_hide (GTK_WIDGET(tile));
		return;
	}

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);


	/* create title */
	_property_to_label ( priv->title, array[EMAIL_SUBJECT] , "<span size='large'><b>%s</b></span>");

	_property_to_label ( priv->info1, array[EMAIL_SENDER] , _("Sender : <b>%s</b>"));
	_date_to_label ( priv->info2, array[EMAIL_DATE] , _("Date : <b>%s</b>"));
	_property_to_label ( priv->info3, " " , "%s");
	_property_to_label ( priv->info4, " " , "%s");
	_property_to_label ( priv->info5, " " , "%s");
	_property_to_label ( priv->info6, " " , "%s");


	/* free properties */
	g_strfreev (array);

	tracker_metadata_tile_show (tile);

	_show_labels (tile, FALSE);
	gtk_widget_show (priv->info1);
	gtk_widget_show (priv->info2);
}


static void
_tile_tracker_populate_applications (char **array, GError *error, TrackerMetadataTile *tile)
{
	/* format properties */
	if (error) {
		g_print ("META_TILE_ERROR : %s", error->message);
		g_clear_error (&error);
		/* hide widget */
		gtk_widget_hide (GTK_WIDGET(tile));
		return;
	}

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);



	/* create title */
	_property_to_label ( priv->title, array[APP_DISPLAYNAME] , "<span size='large'><b>%s</b></span>");


	_property_to_label ( priv->info1, array[APP_COMMENT] , _("Comment : <b>%s</b>"));
	_property_to_label ( priv->info2, array[APP_CATEGORIES] , _("Categories : <b>%s</b>"));
	_property_to_label ( priv->info4, " " , "%s");
	_property_to_label ( priv->info5, " " , "%s");
	_property_to_label ( priv->info6, " " , "%s");


	/* free properties */
	g_strfreev (array);

	tracker_metadata_tile_show (tile);
	_show_labels (tile, FALSE);
	gtk_widget_show (priv->info1);
	gtk_widget_show (priv->info2);

}



static void
_tile_tracker_populate_audio (char **array, GError *error, TrackerMetadataTile *tile)
{
	/* format properties */
	if (error) {
		g_print ("META_TILE_ERROR : %s", error->message);
		g_clear_error (&error);
		/* hide widget */
		gtk_widget_hide (GTK_WIDGET(tile));
		return;
	}

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);	/* create title */

	char *prop = NULL;
	/* make nice looking description */
	GString *str;
	gboolean artist = FALSE;
	gboolean album = FALSE;

	str = g_string_new ("<span size='large'><b>%s</b></span>");

	if (!is_empty_string (array[AUDIO_ARTIST])) {
		artist = TRUE;
		str = g_string_append (str, " by <span size='large'><i>%s</i></span>");
	}

	if (!is_empty_string (array[AUDIO_ALBUM])) {
		album = TRUE;
		str = g_string_append (str, " from <span size='large'><i>%s</i></span>");
	}

	if (artist && album) {
		char *temp1, *temp2, *temp3;
		temp1 = g_markup_escape_text (array[AUDIO_TITLE], -1);
		temp2 = g_markup_escape_text (array[AUDIO_ARTIST], -1);
		temp3 = g_markup_escape_text (array[AUDIO_ALBUM], -1);
		prop = g_strdup_printf (str->str, temp1, temp2, temp3);
		g_free (temp1);
		g_free (temp2);
		g_free (temp3);
	} else if (artist) {
		char *temp1, *temp2;
		temp1 = g_markup_escape_text (array[AUDIO_TITLE], -1);
		temp2 = g_markup_escape_text (array[AUDIO_ARTIST], -1);
		prop = g_strdup_printf (str->str, temp1, temp2);
		g_free (temp1);
		g_free (temp2);
	} else if (album) {
		char *temp1, *temp2;
		temp1 = g_markup_escape_text (array[AUDIO_TITLE], -1);
		temp2 = g_markup_escape_text (array[AUDIO_ALBUM], -1);
		prop = g_strdup_printf (str->str, temp1, temp2);
		g_free (temp1);
		g_free (temp2);
	} else {
		char *temp1;
		temp1 = g_markup_escape_text (array[AUDIO_TITLE], -1);
		prop = g_strdup_printf (str->str, temp1);
		g_free (temp1);
	}

	gtk_label_set_markup (GTK_LABEL (priv->title), prop);
	g_free (prop);
	g_string_free (str, TRUE);

	_seconds_to_label ( priv->info1, array[AUDIO_DURATION] , _("Duration : <b>%s</b>"));
	_property_to_label ( priv->info2, array[AUDIO_GENRE] , _("Genre : <b>%s</b>"));
	_bitrate_to_label ( priv->info3, array[AUDIO_BITRATE] , _("Bitrate : <b>%s Kbs</b>"));
	_year_to_label ( priv->info4, array[AUDIO_RELEASEDATE] , _("Year : <b>%s</b>"));
	_size_to_label ( priv->info5, array[AUDIO_SIZE] , _("Size : <b>%s</b>"));
	_property_to_label ( priv->info6, array[AUDIO_CODEC] , _("Codec : <b>%s</b>"));




	/* free properties */
	g_strfreev (array);

	tracker_metadata_tile_show (tile);
}

/* populates the metadata tile for a image file */
static void
_tile_tracker_populate_image (char **array, GError *error, TrackerMetadataTile *tile )
{
	if (error) {
		g_print ("METADATA_TILE_ERROR : %s", error->message);
		g_clear_error (&error);
		gtk_widget_hide (GTK_WIDGET(tile));
		return;
	}

	/* TODO : check for a normal image file, not all images are photos */

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);	/* create title */

	/* create title */
	_property_to_label ( priv->title, array[IMAGE_TITLE] , "<span size='large'><b>%s</b></span>");

	char *prop = NULL;
	/* make nice looking description */
	GString *str;
	gboolean camera = TRUE;
	gboolean model = TRUE;

	str = g_string_new ("<span size='large'><b>%s</b></span>");

	if (!is_empty_string (array[IMAGE_CAMERA])) {
		camera = TRUE;
		str = g_string_append (str, _(" taken with a <span size='large'><i>%s</i></span>"));
	}

	if (!is_empty_string (array[IMAGE_MODEL])) {
		model = TRUE;
		str = g_string_append (str, _(" <span size='large'><i>%s</i></span>"));
	}
	if (camera && model) {
		prop = g_strdup_printf (str->str, g_markup_escape_text (array[IMAGE_TITLE], -1),
						  g_markup_escape_text (array[IMAGE_CAMERA], -1),
						  g_markup_escape_text (array[IMAGE_MODEL], -1));
	} else if (camera) {
		prop = g_strdup_printf (str->str, g_markup_escape_text (array[IMAGE_TITLE], -1),
						  g_markup_escape_text (array[IMAGE_CAMERA], -1));
	} else if (model) {
		prop = g_strdup_printf (str->str, g_markup_escape_text (array[IMAGE_TITLE], -1),
						  g_markup_escape_text (array[IMAGE_MODEL], -1));
	} else {
		prop = g_strdup_printf (str->str, g_markup_escape_text (array[IMAGE_TITLE], -1));
	}
	gtk_label_set_markup (GTK_LABEL (priv->title), prop);
	g_free (prop);
	g_string_free (str, TRUE);

	/* then set the remaining properties */
	_dimensions_to_label ( priv->info1, array[IMAGE_WIDTH], array[IMAGE_HEIGHT] , _("Dimensions : <b>%d x %d</b>"));
	_date_to_label ( priv->info2, array[IMAGE_DATE] , _("Date Taken : <b>%s</b>"));
	_property_to_label ( priv->info3, array[IMAGE_ORIENT] , _("Orientation : <b>%s</b>"));
	_property_to_label ( priv->info4, array[IMAGE_FLASH] , _("Flash : <b>%s</b>"));
	_property_to_label ( priv->info5, array[IMAGE_FOCAL] , _("Focal Length : <b>%s</b>"));
	_property_to_label ( priv->info6, array[IMAGE_EXPO] , _("Exposure Time : <b>%s</b>"));




	tracker_metadata_tile_show (tile);
	g_strfreev (array);
}

/* populates the metadata tile for a video file */
static void
_tile_tracker_populate_video (char **array, GError *error, TrackerMetadataTile *tile )
{
	if (error) {
		g_print ("METADATA_TILE_ERROR : %s", error->message);
		g_clear_error (&error);
		gtk_widget_hide (GTK_WIDGET(tile));
		return;
	}

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);	/* create title */

	_property_to_label ( priv->title, array[VIDEO_TITLE] , "<span size='large'><b>%s</b></span>");

	/* then set the remaining properties */
	_dimensions_to_label ( priv->info1, array[VIDEO_WIDTH], array[VIDEO_HEIGHT] , _("Dimensions : <b>%d x %d</b>"));
	_property_to_label ( priv->info2, array[VIDEO_AUTHOR] , _("Author : <b>%s</b>"));
	_seconds_to_label ( priv->info3, array[VIDEO_DURATION] , _("Duration : <b>%s</b>"));
	_property_to_label ( priv->info4, array[VIDEO_BITRATE] , _("Bitrate : <b>%s</b>"));
	_property_to_label ( priv->info5, array[VIDEO_CODEC] , _("Encoded In : <b>%s</b>"));
	_property_to_label ( priv->info6, array[VIDEO_FRAMERATE] , _("Framerate : <b>%s</b>"));



	tracker_metadata_tile_show (tile);
	g_strfreev (array);

}

/* populates the metadata tile for a document */
static void
_tile_tracker_populate_documents (char **array, GError *error, TrackerMetadataTile *tile )
{
	if (error) {
		g_print ("METADATA_TILE_ERROR : %s", error->message);
		g_clear_error (&error);
		gtk_widget_hide (GTK_WIDGET(tile));
		return;
	}

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	/* create title */
	_property_to_label ( priv->title, array[DOC_NAME] , "<span size='large'><b>%s</b></span>");

	/* then set the remaining properties */
	_property_to_label ( priv->info1, array[DOC_SUBJECT] , _("Subject : <b>%s</b>"));
	_property_to_label ( priv->info2, array[DOC_AUTHOR] , _("Author : <b>%s</b>"));
	_int_to_label ( priv->info3, array[DOC_PAGECOUNT] , _("Page Count : <b>%s</b>"));
	_int_to_label ( priv->info4, array[DOC_WORDCOUNT] , _("Word Count : <b>%s</b>"));
	_date_to_label ( priv->info5, array[DOC_CREATED] , _("Created : <b>%s</b>"));
	_property_to_label ( priv->info6, array[DOC_COMMENTS] , _("Comments : <b>%s</b>"));


	tracker_metadata_tile_show (tile);
	g_strfreev (array);
}


/*populates the metadata tile for a web history url */
static void
_tile_tracker_populate_webhistory(char **array, GError *error, TrackerMetadataTile *tile )
{
	if (error) {
		g_print ("METADATA_TILE_ERROR : %s", error->message);
		g_clear_error (&error);
		gtk_widget_hide (GTK_WIDGET(tile));
		return;
	}

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	/* create title */
	_property_to_label ( priv->title, array[WEBHISTORY_URL] , "<span size='large'><b>%s</b></span>");

	/* then set the remaining properties */
	_property_to_label ( priv->info1, array[WEBHISTORY_TITLE] , _("Subject : <b>%s</b>"));
	_property_to_label ( priv->info2, array[WEBHISTORY_KEYWORDS] , "Keywords: <b>%s</b>");

	tracker_metadata_tile_show (tile);
	g_strfreev (array);

	_show_labels (tile, FALSE);
	gtk_widget_show (priv->info1);
	gtk_widget_show (priv->info2);

}



/* UTILILTY FUNCTIONS FOR CONVERSIONS */

/* Converts bitrate to kbs */
static void
_bitrate_to_label (GtkWidget *label, const char *prop, const char *string)
{
	int size;
	char *format;
	char *temp;

	size = atoi (prop);
	size = size/1000;

	format = g_strdup_printf ("%d", size);
	temp = g_strdup_printf (string, format);
	gtk_label_set_markup (GTK_LABEL (label), temp);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);

	g_free (temp);
	g_free (format);
}

/* Converts seconds to time */
static void
_seconds_to_label (GtkWidget *label, const char *prop, const char *string)
{
	gulong size;
	char *format;
	char *temp;

	size = atol (prop);
	int hours = (int) (size / 3600);
	int minutes = (int)(size/60) - (hours * 60);
	int seconds = (int) (size % 60);

	if ( hours > 0 ) {
		format = g_strdup_printf ("%02d:%02d:%02d", hours, minutes, seconds);
	} else {
		format = g_strdup_printf ("%02d:%02d", minutes, seconds);
	}

	temp = g_strdup_printf (string, format);
	gtk_label_set_markup (GTK_LABEL (label), temp);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);

	g_free (temp);
	g_free (format);
}

/* Converts width and height into WxH */
static void
_dimensions_to_label (GtkWidget *label, const char *width, const char *height, const char *string)
{
	gulong w;
	gulong h;
	char *temp;

	w = atol (width);
	h = atol (height);

	temp = g_strdup_printf (string, w, h);
	gtk_label_set_markup (GTK_LABEL (label), temp);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);

	g_free (temp);
}

/* taken from gnome_vfs, formats a file size to something normal */
static gchar *
format_file_size_for_display (gulong size)
{
	if (size < KILOBYTE_FACTOR) {
		//return g_strdup_printf (dngettext(GETTEXT_PACKAGE, "%u byte", "%u bytes",(guint) size), (guint) size);
		return g_strdup_printf ("%u bytes", (guint) size);
	} else {
		gdouble displayed_size;

		if (size < MEGABYTE_FACTOR) {
			displayed_size = (gdouble) size / KILOBYTE_FACTOR;
			return g_strdup_printf (_("%.1f KB"),
						displayed_size);
		} else if (size < GIGABYTE_FACTOR) {
			displayed_size = (gdouble) size / MEGABYTE_FACTOR;
			return g_strdup_printf (_("%.1f MB"),
						displayed_size);
		} else {
			displayed_size = (gdouble) size / GIGABYTE_FACTOR;
			return g_strdup_printf (_("%.1f GB"),
						displayed_size);
		}
	}
}

/* Converts text size to something human readable */
static void
_size_to_label (GtkWidget *label, const char *prop, const char *string)
{
	gulong size;
	char *format;
	char *temp;

	size = atol (prop);
	format = format_file_size_for_display (size);

	temp = g_strdup_printf (string, format);
	gtk_label_set_markup (GTK_LABEL (label), temp);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);

	g_free (format);
	g_free (temp);
}

/* Converts text size to something human readable */
static void
_int_to_label (GtkWidget *label, const char *prop, const char *string)
{
	gulong size;
	char *temp;
	char *format;

	size = atol (prop);
	format = g_strdup_printf ("%ld", size);

	if (size) {
		temp = g_strdup_printf (string, format);
	} else {
		temp = g_strdup_printf (string, _("Unknown"));
	}
	gtk_label_set_markup (GTK_LABEL (label), temp);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);

	g_free (temp);
	g_free (format);
}

/* Converts ISO date to something human readable */
static gboolean
get_time_from_iso (const char *iso, GDate *val)
{
	g_return_val_if_fail (val, FALSE);

	time_t my_time = atoi (iso);

	if (my_time != 0) {
		g_date_set_time_t (val, my_time);
		return TRUE;
	} else {
		return FALSE;
	}
}

static void
_date_to_label (GtkWidget *label, const char *iso, const char *string)
{
	GDate val;
	char *temp = NULL;

	if (string) {
		if (get_time_from_iso (iso, &val)) {
			gchar buf[256];
			g_date_strftime (buf, 256, "%a %d %b %Y", &val);
			temp = g_strdup_printf (string, buf);
		}
	}

	if (!temp) {
		temp = g_strdup_printf (string, _("Unknown"));
	}

	gtk_label_set_markup (GTK_LABEL (label), temp);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	g_free (temp);
}

static void
_year_to_label (GtkWidget *label, const char *iso, const char *string)
{
	GDate val;
	char *temp = NULL;

	if (string) {
		if (get_time_from_iso (iso, &val)) {
			gchar buf[32];
			g_date_strftime (buf, 32, "%Y", &val);
			temp = g_strdup_printf (string, buf);
		}
	}

	if (!temp) {
		temp = g_strdup_printf (string, _("Unknown"));
	}

	gtk_label_set_markup (GTK_LABEL (label), temp);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	g_free (temp);
}

/* Checks that a property is valid, parses it to play nicely wth pango */
static void
_property_to_label (GtkWidget *label, const char *prop, const char *string)
{
	if (!is_empty_string(prop)) {
		char * temp, *temp2;
		temp2 = g_markup_escape_text (prop, -1);
		temp = g_strdup_printf (string, temp2);
		g_free (temp2);
		gtk_label_set_markup (GTK_LABEL (label), temp);
		gtk_label_set_selectable (GTK_LABEL (label), TRUE);
		g_free (temp);
	} else {
		char * temp;
		temp = g_strdup_printf (string, _("Unknown"));
		gtk_label_set_markup (GTK_LABEL (label), temp);
		gtk_label_set_selectable (GTK_LABEL (label), TRUE);
		g_free (temp);
	}
}



/**
 * tracker_metadata_tile_set_uri:
 * @tile: a #TrackerMetadataTile
 * @uri: the local uri of a file
 * @type: the tracker type or mime type of the file
 * @icon: a GdkPixbuf representing the file, or #NULL
 *
 * Replaces the current metadata in the tile with metadata for the
 * uri specified. Can optionally also update the #GtkImage in the tile with
 * the icon specified.
 *
 **/

void
tracker_metadata_tile_set_uri (TrackerMetadataTile *tile, const gchar *uri,
							  ServiceType service_type,
							  const gchar *type,
							  GdkPixbuf *icon)
{
	TrackerMetadataTilePrivate *priv;

	g_return_if_fail (TRACKER_IS_METADATA_TILE (tile));

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	gtk_image_clear (GTK_IMAGE (priv->image));

	if (icon) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), icon);
	} else {
		gtk_widget_hide (priv->image);
	}

	/* call correct function according to service type */
	switch (service_type) {

	case SERVICE_MUSIC:

		tracker_metadata_get_async (priv->client, SERVICE_MUSIC,
					    uri, audio_keys,
					    (TrackerArrayReply)_tile_tracker_populate_audio,
					    (gpointer)tile);

		break;

	case SERVICE_EMAILS:

		tracker_metadata_get_async (priv->client, SERVICE_EMAILS,
					    uri, email_keys,
					    (TrackerArrayReply)_tile_tracker_populate_email,
					    (gpointer)tile);
		break;


	case SERVICE_DOCUMENTS:

		tracker_metadata_get_async (priv->client, SERVICE_DOCUMENTS,
					    uri, doc_keys,
					    (TrackerArrayReply)_tile_tracker_populate_documents,
					    (gpointer)tile);
		break;

	case SERVICE_WEBHISTORY:

		tracker_metadata_get_async (priv->client, SERVICE_WEBHISTORY,
					    uri, webhistory_keys,
					    (TrackerArrayReply)_tile_tracker_populate_webhistory,
					    (gpointer)tile);
		break;


	case SERVICE_IMAGES:

		tracker_metadata_get_async (priv->client, SERVICE_IMAGES,
					    uri, image_keys,
					    (TrackerArrayReply)_tile_tracker_populate_image,
					    (gpointer)tile);

		break;

	case SERVICE_VIDEOS:
		tracker_metadata_get_async (priv->client, SERVICE_VIDEOS,
					    uri, video_keys,
					    (TrackerArrayReply)_tile_tracker_populate_video,
					    (gpointer)tile);

		break;

	case SERVICE_APPLICATIONS:

		tracker_metadata_get_async (priv->client, SERVICE_APPLICATIONS,
					    uri, app_keys,
					    (TrackerArrayReply)_tile_tracker_populate_applications,
					    (gpointer)tile);

		break;


	default:

		if (!uri) {
			_tile_tracker_populate_blank (tile);
		} else {

			tracker_metadata_get_async (priv->client, SERVICE_FILES,
						    uri, default_keys,
						    (TrackerArrayReply)_tile_tracker_populate_default,
						    (gpointer)tile);
		}

		break;
	}



	if (uri) {
		gtk_widget_show (priv->tag_bar);
		tracker_tag_bar_set_uri (TRACKER_TAG_BAR (priv->tag_bar), service_type, uri);
	} else {
		gtk_widget_hide (priv->tag_bar);
	}

	gtk_widget_queue_draw (GTK_WIDGET (tile));
}

static void
tracker_metadata_tile_show (TrackerMetadataTile *tile)
{
	g_return_if_fail (TRACKER_IS_METADATA_TILE (tile));

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	if (priv->expanded) {
		gtk_widget_show_all (GTK_WIDGET (tile));
	} else {
		gtk_widget_show_all (GTK_WIDGET (tile));
		gtk_widget_hide (priv->table);
		gtk_widget_hide (priv->image);
	}
}

static gboolean
tracker_metadata_tile_toggle_view (GtkWidget *button, TrackerMetadataTile *tile)
{
	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	if (priv->expanded) {
		gtk_widget_hide (priv->image);
		gtk_widget_hide (priv->table);
		gtk_arrow_set (GTK_ARROW (priv->arrow),
			       GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
		gtk_alignment_set_padding (GTK_ALIGNMENT (priv->align), 1, 1, 4, 4);
	} else {
		gtk_widget_show (priv->image);
		gtk_widget_show (priv->table);
		gtk_arrow_set (GTK_ARROW (priv->arrow),
			       GTK_ARROW_DOWN, GTK_SHADOW_NONE);
		gtk_alignment_set_padding (GTK_ALIGNMENT (priv->align), 6, 6, 4, 4);
	}
	priv->expanded = !priv->expanded;
	return FALSE;
}

static void
draw (GtkWidget *widget, cairo_t *cr)
{
	TrackerMetadataTile *tile;
	TrackerMetadataTilePrivate *priv;
	double width, height;
	GtkStyle *style;
	GdkColor step1;
	GdkColor step2;

	tile = TRACKER_METADATA_TILE (widget);
	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	width = widget->allocation.width;
	height = widget->allocation.height;

	style = gtk_widget_get_style (widget);
	step1 = style->base[GTK_STATE_NORMAL];
	step2 = style->bg[GTK_STATE_SELECTED];

	/* clear window to base[NORMAL] */
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	gdk_cairo_set_source_color (cr, &step1);
	cairo_paint (cr);

	cairo_move_to(cr, 0, 0);
	cairo_set_line_width(cr, 1.0);

	cairo_pattern_t *pat;
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	/* main gradient */
	pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, height);
	cairo_pattern_add_color_stop_rgba (pat, 0.0, step2.red/65535.0,
						     step2.green/65535.0,
						     step2.blue/65535.0,
						     0.05);
	cairo_pattern_add_color_stop_rgba ( pat, 1.0, step2.red/65535.0,
						      step2.green/65535.0,
						      step2.blue/65535.0,
						      0.5);

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_set_source(cr, pat);
	cairo_fill(cr);
	cairo_pattern_destroy(pat);

	/* border line */
	cairo_set_source_rgba (cr, step2.red/65535.0,
				   step2.green/65535.0,
				   step2.blue/65535.0,
				   0.7);
	cairo_move_to (cr, 0, 0);
	cairo_line_to (cr, width, 0);
	cairo_stroke (cr);

	/* highlight line */
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.5);
	cairo_move_to (cr, 0, 1);
	cairo_line_to (cr, width, 1);
	cairo_stroke (cr);

	if (!priv->expanded)
		return;
	/* mime icon */
	if (priv->preview != NULL) {
		double x, y;
		x = width - gdk_pixbuf_get_width (priv->preview);
		y = height - gdk_pixbuf_get_height (priv->preview)+5;
		gdk_cairo_set_source_pixbuf  (cr, priv->preview, x, y);
		cairo_paint_with_alpha (cr, 0.2);
	}
	/* watermark */
	if (priv->type == NULL)
		return;

	cairo_text_extents_t extents;
	double x,y;
	int font_slant = CAIRO_FONT_SLANT_NORMAL;
	int font_weight = CAIRO_FONT_WEIGHT_NORMAL;

	cairo_select_font_face (cr, "Sans",font_slant, font_weight);
	cairo_set_font_size (cr, 40);

	cairo_text_extents (cr, priv->type, &extents);
	x = (width)-(extents.width + extents.x_bearing)-90;
	y = (height)-(extents.height + extents.y_bearing)-5;


	/* shadow */
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.05);
	cairo_move_to (cr, x-1, y-1);
	cairo_show_text (cr, priv->type);

	/*main text */
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.1);
	cairo_move_to (cr, x, y);
	cairo_show_text (cr, priv->type);
}

static gboolean
tracker_metadata_tile_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	cairo_t *cr;
	cr = gdk_cairo_create (widget->window);
	draw (widget, cr);
	cairo_destroy (cr);

	return GTK_WIDGET_CLASS(parent_class)->expose_event(widget, event);
}

static void
tracker_metadata_tile_class_init (TrackerMetadataTileClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	gobject_class = G_OBJECT_CLASS (klass);
	//gobject_class->finalize = finalize;

	widget_class->expose_event = tracker_metadata_tile_expose_event;

	g_type_class_add_private (gobject_class, sizeof (TrackerMetadataTilePrivate));
}

static void
tracker_metadata_tile_init (TrackerMetadataTile *tile)
{
	GtkWidget *align, *button, *image;
	GtkWidget *label, *table, *arrow;
	GtkWidget *tag_bar;
	GtkWidget *hbox, *vbox, *box;

	TrackerMetadataTilePrivate *priv;

	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	gtk_widget_set_app_paintable (GTK_WIDGET(tile), TRUE);

	priv->expanded = TRUE;
	priv->type = NULL;
	priv->preview = NULL;

	align = gtk_alignment_new (0.5, 0.5, 1, 1);
	priv->align = align;
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 6, 6, 4, 4);
	gtk_container_add (GTK_CONTAINER (tile), align);
	gtk_widget_show (align);

	/* main hbox */
	box = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (align), box);
	gtk_widget_show (box);

	/* Image widget */
	image = gtk_image_new ();
	priv->image = image;
	gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
	gtk_widget_show (image);

	/* center vbox */
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (box), vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	/* arrow & title */
	button = gtk_button_new ();
	priv->expander = button;
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_widget_show (button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (tracker_metadata_tile_toggle_view), (gpointer)tile);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_add (GTK_CONTAINER(button), hbox);
	gtk_widget_show (hbox);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	priv->arrow = arrow;
	gtk_box_pack_start (GTK_BOX(hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_show (arrow);

	label = gtk_label_new (" ");
	priv->title = label;
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	/* info table */
	table = gtk_table_new (3, 3, FALSE);
	priv->table = table;
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_widget_show (table);

	label = gtk_label_new (" ");
	priv->info1 = label;
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, 0, 1);

	label = gtk_label_new (" ");
	priv->info2 = label;
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, 1, 2);

	label = gtk_label_new (" ");
	priv->info3 = label;
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE(table), label, 1, 2, 0, 1);

	label = gtk_label_new (" ");
	priv->info4 = label;
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE(table), label, 1, 2, 1, 2);

	label = gtk_label_new (" ");
	priv->info5 = label;
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE(table), label, 2, 3, 0, 1);

	label = gtk_label_new (" ");
	priv->info6 = label;
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE(table), label, 2, 3, 1, 2);

	/* tag bar */
	tag_bar = tracker_tag_bar_new ();
	priv->tag_bar = tag_bar;
	gtk_widget_show_all (tag_bar);

	gtk_table_attach_defaults (GTK_TABLE (table), tag_bar, 0, 3, 2, 3);
	gtk_widget_show_all (table);
}

GtkWidget *
tracker_metadata_tile_new (void)
{
	TrackerClient *client;
	GtkWidget *tile;
	TrackerMetadataTilePrivate *priv;

	tile = g_object_new (TRACKER_TYPE_METADATA_TILE, NULL);
	priv = TRACKER_METADATA_TILE_GET_PRIVATE (tile);

	client = tracker_connect (TRUE);
	priv->client = client;
	return tile;
}
