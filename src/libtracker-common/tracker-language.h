/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#ifndef __TRACKER_LANGUAGE_H__
#define __TRACKER_LANGUAGE_H__

#include "tracker-config.h"

G_BEGIN_DECLS

typedef struct _TrackerLanguage TrackerLanguage;

TrackerLanguage *tracker_language_new              (TrackerConfig   *config);
void             tracker_language_free             (TrackerLanguage *language);

gboolean         tracker_language_check_exists     (const gchar     *language_code);
gchar *          tracker_language_get_default_code (void);
gchar *          tracker_language_stem_word        (TrackerLanguage *language,
						    const gchar     *word,
						    gint             word_length);

G_END_DECLS

#endif /* __TRACKER_LANGUAGE_H__ */
