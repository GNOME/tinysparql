/*
 * Copyright (C) 2009, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "tracker-aligned-window.h"

#define TRACKER_ALIGNED_WINDOW_GET_PRIVATE(obj)         (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_ALIGNED_WINDOW, TrackerAlignedWindowPrivate))

struct _TrackerAlignedWindowPrivate {
	GtkWidget *align_widget;
	guint motion_id;
};

enum {
	PROP_0,
	PROP_ALIGN_WIDGET
};

static void     tracker_aligned_window_finalize         (GObject              *object);
static void     tracker_aligned_window_get_property     (GObject              *object,
                                                         guint                 prop_id,
                                                         GValue               *value,
                                                         GParamSpec           *pspec);
static void     tracker_aligned_window_set_property     (GObject              *object,
                                                         guint                 prop_id,
                                                         const GValue         *value,
                                                         GParamSpec           *pspec);
static void     tracker_aligned_window_realize          (GtkWidget            *widget);
static void     tracker_aligned_window_show             (GtkWidget            *widget);
static gboolean tracker_aligned_window_motion_notify_cb (GtkWidget            *widget,
                                                         GdkEventMotion       *event,
                                                         TrackerAlignedWindow *aligned_window);

G_DEFINE_TYPE (TrackerAlignedWindow, tracker_aligned_window, GTK_TYPE_WINDOW);

static void
tracker_aligned_window_class_init (TrackerAlignedWindowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  
	gobject_class->set_property = tracker_aligned_window_set_property;
	gobject_class->get_property = tracker_aligned_window_get_property;
	gobject_class->finalize = tracker_aligned_window_finalize;
  
	widget_class->realize = tracker_aligned_window_realize;
	widget_class->show = tracker_aligned_window_show;
  
	g_object_class_install_property (gobject_class, PROP_ALIGN_WIDGET,
	                                 g_param_spec_object ("align-widget",
	                                                      "Align Widget",
	                                                      "The widget the window should align to",
	                                                      GTK_TYPE_WIDGET,
	                                                      G_PARAM_READWRITE));
  
	g_type_class_add_private (klass, sizeof (TrackerAlignedWindowPrivate));
}

static void
tracker_aligned_window_init (TrackerAlignedWindow *aligned_window)
{
	TrackerAlignedWindowPrivate *priv = TRACKER_ALIGNED_WINDOW_GET_PRIVATE (aligned_window);
	GtkWindow *window = GTK_WINDOW (aligned_window);
  
	aligned_window->private = priv;
  
	priv->align_widget = NULL;
	priv->motion_id = 0;
  
	/* set window properties */
	window->type = GTK_WINDOW_TOPLEVEL;

	gtk_window_set_decorated (window, FALSE);
	gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DOCK);
}

static void
tracker_aligned_window_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	TrackerAlignedWindow *aligned_window = TRACKER_ALIGNED_WINDOW (object);
  
	switch (prop_id) {
	case PROP_ALIGN_WIDGET:
		g_value_set_object (value, aligned_window->private->align_widget);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_aligned_window_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	TrackerAlignedWindow *aligned_window = TRACKER_ALIGNED_WINDOW (object);
  
	switch (prop_id) {
	case PROP_ALIGN_WIDGET:
		tracker_aligned_window_set_widget (aligned_window,
		                                   g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_aligned_window_position (TrackerAlignedWindow *window)
{
	TrackerAlignedWindowPrivate *priv;
	GtkWidget *align_widget;
	gint our_width, our_height;
	gint entry_x, entry_y, entry_width, entry_height;
	gint x, y;
	GdkGravity gravity = GDK_GRAVITY_NORTH_WEST;

	g_assert (TRACKER_IS_ALIGNED_WINDOW (window));
	priv = window->private;

	if (!priv->align_widget) {
		return;
	}

	align_widget = priv->align_widget;

	gdk_flush ();
  
	gdk_window_get_geometry (GTK_WIDGET (window)->window,
	                         NULL,
	                         NULL,
	                         &our_width,
	                         &our_height,
	                         NULL);
  
	/* stick, skip taskbar and pager */
	gtk_window_stick (GTK_WINDOW (window));
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);

	/* make sure the align_widget is realized before we do anything */
	gtk_widget_realize (align_widget);
  
	/* get the positional and dimensional attributes of the align widget */
	gdk_window_get_origin (align_widget->window,
	                       &entry_x,
	                       &entry_y);
	gdk_window_get_geometry (align_widget->window,
	                         NULL,
	                         NULL,
	                         &entry_width,
	                         &entry_height,
	                         NULL);
  
	if (entry_x + our_width < gdk_screen_width ()) {
		x = entry_x + 1;
	} else {
		x = entry_x + entry_width - our_width - 1;
      
		gravity = GDK_GRAVITY_NORTH_EAST;
	}
  
	if (entry_y + entry_height + our_height < gdk_screen_height ()) {
		y = entry_y + entry_height + 1;
	} else {
		y = entry_y - our_height + 1;
      
		if (gravity == GDK_GRAVITY_NORTH_EAST) {
			gravity = GDK_GRAVITY_SOUTH_EAST;
		} else {
			gravity = GDK_GRAVITY_SOUTH_WEST;
		}
	}
  
	gtk_window_set_gravity (GTK_WINDOW (window), gravity);
	gtk_window_move (GTK_WINDOW (window), x, y);
}

static void
tracker_aligned_window_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (tracker_aligned_window_parent_class)->realize (widget);

	tracker_aligned_window_position (TRACKER_ALIGNED_WINDOW (widget));
}

static void
tracker_aligned_window_show (GtkWidget *widget)
{
	tracker_aligned_window_position (TRACKER_ALIGNED_WINDOW (widget));
  
	GTK_WIDGET_CLASS (tracker_aligned_window_parent_class)->show (widget);
}

static void
tracker_aligned_window_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_aligned_window_parent_class)->finalize (object);
}

static gboolean
tracker_aligned_window_motion_notify_cb (GtkWidget            *widget,
                                         GdkEventMotion       *event,
                                         TrackerAlignedWindow *aligned_window)
{
	GtkAllocation alloc;
	GdkRectangle rect;

	alloc = GTK_WIDGET (aligned_window)->allocation;
  
	rect.x = 0;
	rect.y = 0;
	rect.width = alloc.width;
	rect.height = alloc.height;

	gdk_window_invalidate_rect (GTK_WIDGET (aligned_window)->window,
	                            &rect,
	                            FALSE);
  
	return FALSE;
}

/**
 * tracker_aligned_window_new:
 * @align_widget: a #GtkWidget to which the window should align
 *
 * Creates a new window, aligned to a previously created widget.
 *
 * Return value: a new #TrackerAlignedWindow
 */
GtkWidget *
tracker_aligned_window_new (GtkWidget *align_widget)
{
	return g_object_new (TRACKER_TYPE_ALIGNED_WINDOW,
	                     "align-widget", align_widget,
	                     NULL);
}

/**
 * tracker_aligned_window_set_widget:
 * @aligned_window: a #TrackerAlignedWindow
 * @align_widget: the #GtkWidget @aligned_window should align to
 *
 * Sets @align_widget as the #GtkWidget to which @aligned_window should
 * align.
 *
 * Note that @align_widget must have a #GdkWindow in order to
 * #TrackerAlignedWindow to work.
 */
void
tracker_aligned_window_set_widget (TrackerAlignedWindow *aligned_window,
                                   GtkWidget          *align_widget)
{
	TrackerAlignedWindowPrivate *priv;
  
	g_return_if_fail (TRACKER_IS_ALIGNED_WINDOW (aligned_window));
	g_return_if_fail (GTK_IS_WIDGET (align_widget));

#if 0  
	if (GTK_WIDGET_NO_WINDOW (align_widget))
		{
			g_warning ("Attempting to set a widget of class '%s' as the "
			           "align widget, but widgets of this class does not "
			           "have a GdkWindow.",
			           g_type_name (G_OBJECT_TYPE (align_widget)));
      
			return;
		}
#endif

	priv = TRACKER_ALIGNED_WINDOW_GET_PRIVATE (aligned_window);
  
	if (priv->align_widget) {
		g_signal_handler_disconnect (priv->align_widget, priv->motion_id);
		priv->align_widget = NULL;
	}

	priv->align_widget = align_widget;

	/* FIXME: This causes problems when the pointer goes out of the
	 * window after it is removed using escape.
	 * 
	 * Probably fixable by watching for events somewhere and
	 * handling this better.
	 */
	if (0) {
		priv->motion_id = g_signal_connect (priv->align_widget, "motion-notify-event",
		                                    G_CALLBACK (tracker_aligned_window_motion_notify_cb),
		                                    aligned_window);
	}
}

/**
 * tracker_aligned_window_get_widget:
 * @aligned_window: a #TrackerAlignedWindow
 *
 * Retrieves the #GtkWidget to which @aligned_window is aligned to.
 *
 * Return value: the align widget.
 */
GtkWidget *
tracker_aligned_window_get_widget (TrackerAlignedWindow *aligned_window)
{
	g_return_val_if_fail (TRACKER_IS_ALIGNED_WINDOW (aligned_window), NULL);
  
	return aligned_window->private->align_widget;
}
