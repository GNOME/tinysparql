/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __TRACKER_MAIN_H__
#define __TRACKER_MAIN_H__

#include "tracker-config.h"

G_BEGIN_DECLS

/* This is used to not shutdown after the default of 30 seconds if we
 * get more work to do.
 */
void              tracker_main_quit_timeout_reset (void);

/* Enables getting the config object from extractors */
TrackerConfig    *tracker_main_get_config         (void);

G_END_DECLS

#endif /* __TRACKER_MAIN_H__ */
