/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_PARSER_UTILS_H__
#define __TRACKER_PARSER_UTILS_H__

#include "config.h"

#include <glib.h>

#ifdef HAVE_LIBICU
#include <unicode/utypes.h>
#endif

G_BEGIN_DECLS

/* ASCII-7 is in range [0x00,0x7F] */
#define IS_ASCII_UCS4(c)      ((c) <= 0x7F)

/* CJK ranges are : [0x3400,0x4DB5], [0x4E00,0x9FA5], [0x20000,0x2A6D6]  */
#define IS_CJK_UCS4(c)        (((c) >= 0x3400 && (c) <= 0x4DB5)  ||	\
                               ((c) >= 0x4E00 && (c) <= 0x9FA5)  ||	\
                               ((c) >= 0x20000 && (c) <= 0x2A6D6))

#define IS_UNDERSCORE_UCS4(c) ((c) == 0x005F)


gchar *tracker_parser_unaccent_utf16be_word (const gchar *string,
                                             gsize        ilength,
                                             gsize        *p_olength);

gchar *tracker_parser_unaccent_utf8_word (const gchar *string,
                                          gsize        ilength,
                                          gsize        *p_olength);

#ifdef HAVE_LIBICU
gchar *tracker_parser_unaccent_UChar_word (const UChar *string,
                                           gsize        ilength,
                                           gsize        *p_olength);
#endif


gboolean tracker_parser_is_reserved_word_utf8 (const gchar *word,
                                               gsize word_length);


/* Define to 1 if you want to enable debugging logs showing HEX contents
 * of the words being parsed */
#define TRACKER_PARSER_DEBUG_HEX 0

#if TRACKER_PARSER_DEBUG_HEX
void    tracker_parser_message_hex (const gchar  *message,
                                    const gchar  *str,
                                    gsize         str_length);
#else
#define tracker_parser_message_hex(a,b,c)
#endif

G_END_DECLS

#endif /* __TRACKER_PARSER_UTILS_H__ */
