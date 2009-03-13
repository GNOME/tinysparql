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

#ifndef TRACKER_UI_H
#define TRACKER_UI_H

#include <glib.h>
#include <gtk/gtk.h>

/**
 * The GtkTargetEntry to use as the drag type when
 * dragging and dropping keywords
 **/
const GtkTargetEntry KEYWORD_DRAG_TYPES[] = {
	{"property/keyword", 0, 0 }
};

void
tracker_render_emblem_pixbuf_cb (GtkCellLayout	 *cell_layout,
				 GtkCellRenderer *cell,
				 GtkTreeModel	 *tree_model,
				 GtkTreeIter	 *iter,
				 gpointer	 user_data);

#endif /* TRACKER_UI_H */
