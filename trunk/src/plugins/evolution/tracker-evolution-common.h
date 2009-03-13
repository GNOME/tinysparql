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

#ifndef __TRACKER_EVOLUTION_COMMON_H__
#define __TRACKER_EVOLUTION_COMMON_H__

/* Latest spec location: http://live.gnome.org/Evolution/Metadata */

#define TRACKER_EVOLUTION_INDEXER_PATH		"/org/freedesktop/Tracker/Indexer/Evolution/Registrar"

#define TRACKER_EVOLUTION_REGISTRAR_SERVICE	"org.freedesktop.Tracker"
#define TRACKER_EVOLUTION_REGISTRAR_PATH	"/org/freedesktop/Tracker/Evolution/Registrar"
#define TRACKER_EVOLUTION_REGISTRAR_INTERFACE	"org.freedesktop.email.metadata.Registrar"

#define TRACKER_EVOLUTION_MANAGER_SERVICE	"org.gnome.evolution"
#define TRACKER_EVOLUTION_MANAGER_PATH		"/org/freedesktop/email/metadata/Manager"
#define TRACKER_EVOLUTION_MANAGER_INTERFACE	"org.freedesktop.email.metadata.Manager"

#define DBUS_ERROR_DOMAIN			"TrackerEvolution"
#define DBUS_ERROR				g_quark_from_static_string (DBUS_ERROR_DOMAIN)

#define TRACKER_EVOLUTION_PREDICATE_SUBJECT	"EMailMeta:MessageSubject"
#define TRACKER_EVOLUTION_PREDICATE_SENT	"EMailMeta:MessageSent"
#define TRACKER_EVOLUTION_PREDICATE_FROM	"EMailMeta:MessageFrom"
#define TRACKER_EVOLUTION_PREDICATE_TO		"EMailMeta:MessageTo"
#define TRACKER_EVOLUTION_PREDICATE_CC		"EMailMeta:MessageCc"
#define TRACKER_EVOLUTION_PREDICATE_SEEN	"EMailMeta:MessageSeen"
#define TRACKER_EVOLUTION_PREDICATE_ANSWERED	"EMailMeta:MessageAnswered"
#define TRACKER_EVOLUTION_PREDICATE_FORWARDED	"EMailMeta:MessageForwarded"
#define TRACKER_EVOLUTION_PREDICATE_DELETED	"EMailMeta:MessageDeleted"
#define TRACKER_EVOLUTION_PREDICATE_SIZE	"EMailMeta:MessageSize"

#define TRACKER_EVOLUTION_PREDICATE_JUNK	"Evolution:MessageJunk"
#define TRACKER_EVOLUTION_PREDICATE_FILE	"Evolution:MessageFile"
#define TRACKER_EVOLUTION_PREDICATE_UID		"Evolution:MessageUid"
#define TRACKER_EVOLUTION_PREDICATE_FLAGGED	"Evolution:MessageFlagged"
#define TRACKER_EVOLUTION_PREDICATE_TAG		"Evolution:MessageTag"

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

#endif /* __TRACKER_EVOLUTION_COMMON_H__ */
