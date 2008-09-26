/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#ifndef __LIBTRACKER_COMMON_MODULE_CONFIG_H__
#define __LIBTRACKER_COMMON_MODULE_CONFIG_H__

#include <glib-object.h>

G_BEGIN_DECLS

gboolean     tracker_module_config_init				   (void);
void	     tracker_module_config_shutdown			   (void);

GList *      tracker_module_config_get_modules			   (void);

const gchar *tracker_module_config_get_description		   (const gchar *name);
gboolean     tracker_module_config_get_enabled			   (const gchar *name);

GList *      tracker_module_config_get_monitor_directories	   (const gchar *name);
GList *      tracker_module_config_get_monitor_recurse_directories (const gchar *name);

GList *      tracker_module_config_get_ignored_directories	   (const gchar *name);
GList *      tracker_module_config_get_ignored_files		   (const gchar *name);

const gchar *tracker_module_config_get_index_service		   (const gchar *name);
GList *      tracker_module_config_get_index_mime_types		   (const gchar *name);
GList *      tracker_module_config_get_index_files		   (const gchar *name);

/* Convenience functions */
GList *      tracker_module_config_get_ignored_directory_patterns  (const gchar *name);
GList *      tracker_module_config_get_ignored_file_patterns	   (const gchar *name);
GList *      tracker_module_config_get_index_file_patterns	   (const gchar *name);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_MODULE_CONFIG_H__ */

