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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#pragma once

#include <glib.h>

#include "tracker-language.h"

/* This version MUST be bumped on any change to any tracker-parser-*
 * file. Given the parser output gets both stored in disk and performed
 * at runtime, the former must be rebuilt for those to match perfectly
 * to avoid returning meaningless results on FTS searches.
 */
#define TRACKER_PARSER_VERSION 7

G_BEGIN_DECLS

/* Parser */
typedef struct TrackerParser TrackerParser;

TrackerParser *tracker_parser_new             (void);

void           tracker_parser_reset           (TrackerParser   *parser,
                                               const gchar     *txt,
                                               gint             txt_size,
                                               guint            max_word_length,
                                               gboolean         enable_stemmer,
                                               gboolean         enable_unaccent,
                                               gboolean         ignore_numbers);

const gchar *  tracker_parser_next            (TrackerParser   *parser,
                                               gint            *position,
                                               gint            *byte_offset_start,
                                               gint            *byte_offset_end,
                                               gint            *word_length);

void           tracker_parser_free            (TrackerParser   *parser);

/* Collation */
gpointer tracker_collation_init (void);

void tracker_collation_shutdown (gpointer collator);

gint tracker_collation_utf8 (gpointer      collator,
                             gint          len1,
                             gconstpointer str1,
                             gint          len2,
                             gconstpointer str2);

/* Other helper methods */

gunichar2 * tracker_parser_tolower (const gunichar2 *input,
                                    gsize            len,
                                    gsize           *len_out);

gunichar2 * tracker_parser_toupper (const gunichar2 *input,
                                    gsize            len,
                                    gsize           *len_out);

gunichar2 * tracker_parser_casefold (const gunichar2 *input,
                                     gsize            len,
                                     gsize           *len_out);

gunichar2 * tracker_parser_normalize (const gunichar2 *input,
                                      GNormalizeMode   mode,
                                      gsize            len,
                                      gsize           *len_out);

gunichar2 * tracker_parser_unaccent (const gunichar2 *input,
                                     gsize            len,
                                     gsize           *len_out);

G_END_DECLS
