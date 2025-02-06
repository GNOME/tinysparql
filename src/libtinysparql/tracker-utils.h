/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
 */

#pragma once

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <tinysparql.h> must be included directly."
#endif

#include <glib.h>
#include <gio/gio.h>
#include <tracker-version.h>

G_BEGIN_DECLS

TRACKER_AVAILABLE_IN_ALL
gchar * tracker_sparql_escape_uri_vprintf (const gchar *format,
                                           va_list      args);
TRACKER_AVAILABLE_IN_ALL
gchar * tracker_sparql_escape_uri_printf  (const gchar* format,
                                           ...);
TRACKER_AVAILABLE_IN_ALL
gchar * tracker_sparql_escape_uri         (const gchar *uri);

TRACKER_AVAILABLE_IN_ALL
gchar* tracker_sparql_escape_string (const gchar* literal);
TRACKER_AVAILABLE_IN_ALL
gchar* tracker_sparql_get_uuid_urn (void);

TRACKER_AVAILABLE_IN_ALL
GFile *
tracker_sparql_get_ontology_nepomuk (void);

G_END_DECLS
