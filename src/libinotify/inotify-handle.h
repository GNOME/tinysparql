/*
 * inotify-handle.h - a structure to represent a client-side watch
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

#ifndef _libinotify_inotify_handle_h_
#define _libinotify_inotify_handle_h_

#define IN_FLAG_NONE			0x00000000
#define IN_FLAG_FILE_BASED		0x00000001
#define IN_FLAG_SYNTH_CREATE		0x00000002
#define IN_FLAG_SYNTH_DELETE		0x00000004

#define IN_SYNTHETIC			0x00001000

#include <glib.h>

typedef struct _INotifyHandle INotifyHandle;
typedef void (*INotifyCallback)( INotifyHandle *inh,
				 const char *monitor_name,
				 const char *filename,
				 guint32 event_type,
				 guint32 cookie,
				 gpointer user_data );

INotifyHandle *inotify_handle_new( const char *filename, guint32 mask,
				   unsigned long flags );
void inotify_handle_ref( INotifyHandle *inh );
void inotify_handle_unref( INotifyHandle *inh );
gint32 inotify_handle_get_wd( INotifyHandle *inh );
void inotify_handle_set_wd( INotifyHandle *inh, gint32 wd );
guint32 inotify_handle_get_mask( INotifyHandle *inh );
const char *inotify_handle_get_filename( INotifyHandle *inh );
const char *inotify_handle_get_basename( INotifyHandle *inh );
void inotify_handle_set_parent( INotifyHandle *inh, INotifyHandle *parent );
INotifyHandle *inotify_handle_get_parent( INotifyHandle *inh );
void inotify_handle_set_callback( INotifyHandle *inh, INotifyCallback callback,
				  gpointer user_data );
void inotify_handle_invoke_callback( INotifyHandle *inh, const char *filename,
				     guint32 event_type, guint32 cookie );

#endif /* _libinotify_inotify_handle_h_ */
