/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __LIBTRACKER_COMMON_OS_DEPENDANT_H__
#define __LIBTRACKER_COMMON_OS_DEPENDANT_H__

#include <glib.h>
#include <glib/gstdio.h>

/* Process spawning */
gboolean tracker_spawn			   (gchar	**argv,
					    gint	  timeout,
					    gchar	**tmp_stdout,
					    gint	 *exit_status);
gboolean tracker_spawn_async_with_channels (const gchar **argv,
					    gint	  timeout,
					    GPid	 *pid,
					    GIOChannel	**stdin_channel,
					    GIOChannel	**stdout_channel,
					    GIOChannel	**stderr_channel);
void	 tracker_spawn_child_func	   (gpointer	  user_data);

/* File permissions */
gchar *  tracker_create_permission_string  (struct stat   finfo);

/* Memory limits */
gboolean tracker_memory_setrlimits (void);

#endif /* __LIBTRACKER_COMMON_OS_DEPENDANT_H__ */
