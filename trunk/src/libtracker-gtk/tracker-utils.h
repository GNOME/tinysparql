/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * libtracker-gtk/tracker-utils.c - Grab bag of functions for manuipulating
 * tracker results into more Gtk friedly types.
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

#ifndef TRACKER_UTILS_H
#define TRACKER_UTILS_H

#include <glib.h>
#include <gtk/gtk.h>

#include <tracker.h>

GList *		tracker_keyword_array_to_glist (gchar **array);
GList *		tracker_get_all_keywords (TrackerClient *tracker_client);
GtkTreeModel *	tracker_create_simple_keyword_liststore (const GList *list);
void		tracker_set_atk_relationship(GtkWidget *obj1, int relation_type,
					     GtkWidget *obj2);
#endif /* TRACKER_UTILS_H */
