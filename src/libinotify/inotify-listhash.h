/*
 * inotify-listhash.h - a structure to map wd's to client-side watches
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

#ifndef _libinotify_inotify_listhash_h_
#define _libinotify_inotify_listhash_h_

#include "inotify-handle.h"

GSList *inotify_listhash_get( gint32 wd );
int inotify_listhash_remove( INotifyHandle *inh );
void inotify_listhash_append( INotifyHandle *inh, gint32 wd );
int inotify_listhash_ignore( gint32 wd );
int inotify_listhash_length( gint32 wd );
guint32 inotify_listhash_get_mask( gint32 wd );
void inotify_listhash_initialise( void );
void inotify_listhash_destroy( void );

#endif /* _libinotify_inotify_lasthash_h_ */
