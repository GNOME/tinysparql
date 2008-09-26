/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __TRACKERD_RDF_QUERY_H__
#define __TRACKERD_RDF_QUERY_H__

#include <glib.h>

#include <libtracker-db/tracker-db-manager.h>

G_BEGIN_DECLS

gchar *tracker_rdf_query_to_sql (TrackerDBInterface  *iface,
				 const gchar	     *query,
				 const gchar	     *service,
				 gchar		    **fields,
				 gint		      field_count,
				 const gchar	     *search_text,
				 const gchar	     *keyword,
				 gboolean	      sort_by_service,
				 gchar		    **sort_fields,
				 gint		      sort_field_count,
				 gboolean	      sort_desc,
				 gint		      offset,
				 gint		      limit,
				 GError		    **error);

void tracker_rdf_filter_to_sql	(TrackerDBInterface  *iface,
				 const gchar	     *query,
				 const gchar	     *service,
				 GSList		    **fields,
				 gchar		    **from,
				 gchar		    **where,
				 GError		    **error);

G_END_DECLS

#endif /* __TRACKERD_RDF_QUERY_H__ */
