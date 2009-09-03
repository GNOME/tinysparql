/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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
 *
 *  tracker_uri_vprintf_escaped, tracker_uri_printf_escaped got copied from 
 *  GLib's gmarkup.c
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include <libtracker-common/tracker-statement-list.h>

void
tracker_statement_list_insert (TrackerSparqlBuilder *statements, 
                               const gchar *subject,
                               const gchar *predicate,
                               const gchar *value)
{
	g_return_if_fail (TRACKER_IS_SPARQL_BUILDER (statements));
	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (value != NULL);

	tracker_sparql_builder_subject_iri (statements, subject);
	tracker_sparql_builder_predicate_iri (statements, predicate);
	tracker_sparql_builder_object_unvalidated (statements, value);
}


void
tracker_sparql_builder_object_unvalidated (TrackerSparqlBuilder *sparql,
                                           const gchar *value)
{
	const gchar *end;

	g_return_if_fail (TRACKER_IS_SPARQL_BUILDER (sparql));
	g_return_if_fail (value != NULL);

	if (!g_utf8_validate (value, -1, &end)) {
		gchar *valid;

		/* g_message ("Only inserting %ld/%ld (valid UTF8) bytes for statement list", */
		/* 	   end - value, */
		/* 	   strlen (value)); */

		if (value != end) {
			valid = g_strndup (value, end - value);
			tracker_sparql_builder_object_string (sparql, valid);
			g_free (valid);
		} else {
			tracker_sparql_builder_object_string (sparql, "(invalid data)");
		}

		return;
	}

	tracker_sparql_builder_object_string (sparql, value);
}

void
tracker_statement_list_insert_with_int64 (TrackerSparqlBuilder   *statements,
                                     const gchar *subject,
                                     const gchar *predicate,
                                     gint64       value)
{
	gchar *value_str;

	value_str = g_strdup_printf ("%" G_GINT64_FORMAT, value);
	tracker_statement_list_insert (statements, subject, predicate, value_str);
	g_free (value_str);
}

void
tracker_statement_list_insert_with_uint (TrackerSparqlBuilder   *statements,
					 const gchar *subject,
					 const gchar *predicate,
					 guint32      value)
{
	gchar *value_str;

	value_str = g_strdup_printf ("%" G_GUINT32_FORMAT, value);
	tracker_statement_list_insert (statements, subject, predicate, value_str);
	g_free (value_str);
}


void
tracker_statement_list_insert_with_double  (TrackerSparqlBuilder   *statements,
                                       const gchar *subject,
                                       const gchar *predicate,
                                       gdouble      value)
{
	gchar *value_str;

	value_str = g_strdup_printf ("%g", value);
	tracker_statement_list_insert (statements, subject, predicate, value_str);
	g_free (value_str);
}


void
tracker_statement_list_insert_with_float  (TrackerSparqlBuilder   *statements,
                                       const gchar *subject,
                                       const gchar *predicate,
                                       gfloat      value)
{
	gchar *value_str;

	value_str = g_strdup_printf ("%f", value);
	tracker_statement_list_insert (statements, subject, predicate, value_str);
	g_free (value_str);
}

void
tracker_statement_list_insert_with_int (TrackerSparqlBuilder   *statements,
                                   const gchar *subject,
                                   const gchar *predicate,
                                   gint         value)
{
	gchar *value_str;

	value_str = g_strdup_printf ("%d", value);
	tracker_statement_list_insert (statements, subject, predicate, value_str);
	g_free (value_str);
}

