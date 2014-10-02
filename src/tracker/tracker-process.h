/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
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
#include <gio/gio.h>

#ifndef __TRACKER_PROCESS_H__
#define __TRACKER_PROCESS_H__

typedef struct {
	char *cmd;
	pid_t pid;
} TrackerProcessData;

typedef enum {
	TRACKER_PROCESS_TYPE_NONE,
	TRACKER_PROCESS_TYPE_ALL,
	TRACKER_PROCESS_TYPE_STORE,
	TRACKER_PROCESS_TYPE_MINERS
} TrackerProcessTypes;

GSList * tracker_process_get_pids        (void);
guint32  tracker_process_get_uid_for_pid (const gchar  *pid_as_string,
                                          gchar       **filename);

void     tracker_process_data_free       (TrackerProcessData *pd);

GSList * tracker_process_find_all        (void);
gint     tracker_process_stop            (TrackerProcessTypes    daemons_to_term,
                                          TrackerProcessTypes    daemons_to_kill);

#endif /* __TRACKER_PROCESS_H__ */
