/*
 * Copyright (C) 2010 Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_STORE_LOCALE_CHANGE_H__
#define __TRACKER_STORE_LOCALE_CHANGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

void tracker_locale_change_initialize_subscription (void);
void tracker_locale_change_shutdown_subscription (void);

G_END_DECLS

#endif /* __TRACKER_STORE_LOCALE_CHANGE_H__ */
