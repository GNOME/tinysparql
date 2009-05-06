/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
 * Copyright (C) 2008, Nokia
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

#ifndef __TRACKER_DATA_SEARCH_H__
#define __TRACKER_DATA_SEARCH_H__

#include <glib.h>

#include <libtracker-common/tracker-field.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-interface.h>
#include <libtracker-db/tracker-db-file-info.h>
#include <libtracker-db/tracker-db-index.h>

#include "tracker-field-data.h"

G_BEGIN_DECLS

/* Search API */
TrackerDBResultSet *tracker_data_search_text			 (TrackerDBInterface  *iface,
								  const gchar	      *service,
								  const gchar	      *search_string,
								  gint		       offset,
								  gint		       limit,
								  gboolean	       save_results,
								  gboolean	       detailed);
TrackerDBResultSet *tracker_data_search_text_and_mime		 (TrackerDBInterface  *iface,
								  const gchar	      *text,
								  gchar		     **mime_array);
TrackerDBResultSet *tracker_data_search_text_and_location	 (TrackerDBInterface  *iface,
								  const gchar	      *text,
								  const gchar	      *location);
TrackerDBResultSet *tracker_data_search_text_and_mime_and_location (TrackerDBInterface  *iface,
								  const gchar	      *text,
								  gchar		     **mime_array,
								  const gchar	      *location);

/* Files API */
gchar **	    tracker_data_search_files_get		 (TrackerDBInterface  *iface,
								  const gchar	      *folder_path);
TrackerDBResultSet *tracker_data_search_files_get_by_service	 (TrackerDBInterface  *iface,
								  const gchar	      *service,
								  gint		       offset,
								  gint		       limit);
TrackerDBResultSet *tracker_data_search_files_get_by_mime	 (TrackerDBInterface  *iface,
								  gchar		     **mimes,
								  gint		       n,
								  gint		       offset,
								  gint		       limit,
								  gboolean	       vfs);

/* Keywords API */
TrackerDBResultSet *tracker_data_search_keywords_get_list	 (TrackerDBInterface  *iface,
								  const gchar	      *service);

TrackerDBResultSet *tracker_data_search_get_unique_values			(const gchar	   *service_type,
										 gchar		  **fields,
										 const gchar	   *query_condition,
										 gboolean	    order_desc,
										 gint		    offset,
										 gint		    max_hits,
										 GError		  **error);
gint		    tracker_data_search_get_sum					(const gchar	   *service_type,
										 const gchar	   *field,
										 const gchar	   *query_condition,
										 GError		  **error);
gint		    tracker_data_search_get_count				(const gchar	   *service_type,
										 const gchar	   *field,
										 const gchar	   *query_condition,
										 GError		  **error);

TrackerDBResultSet *tracker_data_search_get_unique_values_with_count		(const gchar	   *service_type,
										 gchar		  **fields,
										 const gchar	   *query_condition,
										 const gchar	   *count,
										 gboolean	    order_desc,
										 gint		    offset,
										 gint		    max_hits,
										 GError		  **error);

TrackerDBResultSet *tracker_data_search_get_unique_values_with_count_and_sum	(const gchar	   *service_type,
										 gchar		  **fields,
										 const gchar	   *query_condition,
										 const gchar	   *count,
										 const gchar       *sum,
										 gboolean	    order_desc,
										 gint		    offset,
										 gint		    max_hits,
										 GError		  **error);

TrackerDBResultSet *tracker_data_search_get_unique_values_with_concat_count_and_sum	(const gchar	   *service_type,
											 gchar		  **fields,
											 const gchar	   *query_condition,
											 const gchar       *concat_field,
											 const gchar	   *count,
											 const gchar       *sum,
											 gboolean	    order_desc,
											 gint		    offset,
											 gint		    max_hits,
											 GError		  **error);

TrackerDBResultSet *tracker_data_search_get_unique_values_with_aggregates	(const gchar	   *service_type,
										 gchar		  **fields,
										 const gchar	   *query_condition,
										 gchar	          **aggregates,
										 gchar            **aggregate_fields,
										 gboolean	    order_desc,
										 gint		    offset,
										 gint		    max_hits,
										 GError		  **error);

TrackerDBResultSet *tracker_data_search_metadata_in_path			(const gchar	   *path,
										 gchar		  **fields,
										 GError		  **error);

TrackerDBResultSet *tracker_data_search_keywords				(const gchar	   *service_type,
										 const gchar      **keywords,
										 gint		    offset,
										 gint		    max_hits,
										 GError	          **error);

G_END_DECLS

#endif /* __TRACKER_DATA_SEARCH_H__ */
