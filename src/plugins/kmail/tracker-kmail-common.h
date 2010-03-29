/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_KMAIL_COMMON_H__
#define __TRACKER_KMAIL_COMMON_H__

/* Latest spec location: http://live.kde.org/Evolution/Metadata */

#define TRACKER_KMAIL_INDEXER_PATH              "/org/freedesktop/Tracker1/Indexer/KMail/Registrar"

#define TRACKER_KMAIL_REGISTRAR_SERVICE                 "org.freedesktop.Tracker1"
#define TRACKER_KMAIL_REGISTRAR_PATH            "/org/freedesktop/Tracker1/KMail/Registrar"
#define TRACKER_KMAIL_REGISTRAR_INTERFACE       "org.freedesktop.email.metadata.Registrar"

#define TRACKER_KMAIL_MANAGER_SERVICE           "org.kde.kmail"
#define TRACKER_KMAIL_MANAGER_PATH              "/org/freedesktop/email/metadata/Manager"
#define TRACKER_KMAIL_MANAGER_INTERFACE                 "org.freedesktop.email.metadata.Manager"

#define DBUS_ERROR_DOMAIN                       "TrackerKMail"
#define DBUS_ERROR                              g_quark_from_static_string (DBUS_ERROR_DOMAIN)

#define TRACKER_KMAIL_PREDICATE_SUBJECT                 "EMailMeta:MessageSubject"
#define TRACKER_KMAIL_PREDICATE_SENT            "EMailMeta:MessageSent"
#define TRACKER_KMAIL_PREDICATE_FROM            "EMailMeta:MessageFrom"
#define TRACKER_KMAIL_PREDICATE_TO              "EMailMeta:MessageTo"
#define TRACKER_KMAIL_PREDICATE_CC              "EMailMeta:MessageCc"
#define TRACKER_KMAIL_PREDICATE_SEEN            "EMailMeta:MessageSeen"
#define TRACKER_KMAIL_PREDICATE_ANSWERED        "EMailMeta:MessageAnswered"
#define TRACKER_KMAIL_PREDICATE_FORWARDED       "EMailMeta:MessageForwarded"
#define TRACKER_KMAIL_PREDICATE_DELETED                 "EMailMeta:MessageDeleted"
#define TRACKER_KMAIL_PREDICATE_SIZE            "EMailMeta:MessageSize"

#define TRACKER_KMAIL_PREDICATE_IDMD5           "KMail:MessageIdMD5"
#define TRACKER_KMAIL_PREDICATE_UID             "KMail:MessageUID"
#define TRACKER_KMAIL_PREDICATE_TAG             "KMail:MessageTag"
#define TRACKER_KMAIL_PREDICATE_SERNUM          "KMail:MessageSerNum"
#define TRACKER_KMAIL_PREDICATE_SPAM            "KMail:MessageSpam"
#define TRACKER_KMAIL_PREDICATE_HAM             "KMail:MessageHam"

#define dbus_async_return_if_fail(expr,context)	  \
	G_STMT_START { \
		if G_LIKELY(expr) { } else { \
			GError *error = NULL; \
                                                                        \
			g_set_error (&error, \
			             DBUS_ERROR, \
			             0, \
			             "Assertion `%s' failed", \
			             #expr); \
                                                                        \
			dbus_g_method_return_error (context, error); \
			g_error_free (error); \
                                                                        \
			return; \
		}; \
	} G_STMT_END

#endif /* __TRACKER_KMAIL_COMMON_H__ */
