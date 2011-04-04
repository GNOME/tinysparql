/*
 * Copyright (C) 2010 Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_COMMON_LOCALE_H__
#define __LIBTRACKER_COMMON_LOCALE_H__

#include <glib.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

/* Type of locales supported in tracker */
typedef enum {
	TRACKER_LOCALE_LANGUAGE,
	TRACKER_LOCALE_TIME,
	TRACKER_LOCALE_COLLATE,
	TRACKER_LOCALE_NUMERIC,
	TRACKER_LOCALE_MONETARY,
	TRACKER_LOCALE_LAST
} TrackerLocaleID;

/* Callback for the notification of locale changes */
typedef void (* TrackerLocaleNotifyFunc)  (TrackerLocaleID id,
                                           gpointer user_data);

/* Get the current locale of the given type.
 * Note that it returns a newly-allocated string which should be g_free()-ed
 */
gchar       *tracker_locale_get           (TrackerLocaleID id);

/* Adds a new subscriber to locale change notifications.
 * Returns a pointer which identifies the subscription.
 */
gpointer     tracker_locale_notify_add    (TrackerLocaleID         id,
                                           TrackerLocaleNotifyFunc func,
                                           gpointer                user_data,
                                           GFreeFunc               destroy_notify);

/* Remove a given subscriber, passing the id you got in _add() */
void         tracker_locale_notify_remove (gpointer                notification_id);

const gchar* tracker_locale_get_name      (guint                   i);
void         tracker_locale_set           (TrackerLocaleID         id,
                                           const gchar            *value);

void         tracker_locale_init          (void);
void         tracker_locale_shutdown      (void);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_LOCALE_H__ */
