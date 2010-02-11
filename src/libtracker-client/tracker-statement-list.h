/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __LIBTRACKER_CLIENT_STATEMENT_H__
#define __LIBTRACKER_CLIENT_STATEMENT_H__

#include <glib.h>

#include "tracker-sparql-builder.h"

G_BEGIN_DECLS

#define SHOULD_VALIDATE_UTF8

void tracker_statement_list_insert             (TrackerSparqlBuilder *statements,
                                                const gchar          *subject,
                                                const gchar          *predicate,
                                                const gchar          *value);
void tracker_statement_list_insert_with_int    (TrackerSparqlBuilder *statements,
                                                const gchar          *subject,
                                                const gchar          *predicate,
                                                gint                  value);
void tracker_statement_list_insert_with_int64  (TrackerSparqlBuilder *statements,
                                                const gchar          *subject,
                                                const gchar          *predicate,
                                                gint64                value);
void tracker_statement_list_insert_with_uint   (TrackerSparqlBuilder *statements,
                                                const gchar          *subject,
                                                const gchar          *predicate,
                                                guint32               value);
void tracker_statement_list_insert_with_double (TrackerSparqlBuilder *statements,
                                                const gchar          *subject,
                                                const gchar          *predicate,
                                                gdouble               value);
void tracker_statement_list_insert_with_float  (TrackerSparqlBuilder *statements,
                                                const gchar          *subject,
                                                const gchar          *predicate,
                                                gfloat                value);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_STATEMENT_H__ */
