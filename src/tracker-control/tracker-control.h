/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <glib.h>

#ifndef __TRACKER_CONTROL_H__
#define __TRACKER_CONTROL_H__

GOptionGroup *tracker_control_general_get_option_group (void);
gint          tracker_control_general_run              (void);
gboolean      tracker_control_general_options_enabled  (void);

GOptionGroup *tracker_control_status_get_option_group  (void);
gint          tracker_control_status_run               (void);
gboolean      tracker_control_status_options_enabled   (void);

GOptionGroup *tracker_control_miners_get_option_group  (void);
gint          tracker_control_miners_run               (void);
gboolean      tracker_control_miners_options_enabled   (void);


#endif /* __TRACKER_CONTROL_H__ */
