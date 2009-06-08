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
 */

#ifndef __TRACKER_STATEMENT_H__
#define __TRACKER_STATEMENT_H__

#include <glib.h>

#define SHOULD_VALIDATE_UTF8

G_BEGIN_DECLS

void   tracker_statement_list_insert             (GPtrArray   *statements, 
                                             const gchar *subject,
                                             const gchar *predicate,
                                             const gchar *value);
void   tracker_statement_list_insert_with_int    (GPtrArray   *statements,
                                             const gchar *subject,
                                             const gchar *predicate,
                                             gint         value);
void   tracker_statement_list_insert_with_int64  (GPtrArray   *statements,
                                             const gchar *subject,
                                             const gchar *predicate,
                                             gint64       value);
void   tracker_statement_list_insert_with_double (GPtrArray   *statements,
                                             const gchar *subject,
                                             const gchar *predicate,
                                             gdouble      value);
void   tracker_statement_list_insert_with_float  (GPtrArray   *statements,
                                             const gchar *subject,
                                             const gchar *predicate,
                                             gfloat      value);
const gchar* tracker_statement_list_find         (GPtrArray *statements, 
                                             const gchar *subj, 
                                             const gchar *pred);


G_END_DECLS

#endif /* __TRACKER_ESCAPE_H__ */
