/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __LIBTRACKER_COMMON_DBUS_GLIB_H__
#define __LIBTRACKER_COMMON_DBUS_GLIB_H__

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include "tracker-dbus.h"

G_BEGIN_DECLS

#define tracker_dbus_async_return_if_fail(expr,context)	\
	G_STMT_START { \
		if G_LIKELY(expr) { } else { \
			GError *assert_error = NULL; \
	  \
			g_set_error (&assert_error, \
			             TRACKER_DBUS_ERROR, \
			             TRACKER_DBUS_ERROR_ASSERTION_FAILED, \
			             _("Assertion `%s' failed"), \
			             #expr); \
	  \
			dbus_g_method_return_error (context, assert_error); \
			g_clear_error (&assert_error); \
	  \
			return; \
		}; \
	} G_STMT_END

#define tracker_dbus_return_val_if_fail(expr,val,error)	\
	G_STMT_START { \
		if G_LIKELY(expr) { } else { \
			g_set_error (error, \
			             TRACKER_DBUS_ERROR, \
			             TRACKER_DBUS_ERROR_ASSERTION_FAILED, \
			             _("Assertion `%s' failed"), \
			             #expr); \
	  \
			return val; \
		}; \
	} G_STMT_END

/* dbus-glib convenience API */
TrackerDBusRequest *tracker_dbus_g_request_begin       (DBusGMethodInvocation      *context,
                                                        const gchar                *format,
                                                        ...);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_DBUS_GLIB_H__ */
