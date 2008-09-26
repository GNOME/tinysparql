/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Michal Pryc (Michal.Pryc@Sun.Com)
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

#ifndef __TRACKERD_UTILS_H__
#define __TRACKERD_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

gchar *  tracker_get_radix_by_suffix	    (const gchar  *str,
					     const gchar  *suffix);
void	 tracker_notify_file_data_available (void);
void	 tracker_add_metadata_to_table	    (GHashTable   *meta_table,
					     const char   *key,
					     const char   *value);
void	 tracker_add_io_grace		    (const char   *uri);

G_END_DECLS

#endif /* __TRACKERD_UTILS_H__ */
