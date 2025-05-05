/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2010, Nokia <ivan.frade@nokia.com>
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

#pragma once

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

typedef enum  {
	TRACKER_DATE_ERROR_OFFSET,
	TRACKER_DATE_ERROR_INVALID_ISO8601,
	TRACKER_DATE_ERROR_EMPTY
} TrackerDateError;

#define TRACKER_TYPE_DATE_TIME                 (tracker_date_time_get_type ())
#define TRACKER_DATE_ERROR                     tracker_date_error_quark ()

GQuark   tracker_date_error_quark              (void);

GDateTime * tracker_date_new_from_iso8601 (GTimeZone    *tz,
                                           const gchar  *string,
                                           GError      **error);
gchar * tracker_date_format_iso8601 (GDateTime *datetime);

G_END_DECLS
