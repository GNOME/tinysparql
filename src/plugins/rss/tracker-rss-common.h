/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_RSS_COMMON_H__
#define __TRACKER_RSS_COMMON_H__

/* Latest spec location: http://live.gnome.org/Rss/Metadata */

#define TRACKER_RSS_INDEXER_PATH		"/org/freedesktop/Tracker/Indexer/Rss/Registrar"

#define TRACKER_RSS_REGISTRAR_SERVICE		"org.freedesktop.Tracker"
#define TRACKER_RSS_REGISTRAR_PATH		"/org/freedesktop/Tracker/Rss/Registrar"
#define TRACKER_RSS_REGISTRAR_INTERFACE		"my.rss.metadata.Registrar"

#define TRACKER_RSS_MANAGER_SERVICE		"my.rss.service"
#define TRACKER_RSS_MANAGER_PATH		"/my/rss/metadata/Manager"
#define TRACKER_RSS_MANAGER_INTERFACE		"my.rss.metadata.Manager"

#define DBUS_ERROR_DOMAIN			"TrackerRss"
#define DBUS_ERROR				g_quark_from_static_string (DBUS_ERROR_DOMAIN)

#define TRACKER_RSS_PREDICATE_THING		"RssMeta:Thing"

#define TRACKER_TYPE_G_STRV_ARRAY		(dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV))

#define dbus_async_return_if_fail(expr,context)				\
	G_STMT_START {							\
		if G_LIKELY(expr) { } else {				\
			GError *error = NULL;				\
									\
			g_set_error (&error,				\
				     DBUS_ERROR,			\
				     0,					\
				     "Assertion `%s' failed",		\
				     #expr);				\
									\
			dbus_g_method_return_error (context, error);	\
			g_error_free (error);				\
									\
			return;						\
		};							\
	} G_STMT_END

#endif /* __TRACKER_RSS_COMMON_H__ */
