/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#define TRACKER_TYPE_LANGUAGE         (tracker_language_get_type ())
#define TRACKER_LANGUAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_LANGUAGE, TrackerLanguage))
#define TRACKER_LANGUAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_LANGUAGE, TrackerLanguageClass))
#define TRACKER_IS_LANGUAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_LANGUAGE))
#define TRACKER_IS_LANGUAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_LANGUAGE))
#define TRACKER_LANGUAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_LANGUAGE, TrackerLanguageClass))

typedef struct _TrackerLanguage TrackerLanguage;
typedef struct _TrackerLanguageClass TrackerLanguageClass;

struct _TrackerLanguage {
	GObject parent;
};

struct _TrackerLanguageClass {
	GObjectClass parent_class;
};

GType            tracker_language_get_type           (void) G_GNUC_CONST;
TrackerLanguage *tracker_language_new                (const gchar     *language_code);

void tracker_language_stem_word (TrackerLanguage *language,
                                 gchar           *buffer,
                                 gint            *buffer_len,
                                 gint             buffer_size);

G_END_DECLS
