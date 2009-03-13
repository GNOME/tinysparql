/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * libtracker-gtk/tracker-ui.c - Functions for creating tracker centric UI
 * elemetents.
 *
 * Copyright (C) 2007 John Stowers
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
 */

#include <glib.h>
#include <gtk/gtk.h>

#include "tracker-keyword-store.h"
#include "tracker-ui.h"

/**
 * tracker_render_emblem_pixbuf_cb:
 *
 * designed to be used as a gtk_cell_layout_set_cell_data_func. Returns the
 * emblem pixbuf for the keyword in the ListStore to which it is rendering
 *
 **/
void
tracker_render_emblem_pixbuf_cb (GtkCellLayout			*cell_layout,
				 GtkCellRenderer		*cell,
				 GtkTreeModel			*tree_model,
				 GtkTreeIter			*iter,
				 gpointer			icon_theme)
{
	char *stock_id;
	GdkPixbuf *pixbuf;
	GtkIconTheme *theme;

	theme = GTK_ICON_THEME (icon_theme);

	gtk_tree_model_get (tree_model, iter, TRACKER_KEYWORD_STORE_IMAGE_URI, &stock_id, -1);
	if (stock_id == NULL) {
		stock_id = g_strdup ("emblem-generic");
	}

	pixbuf = gtk_icon_theme_load_icon (theme, stock_id, 24, 0, NULL);
	if (pixbuf != NULL) {
		g_object_set (cell, "pixbuf", pixbuf, NULL);
		g_object_unref (pixbuf);
	} else {
		g_warning("ICON NOT FOUND\n");
	}

	g_free (stock_id);
}
