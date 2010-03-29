/*
 * inotify-monitor.h - the primary interface for adding/removing watches
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

#ifndef _libinotify_inotify_monitor_h_
#define _libinotify_inotify_monitor_h_

#include "inotify-handle.h"

INotifyHandle *inotify_monitor_add( const char *filename,
				    guint32 mask,
				    unsigned long flags,
				    INotifyCallback callback,
				    gpointer user_data );
void inotify_monitor_remove( INotifyHandle *inh );
gboolean inotify_is_available( void );

#endif /* _libinotify_inotify_monitor_h_ */
