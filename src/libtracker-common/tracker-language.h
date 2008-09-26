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

#ifndef __LIBTRACKER_COMMON_LANGUAGE_H__
#define __LIBTRACKER_COMMON_LANGUAGE_H__

#include <glib-object.h>

#include "tracker-config.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_LANGUAGE	      (tracker_language_get_type ())
#define TRACKER_LANGUAGE(o)	      (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_LANGUAGE, TrackerLanguage))
#define TRACKER_LANGUAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_LANGUAGE, TrackerLanguageClass))
#define TRACKER_IS_LANGUAGE(o)	      (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_LANGUAGE))
#define TRACKER_IS_LANGUAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_LANGUAGE))
#define TRACKER_LANGUAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_LANGUAGE, TrackerLanguageClass))

typedef struct _TrackerLanguage      TrackerLanguage;
typedef struct _TrackerLanguageClass TrackerLanguageClass;

struct _TrackerLanguage {
	GObject      parent;
};

struct _TrackerLanguageClass {
	GObjectClass parent_class;
};

GType		 tracker_language_get_type	   (void) G_GNUC_CONST;

TrackerLanguage *tracker_language_new		   (TrackerConfig   *language);
TrackerConfig *  tracker_language_get_config	   (TrackerLanguage *language);
GHashTable *	 tracker_language_get_stop_words   (TrackerLanguage *language);
void		 tracker_language_set_config	   (TrackerLanguage *language,
						    TrackerConfig   *config);
const gchar *	 tracker_language_stem_word	   (TrackerLanguage *language,
						    const gchar     *word,
						    gint	     word_length);

/* Utility functions */
gboolean	 tracker_language_check_exists	   (const gchar     *language_code);
gchar *		 tracker_language_get_default_code (void);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_LANGUAGE_H__ */
