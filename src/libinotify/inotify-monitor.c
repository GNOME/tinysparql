/*
 * inotify-monitor.c - the primary interface for adding/removing watches
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

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "inotify-handle.h"
#include "inotify-listhash.h"
#include "inotify-log.h"

#include "inotify-monitor.h"

GStaticRecMutex inotify_monitor_lock = G_STATIC_REC_MUTEX_INIT;
static int inotify_monitor_fd = -1;

static void
process_one_event( struct inotify_event *ine )
{
  const char *filename;
  GSList *list;

  if( ine->len )
    filename = ine->name;
  else
    filename = NULL;

  inotify_debug( "Got one event" );

  list = inotify_listhash_get( ine->wd );
  while (list)
  {
    GSList *next = list->next;
    inotify_debug( "  dispatch to %p", list->data );
    inotify_handle_invoke_callback( list->data, filename,
				    ine->mask, ine->cookie );
    /* Note that AFTER executing the callback, both the list element and the
     *  INotifyHandle may already be disposed. So, the pointer to the next
     *  list element should have been stored before calling the callback */
    list = next;
  }

  if( ine->mask & IN_IGNORED )
    inotify_listhash_ignore( ine->wd );
}

static gboolean
inotify_watch_func( GIOChannel *source, GIOCondition condition, gpointer data )
{
  struct inotify_event ine[20];
  int size, namesize;
  int fd;

  g_static_rec_mutex_lock( &inotify_monitor_lock );

  fd = g_io_channel_unix_get_fd( source );

  while( (size = read( fd, ine, sizeof ine )) >= 0 )
  {
    inotify_debug( "Original size %d", size );
    size /= sizeof *ine;

    inotify_debug( "Got size %d", size );

    while( size > 0 )
    {
      /* Division, rounding up. */
      namesize = (ine->len + sizeof *ine - 1) / sizeof *ine + 1;

      if( namesize > size )
      {
	// XXX might be false if lots of events stack up
	inotify_fatal( "namesize > size!" );
      }

      size -= namesize;

      /* Add some sort of yield to the main loop. */
      while (g_main_context_pending (NULL)) {
              g_main_context_iteration (NULL, FALSE);
      }

      process_one_event( ine );
      memmove( ine, &ine[namesize], sizeof *ine * size );
    }
  }

  g_static_rec_mutex_unlock( &inotify_monitor_lock );

  return TRUE;
}

static int
inotify_monitor_initialise( void )
{
  GIOChannel *gio;

  if( inotify_monitor_fd != -1 )
    return 0;

  inotify_monitor_fd = inotify_init();

  if( inotify_monitor_fd < 0 )
    return -1;

  inotify_listhash_initialise();

  gio = g_io_channel_unix_new( inotify_monitor_fd );
  g_io_add_watch( gio, G_IO_IN, inotify_watch_func, NULL );
  g_io_channel_set_flags( gio, G_IO_FLAG_NONBLOCK, NULL );

  inotify_debug_initialise();

  return 0;
}

static int
inotify_monitor_add_raw( INotifyHandle *inh )
{
  const char *filename = inotify_handle_get_filename( inh );
  guint32 mask = inotify_handle_get_mask( inh );
#ifndef IN_MASK_ADD
  guint32 needmask;
  int wd2;
#endif
  int wd;

#ifdef IN_MASK_ADD
  wd = inotify_add_watch( inotify_monitor_fd, filename, mask | IN_MASK_ADD );
#else
  wd = inotify_add_watch( inotify_monitor_fd, filename, mask );

  if( wd < 0 )
    return -1;

  needmask = mask | inotify_listhash_get_mask( wd );

  if( needmask != mask )
  {
    /* This can only happen if we've already been watching the inode and we
     * just requested another watch on it with fewer events.  We now have
     * to change the watch mask to restore the events we just blew away.
     */

    /* *Very* slight risk of race condition here if 'filename' has
     * disappeared or changed inodes since last inotify_add_watch call.
     */
    wd2 = inotify_add_watch( inotify_monitor_fd, filename, needmask );

    /* If this happens, we're in trouble no matter how you look at it since
     * we have no way of giving the proper mask to the inode.  Even worse,
     * we might have just screwed up the mask on another inode.  Find out.
     */
    if( wd2 != wd )
      inotify_warn( "race condition in inotify_monitor_add! (%d vs %d)",
								 wd, wd2 );

    if( wd2 < 0 )
    {
      /* File has since disappeared -- nothing we can do! */
    }
    else if( wd2 != wd )
    {
      /* File has changed inode.  Even worse! */

      if( inotify_listhash_length( wd2 ) == 0 )
      {
	/* We're not supposed to be watching this inode. */
	inotify_rm_watch( inotify_monitor_fd, wd2 );
      }
      else
      {
	/* If we did hit an inode we're already watching then we just
	 * modified its mask.  Ow.  We could go hunting with the list of
	 * filenames that we have in the listhash in hopes that one of them
	 * still references the inode that we're looking for but this is
	 * such a rare case and going hunting is likely to cause further
	 * errors anyway...
	 */
      }
    }

    /* We've either fixed it or can't fix it.  Proceed... */
  }
#endif

  if( wd < 0 )
    return -1;

  inotify_listhash_append( inh, wd );

  return 0;
}

static void
inotify_monitor_remove_raw( INotifyHandle *inh )
{
  if( inotify_listhash_remove( inh ) == 0 )
  {
    /* No watches left, so cancel the watch with the kernel.  This Will
     * generate an IN_IGNORED event which will free the listhash stuff.
     */
    inotify_rm_watch( inotify_monitor_fd, inotify_handle_get_wd( inh ) );
  }

  /* We currently have no way of safely reducing the event mask on an
   * inode that we're watching.  As such, just leave it alone.	This means
   * that we'll receive extra events (which we'll filter out), but at least
   * we won't potentially put ourselves in an inconsistent state.
   */
}

static void
inotify_internal_callback( INotifyHandle *inh, const char *monitor_name,
			   const char *filename, guint32 event_type,
			   guint32 cookie, gpointer user_data )
{
  INotifyHandle *child = user_data;
  int result;

  inotify_debug( "Got event for %s:%x while watching for %s on %s",
		 filename, event_type, inotify_handle_get_basename( child ),
		 monitor_name );

  event_type &= ~IN_ISDIR;

  if( !filename )
  {
    switch( event_type & ~IN_SYNTHETIC )
    {
      case IN_CREATE:
      case IN_MOVED_TO:
	result = inotify_monitor_add_raw( child );

	/* If child exists... */
	if( result == 0 )
	  inotify_handle_invoke_callback( child, NULL, event_type, cookie );

	break;

      case IN_DELETE:
      case IN_DELETE_SELF:
      case IN_MOVE_SELF:
      case IN_MOVED_FROM:
	/* Parent just disappeared.  Report that we've also been deleted. */
	inotify_handle_invoke_callback( child, NULL, event_type, cookie );

	/* Then unregister our watch with the kernel. */
	inotify_monitor_remove_raw( child );

	break;
	default:
	   break;

    }

    return;
  }

  if( strcmp( inotify_handle_get_basename( child ), filename ) )
    return;

  switch( event_type )
  {
    case IN_CREATE:
    case IN_MOVED_TO:
      result = inotify_monitor_add_raw( child );

      inotify_handle_invoke_callback( child, NULL, event_type, cookie );

      if( result != 0 )
	inotify_handle_invoke_callback( child, NULL, IN_DELETE, cookie );

      break;

    case IN_DELETE:
    case IN_MOVED_FROM:
      /* We just disappeared.  Report that we've been deleted.	We must
       * send the event manually since the remove_raw might cause the
       * event not to be delivered normally.
       */
      inotify_handle_invoke_callback( child, NULL, event_type, cookie );

      /* Then unregister our watch with the kernel. */
      inotify_monitor_remove_raw( child );
      break;
    default:
      break;
  }
}

INotifyHandle *
inotify_monitor_add( const char *filename, guint32 mask, unsigned long flags,
		     INotifyCallback callback, gpointer user_data )
{
  INotifyHandle *pinh, *inh;
  int result;

  g_static_rec_mutex_lock( &inotify_monitor_lock );

  if( inotify_monitor_initialise() )
    return NULL;

  inh = inotify_handle_new( filename, mask, flags );
  inotify_handle_set_callback( inh, callback, user_data );

  if( (flags & IN_FLAG_FILE_BASED) == 0 || !strcmp( filename, "/" ) )
  {
    inotify_debug( "%s is raw", filename );
    result = inotify_monitor_add_raw( inh );

    if( result == 0 )
      inotify_handle_invoke_callback( inh, NULL, IN_CREATE | IN_SYNTHETIC, -1 );
  }
  else
  {
    const char *parent = g_path_get_dirname( filename );
    unsigned long lflags;
    guint32 lmask;

    lflags = IN_FLAG_FILE_BASED | IN_FLAG_SYNTH_CREATE;
    lmask = IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE | IN_DELETE |
	   IN_DELETE_SELF | IN_MOVE_SELF | IN_SYNTHETIC;

    inotify_debug( "Adding internal callback %p for %p(%s)",
		   inotify_internal_callback, inh, parent );

    pinh = inotify_monitor_add( parent, lmask, lflags,
				inotify_internal_callback, inh );

    inotify_handle_set_parent( inh, pinh );

    /* This will be filtered out if it shouldn't be delivered. */
    inotify_handle_invoke_callback( inh, NULL, IN_DELETE | IN_SYNTHETIC, -1 );

    result = 0;
  }

  if( result )
  {
    inotify_handle_unref( inh );
    inh = NULL;
  }

  g_static_rec_mutex_unlock( &inotify_monitor_lock );

  return inh;
}

void
inotify_monitor_remove( INotifyHandle *inh )
{
  INotifyHandle *parent;

  g_static_rec_mutex_lock( &inotify_monitor_lock );

  if( inotify_monitor_initialise() )
    goto error;

  if( inh == NULL )
    goto error;

  if( (parent = inotify_handle_get_parent( inh )) != NULL )
    inotify_monitor_remove( parent );

  inotify_monitor_remove_raw( inh );

  inotify_handle_unref( inh );

error:
  g_static_rec_mutex_unlock( &inotify_monitor_lock );
}

gboolean
inotify_is_available( void )
{
  int result;

  g_static_rec_mutex_lock( &inotify_monitor_lock );

  result = inotify_monitor_initialise();

  g_static_rec_mutex_unlock( &inotify_monitor_lock );

  return (result == 0);
}
