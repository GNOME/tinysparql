/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __LIBTRACKER_DBUS_H__
#define __LIBTRACKER_DBUS_H__

#ifndef DBUS_API_SUBJECT_TO_CHANGE
#define DBUS_API_SUBJECT_TO_CHANGE
#endif

#include <glib/gi18n.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define TRACKER_DBUS_ERROR_DOMAIN "TrackerDBus"
#define TRACKER_DBUS_ERROR	  tracker_dbus_error_quark()

#define tracker_dbus_async_return_if_fail(expr,context)			\
	G_STMT_START {							\
		if G_LIKELY(expr) { } else {				\
			GError *error = NULL;				\
									\
			g_set_error (&error,				\
				     TRACKER_DBUS_ERROR,		\
				     0,					\
				     _("Assertion `%s' failed"),	\
				     #expr);				\
									\
			dbus_g_method_return_error (context, error);	\
			g_clear_error (&error);				\
									\
			return;						\
		};							\
	} G_STMT_END

#define tracker_dbus_return_val_if_fail(expr,val,error)			\
	G_STMT_START {							\
		if G_LIKELY(expr) { } else {				\
			g_set_error (error,				\
				     TRACKER_DBUS_ERROR,		\
				     0,					\
				     _("Assertion `%s' failed"),	\
				     #expr);				\
									\
			return val;					\
		};							\
	} G_STMT_END

typedef struct TrackerDBusRequestHandler TrackerDBusRequestHandler;

typedef void (*TrackerDBusRequestFunc)	 (guint    request_id,
					  gpointer user_data);

typedef struct {
	guint	 id;
	gpointer data1;
	gpointer data2;
} TrackerDBusData;

GQuark		 tracker_dbus_error_quark	     (void);
TrackerDBusData *tracker_dbus_data_new		     (const gpointer		  arg1,
						      const gpointer		  arg2);

/* Utils */
GValue *	 tracker_dbus_gvalue_slice_new	     (GType			  type);
void		 tracker_dbus_gvalue_slice_free      (GValue			 *value);
gchar **	 tracker_dbus_slist_to_strv	     (GSList			 *list);
gchar **	 tracker_dbus_queue_str_to_strv      (GQueue			 *queue,
						      gint			  max);
gchar **	 tracker_dbus_queue_gfile_to_strv    (GQueue			 *queue,
						      gint			  max);
void		 tracker_dbus_results_ptr_array_free (GPtrArray			**ptr_array);

/* Requests */
guint		 tracker_dbus_get_next_request_id    (void);

TrackerDBusRequestHandler *
		 tracker_dbus_request_add_hook	     (TrackerDBusRequestFunc	  new,
						      TrackerDBusRequestFunc	  done,
						      gpointer			  user_data);
void		 tracker_dbus_request_remove_hook    (TrackerDBusRequestHandler  *handler);

void		 tracker_dbus_request_new	     (gint			  request_id,
						      const gchar		 *format,
						      ...);
void		 tracker_dbus_request_success	     (gint			  request_id);
void		 tracker_dbus_request_failed	     (gint			  request_id,
						      GError			**error,
						      const gchar		 *format,
						      ...);
void		 tracker_dbus_request_comment	     (gint			  request_id,
						      const gchar		 *format,
						      ...);

G_END_DECLS

#endif /* __LIBTRACKER_DBUS_H__ */
