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
#ifndef __TRACKER_PREFERENCES_UTILS_H__
#define __TRACKER_PREFERENCES_UTILS_H__

#include <glib.h>

gchar *
get_claws_command (void);

gchar *
get_thunderbird_command (void);

gchar *
get_evolution_command (void);

gchar *
get_kmail_command (void);

gboolean
evolution_available (void);

gboolean
thunderbird_available (void);

gboolean
kmail_available (void);

gboolean
convert_available (void);
#endif
