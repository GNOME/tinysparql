/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
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


const gchar*
tracker_statement_list_find (GPtrArray *statements, 
                        const gchar *subj, 
                        const gchar *pred)
{
	guint i;
	const gchar *subject;
	const gchar *predicate;
	const gchar *object = NULL;

	for (i = 0; i < statements->len; i++) {
		GValueArray *statement;

		statement = statements->pdata[i];

		subject = g_value_get_string (&statement->values[0]);
		predicate = g_value_get_string (&statement->values[1]);
		object = g_value_get_string (&statement->values[2]);

		if (g_strcmp0 (pred, predicate) == 0 && g_strcmp0 (subj, subject) == 0)
			break;
	}

	return object;
}

void
tracker_statement_list_insert (GPtrArray   *statements, 
                          const gchar *subject,
                          const gchar *predicate,
                          const gchar *value)
{
	GValueArray *statement;
	GValue       gvalue = { 0 };

	statement = g_value_array_new (3);

	g_value_init (&gvalue, G_TYPE_STRING);
	g_value_set_string (&gvalue, subject);
	g_value_array_append (statement, &gvalue);
	g_value_unset (&gvalue);

	g_value_init (&gvalue, G_TYPE_STRING);
	g_value_set_string (&gvalue, predicate);
	g_value_array_append (statement, &gvalue);
	g_value_unset (&gvalue);

	g_value_init (&gvalue, G_TYPE_STRING);
	g_value_set_string (&gvalue, value);
	g_value_array_append (statement, &gvalue);
	g_value_unset (&gvalue);

	g_ptr_array_add (statements, statement);
}


void
tracker_statement_list_insert_with_int64 (GPtrArray   *statements,
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
tracker_statement_list_insert_with_double  (GPtrArray   *statements,
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
tracker_statement_list_insert_with_float  (GPtrArray   *statements,
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
tracker_statement_list_insert_with_int (GPtrArray   *statements,
                                   const gchar *subject,
                                   const gchar *predicate,
                                   gint         value)
{
	gchar *value_str;

	value_str = g_strdup_printf ("%d", value);
	tracker_statement_list_insert (statements, subject, predicate, value_str);
	g_free (value_str);
}

