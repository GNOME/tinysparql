/*
 * Copyright (C) 2017, Red Hat, Inc
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef TRACKER_UTILS_H
#define TRACKER_UTILS_H

#include <glib.h>
#include "tracker-ontology-model.h"

G_BEGIN_DECLS

typedef struct {
	GString *before;
	GString *after;
	GString *link_label;
	TrackerOntologyClass *class;
	gint visible_len;
} HierarchyString;

GPtrArray *
class_get_parent_hierarchy_strings (TrackerOntologyClass *klass,
                                    TrackerOntologyModel *model);

G_END_DECLS

#endif /* TRACKER_UTILS_H */
