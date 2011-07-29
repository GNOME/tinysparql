/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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
 *
 * Author: Carlos Garnacho  <carlos@lanedo.com>
 */

#ifndef __TRACKER_MINER_ENUMS_H__
#define __TRACKER_MINER_ENUMS_H__

G_BEGIN_DECLS

typedef enum {
	TRACKER_DIRECTORY_FLAG_NONE        = 0,
	TRACKER_DIRECTORY_FLAG_RECURSE     = 1 << 1,
	TRACKER_DIRECTORY_FLAG_CHECK_MTIME = 1 << 2,
	TRACKER_DIRECTORY_FLAG_MONITOR     = 1 << 3,
} TrackerDirectoryFlags;

typedef enum {
	TRACKER_FILTER_FILE,
	TRACKER_FILTER_DIRECTORY,
	TRACKER_FILTER_PARENT_DIRECTORY
} TrackerFilterType;

G_END_DECLS

#endif /* __TRACKER_MINER_ENUMS_H__ */
