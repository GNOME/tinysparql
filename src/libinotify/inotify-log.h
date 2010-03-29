/*
 * inotify-log.h - internal libinotify logging/debugging functions
 * Copyright © 2005 Ryan Lortie <desrt@desrt.ca>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _libinotify_inotify_log_h_
#define _libinotify_inotify_log_h_

#include <stdlib.h>
#include <glib.h>

#include "config.h"

#ifdef LIBINOTIFY_DEBUG
# define inotify_debug(x, ...) g_log("libinotify", G_LOG_LEVEL_DEBUG, \
				     x, ## __VA_ARGS__)
#else
# define inotify_debug(x, ...) 
#endif

#define inotify_warn(x, ...) g_log("libinotify", G_LOG_LEVEL_WARNING, \
				   x, ## __VA_ARGS__)

#define inotify_fatal(x, ...) g_log("libinotify", G_LOG_LEVEL_ERROR, \
				    x, ## __VA_ARGS__)

static inline void
inotify_debug_initialise()
{
#ifdef LIBINOTIFY_DEBUG
  if( getenv( "DEBUG_LIBINOTIFY" ) == NULL )
    g_log_set_handler( "libinotify", G_LOG_LEVEL_DEBUG,
		       (GLogFunc) strlen, NULL );
#endif
}

#endif /* _libinotify_inotify_log_h_ */
