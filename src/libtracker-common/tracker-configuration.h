/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Saleem Abdulrasool (compnerd@compnerd.org)
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

#ifndef __LIBTRACKER_COMMON_CONFIGURATION_H__
#define __LIBTRACKER_COMMON_CONFIGURATION_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _LanguageMapEntry {
	const gchar * const code;
	const gchar * const language;
} LanguageMapEntry;

extern const LanguageMapEntry LanguageMap[];

void	 tracker_configuration_load	   (void);
void	 tracker_configuration_save	   (void);
void	 tracker_configuration_free	   (void);
gboolean tracker_configuration_get_boolean (const gchar * const   key,
					    GError		**error);
void	 tracker_configuration_set_boolean (const gchar * const   key,
					    const gboolean	  value);
gint	 tracker_configuration_get_integer (const gchar * const   key,
					    GError		**error);
void	 tracker_configuration_set_integer (const gchar * const   key,
					    const gint		  value);
gchar *  tracker_configuration_get_string  (const gchar * const   key,
					    GError		**error);
void	 tracker_configuration_set_string  (const gchar * const   key,
					    const gchar * const   value);
GSList * tracker_configuration_get_list    (const gchar * const   key,
					    GType		  g_type,
					    GError		**error);
void	 tracker_configuration_set_list    (const gchar * const   key,
					    GType		  g_type,
					    GSList		 *value);

G_END_DECLS

#endif /* LIBTRACKER_COMMON_CONFIGURATION */
