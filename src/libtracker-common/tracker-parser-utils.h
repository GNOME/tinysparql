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

/* ASCII undescore? */
#define IS_UNDERSCORE_UCS4(c) ((c) == 0x005F)

/* Combining diacritical mark?
 * Basic range: [0x0300,0x036F]
 * Supplement:  [0x1DC0,0x1DFF]
 * For Symbols: [0x20D0,0x20FF]
 * Half marks:  [0xFE20,0xFE2F]
 */
#define IS_CDM_UCS4(c)        (((c) >= 0x0300 && (c) <= 0x036F)  ||	\
                               ((c) >= 0x1DC0 && (c) <= 0x1DFF)  ||	\
                               ((c) >= 0x20D0 && (c) <= 0x20FF)  ||	\
                               ((c) >= 0xFE20 && (c) <= 0xFE2F))

/* Forced word breaks in Unicode parsers.
 * If any of these is found INSIDE a properly delimited Unicode word, a new word
 * break is forced and the Unicode word is split in two words.
 * Current forced wordbreaks:
 *   - 0x002E: DOT ('.')
 */
#define IS_FORCED_WORDBREAK_UCS4(c) ((c) == 0x002E)


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
