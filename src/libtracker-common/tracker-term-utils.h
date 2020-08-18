/*
 * Copyright (C) 2020, Red Hat Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef __TRACKER_TERM_UTILS_H__
#define __TRACKER_TERM_UTILS_H__

#include <glib.h>
#include <gio/gio.h>

typedef enum {
	TRACKER_ELLIPSIZE_START,
	TRACKER_ELLIPSIZE_END,
} TrackerEllipsizeMode;

gchar * tracker_term_ellipsize (const gchar          *str,
                                gint                  max_len,
                                TrackerEllipsizeMode  mode);

void tracker_term_dimensions (guint *columns,
                              guint *lines);

gboolean tracker_term_is_tty (void);

gboolean tracker_term_pipe_to_pager (void);
gboolean tracker_term_pager_close (void);

#endif /* __TRACKER_TERM_UTILS_H__ */
