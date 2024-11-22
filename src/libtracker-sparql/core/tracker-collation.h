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

#pragma once

G_BEGIN_DECLS

#include <tracker-common.h>

gint     tracker_collation_utf8_title (gpointer      collator,
                                       gint          len1,
                                       gconstpointer str1,
                                       gint          len2,
                                       gconstpointer str2);

#define TRACKER_COLLATION_LAST_CHAR ((gunichar) 0x10fffd)

G_END_DECLS
