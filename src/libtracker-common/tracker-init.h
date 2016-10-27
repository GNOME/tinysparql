/*
 * Copyright (C) 2016 Red Hat
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

#ifndef __TRACKER_MINER_INIT_H__
#define __TRACKER_MINER_INIT_H__

#include "config.h"
#include <glib.h>

gboolean tracker_init_get_first_index_done (void);
void     tracker_init_set_first_index_done (gboolean done);

guint64  tracker_init_get_last_crawl_done  (void);
void     tracker_init_set_last_crawl_done  (gboolean done);

gboolean tracker_init_get_need_mtime_check (void);
void     tracker_init_set_need_mtime_check (gboolean needed);

#endif /* __TRACKER_MINER_INIT_H__ */
