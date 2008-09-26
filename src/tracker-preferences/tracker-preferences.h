/* Tracker - indexer and metadata database engine
 * Copyright (C) 2007, Saleem Abdulrasool (compnerd@gentoo.org)
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

#ifndef __TRACKER_PREFERENCES_H__
#define __TRACKER_PREFERENCES_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_PREFERENCES	     (tracker_preferences_get_type())
#define TRACKER_PREFERENCES(obj)	     (G_TYPE_CHECK_INSTANCE_CAST((obj), TRACKER_TYPE_PREFERENCES, TrackerPreferences))
#define TRACKER_PREFERENCES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), TRACKER_TYPE_PREFERENCES, TrackerPreferencesClass))
#define TRACKER_IS_PREFERENCES(obj)	     (G_TYPE_CHECK_INSTANCE_TYPE((obj), TRACKER_TYPE_PREFERENCES))
#define TRACKER_IS_PREFERENCES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), TRACKER_TYPE_PREFERENCES))
#define TRACKER_PREFERENCES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), TRACKER_TYPE_PREFERENCES, TrackerPreferencesClass))

typedef struct _TrackerPreferences {
	GObject parent;
} TrackerPreferences;

typedef struct _TrackerPreferencesClass {
	GObjectClass parent_class;
} TrackerPreferencesClass;

GType
tracker_preferences_get_type (void);

TrackerPreferences *
tracker_preferences_new (void);

G_END_DECLS

#endif
