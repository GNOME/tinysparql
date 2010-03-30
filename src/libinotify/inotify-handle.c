/*
 * inotify-handle.c - a structure to represent a client-side watch
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
#include <glib.h>
#include <sys/inotify.h>

#include "inotify-handle.h"
#include "inotify-log.h"

enum inh_state
{
  inh_state_initial,
  inh_state_deleted,
  inh_state_created
};

struct _INotifyHandle
{
  int refcount;
  INotifyCallback callback;
  gpointer user_data;
  gint32 wd;
  guint32 mask;
  char *filename;
  unsigned long flags;
  enum inh_state state;
  INotifyHandle *parent;
};

INotifyHandle *
inotify_handle_new( const char *filename, guint32 mask, unsigned long flags )
{
  INotifyHandle *inh;

  inh = g_new( INotifyHandle, 1 );
  inh->wd = -1;
  inh->mask = mask;
  inh->filename = g_strdup( filename );
  inh->refcount = 1;
  inh->callback = NULL;
  inh->user_data = NULL;
  inh->flags = flags;
  inh->state = inh_state_initial;
  inh->parent = NULL;

  inotify_debug( "New handle %p on %s", inh, inh->filename );

  return inh;
}

void
inotify_handle_ref( INotifyHandle *inh )
{
  inotify_debug( "Ref handle %p on %s", inh, inh->filename );

  g_atomic_int_inc( &inh->refcount );
}

void
inotify_handle_unref( INotifyHandle *inh )
{
  inotify_debug( "Deref handle %p on %s", inh, inh->filename );

  if( g_atomic_int_dec_and_test( &inh->refcount ) )
  {
    inotify_debug( "  and destroy" );
    g_free( inh->filename );
    g_free( inh );
  }
}

gint32
inotify_handle_get_wd( INotifyHandle *inh )
{
  return inh->wd;
}

void
inotify_handle_set_wd( INotifyHandle *inh, gint32 wd )
{
  inh->wd = wd;
}

guint32
inotify_handle_get_mask( INotifyHandle *inh )
{
  return inh->mask;
}

const char *
inotify_handle_get_filename( INotifyHandle *inh )
{
  return inh->filename;
}

const char *
inotify_handle_get_basename( INotifyHandle *inh )
{
  const char *bn;

  bn = rindex( inh->filename, '/' );

  if( bn == NULL )
    return NULL;

  if( bn == inh->filename )
    return bn;

  return bn + 1;
}

void
inotify_handle_set_parent( INotifyHandle *inh, INotifyHandle *parent )
{
  inh->parent = parent;
}

INotifyHandle *
inotify_handle_get_parent( INotifyHandle *inh )
{
  return inh->parent;
}

void
inotify_handle_set_callback( INotifyHandle *inh, INotifyCallback callback,
			     gpointer user_data )
{
  inh->callback = callback;
  inh->user_data = user_data;
}

static guint32
inotify_handle_event_applicable( INotifyHandle *inh, guint32 type,
				 const char *filename )
{
  enum inh_state state = inh->state;

  inotify_debug( "Juding applicability of event %x on %p/%s",
		 type, inh, filename );

  if( type & IN_SYNTHETIC )
  {
    inotify_debug( "  event is synthetic" );

    /* Synthetic events should not be delivered except as the first event. */
    if( state != inh_state_initial )
    {
      inotify_debug( "	dropping synthetic event on non-initial state" );
      return 0;
    }

    /* Synthetic create event... */
    if( type & IN_CREATE )
    {
      inotify_debug( "	synthetic create event" );

      inh->state = inh_state_created;

      /* Only deliver if the user wants to receive synthetic create events. */
      if( inh->flags & IN_FLAG_SYNTH_CREATE )
      {
	inotify_debug( "  user wants it -- delivering" );
	return IN_CREATE | (IN_SYNTHETIC & inh->mask);
      }
      else
      {
	inotify_debug( "  user doesn't want it -- dropping" );
	return 0;
      }
    }

    if( type & IN_DELETE )
    {
      inotify_debug( "	synthetic delete event" );
      inh->state = inh_state_deleted;

      /* Only deliver if the user wants to receive synthetic delete events. */
      if( inh->flags & IN_FLAG_SYNTH_DELETE )
      {
	inotify_debug( "  user wants it -- delivering" );
	return IN_DELETE | (IN_SYNTHETIC & inh->mask);
      }
      else
      {
	inotify_debug( "  user doesn't want it -- dropping" );
	return 0;
      }
    }

    inotify_warn( "Invalid synthetic event" );
    return 0;
  }

  /* Non-synthetic events. */
  type &= inh->mask;

  /* Event occured on a file in a directory -- not the object itself. */
  if( filename != NULL )
  {
    if( state != inh_state_created )
      inotify_warn( "Received directory event on non-created inh" );

    inotify_debug( "  event is on file -- passing through" );

    return type;
  }

  /* Else, non-synthetic event directly on the watched object. */
  switch( type )
  {
    case IN_CREATE:
    case IN_MOVED_TO:
      inh->state = inh_state_created;

      if( state == inh_state_created )
	inotify_warn( "Create on already-existing file" );

      inotify_debug( "	event is create-type.  passing through" );

      return type;

    case IN_DELETE:
    case IN_DELETE_SELF:
    case IN_MOVED_FROM:
      inh->state = inh_state_deleted;

      if( state == inh_state_deleted )
      {
	inotify_debug( "  dropping remove event on already-removed file" );
	return 0;
      }

      inotify_debug( "	event is delete-type.  passing through" );

      return type;

    default:
      /* if( state != inh_state_created ) */
      /*   inotify_warn( "Received direct event on non-created inh" ); */

      inotify_debug( "	event is other type.  passing through" );

      return type;
  }
}

void
inotify_handle_invoke_callback( INotifyHandle *inh, const char *filename,
				guint32 type, guint32 cookie )
{
  type = inotify_handle_event_applicable( inh, type, filename );

  if( type != 0 && inh->callback )
    inh->callback( inh, inh->filename, filename, type,
		   cookie, inh->user_data );
}
