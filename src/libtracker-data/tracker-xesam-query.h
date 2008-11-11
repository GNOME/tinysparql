/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 * Authors: Ottela Mikael, (mikael.ottela@ixonos.com)
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

#ifndef __TRACKERD_XESAM_QUERY_H__
#define __TRACKERD_XESAM_QUERY_H__

#include <glib.h>

#include <libtracker-db/tracker-db-manager.h>

G_BEGIN_DECLS

void tracker_xesam_query_to_sql (TrackerDBInterface  *iface,
				 const gchar	     *query,
				 gchar		    **from,
				 gchar		    **join,
				 gchar		    **where,
				 GError		    **error);
G_END_DECLS

#endif /* __TRACKERD_XESAM_QUERY_H__ */

