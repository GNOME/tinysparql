/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _TRACKER_RDF_QUERY_H_
#define _TRACKER_RDF_QUERY_H_

#include <glib.h>

#include "tracker-db.h"


char *	tracker_rdf_query_to_sql (DBConnection *db_con, const char *query, const char *service, char **fields, int field_count, const char *search_text, const char *keyword, gboolean sort_by_service, int offset, int limit, GError *error);

#endif
