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

/**
 * SECTION:tracker-miner-enums
 * @title: Enumerations
 * @short_description: Common enumerations
 * @include: libtracker-miner/tracker-miner-enums.h
 *
 * Common enumeration types used in libtracker-miner.
 **/

/**
 * TrackerDirectoryFlags:
 * @TRACKER_DIRECTORY_FLAG_NONE: No flags.
 * @TRACKER_DIRECTORY_FLAG_RECURSE: Should recurse in the directory.
 * @TRACKER_DIRECTORY_FLAG_CHECK_MTIME: Should check mtimes of items
 * in the directory.
 * @TRACKER_DIRECTORY_FLAG_MONITOR: Should setup monitors in the items
 * found in the directory.
 * @TRACKER_DIRECTORY_FLAG_IGNORE: Should ignore the directory
 * contents.
 * @TRACKER_DIRECTORY_FLAG_PRESERVE: Should preserve items in the
 * directory even if the directory gets removed.
 * @TRACKER_DIRECTORY_FLAG_PRIORITY: Internally a priority queue is
 * used and this flag makes sure the directory is given a priority
 * over other directories queued.
 * @TRACKER_DIRECTORY_FLAG_NO_STAT: For cases where the content being
 * crawled by the enumerator is not local (e.g. it's on a
 * server somewhere), use the #TRACKER_DIRECTORY_FLAG_NO_STAT flag.
 * The default is to use stat() and assume we're mining a local or
 * mounted file system.
 * @TRACKER_DIRECTORY_FLAG_CHECK_DELETED: Forces checks on deleted
 * contents. This is most usually optimized away unless directory
 * mtime changes indicate there could be deleted content.
 *
 * Flags used when adding a new directory to be indexed in the
 * #TrackerIndexingTree and #TrackerDataProvider.
 */
typedef enum {
	TRACKER_DIRECTORY_FLAG_NONE            = 0,
	TRACKER_DIRECTORY_FLAG_RECURSE         = 1 << 1,
	TRACKER_DIRECTORY_FLAG_CHECK_MTIME     = 1 << 2,
	TRACKER_DIRECTORY_FLAG_MONITOR         = 1 << 3,
	TRACKER_DIRECTORY_FLAG_IGNORE          = 1 << 4,
	TRACKER_DIRECTORY_FLAG_PRESERVE        = 1 << 5,
	TRACKER_DIRECTORY_FLAG_PRIORITY        = 1 << 6,
	TRACKER_DIRECTORY_FLAG_NO_STAT         = 1 << 7,
	TRACKER_DIRECTORY_FLAG_CHECK_DELETED   = 1 << 8,
} TrackerDirectoryFlags;

/**
 * TrackerFilterType:
 * @TRACKER_FILTER_FILE: All files matching this filter will be filtered out.
 * @TRACKER_FILTER_DIRECTORY: All directories matching this filter will be filtered out.
 * @TRACKER_FILTER_PARENT_DIRECTORY: All files in directories matching this filter will be filtered out.
 *
 * Flags used when adding a new filter in the #TrackerIndexingTree.
 */
typedef enum {
	TRACKER_FILTER_FILE,
	TRACKER_FILTER_DIRECTORY,
	TRACKER_FILTER_PARENT_DIRECTORY
} TrackerFilterType;

/**
 * TrackerFilterPolicy:
 * @TRACKER_FILTER_POLICY_DENY: Items matching the filter will be skipped.
 * @TRACKER_FILTER_POLICY_ACCEPT: Items matching the filter will be accepted.
 *
 * Flags used when defining default filter policy in the #TrackerIndexingTree.
 */
typedef enum {
	TRACKER_FILTER_POLICY_DENY,
	TRACKER_FILTER_POLICY_ACCEPT
} TrackerFilterPolicy;

/**
 * TrackerNetworkType:
 * @TRACKER_NETWORK_TYPE_NONE: Network is disconnected
 * @TRACKER_NETWORK_TYPE_UNKNOWN: Network status is unknown
 * @TRACKER_NETWORK_TYPE_GPRS: Network is connected over a GPRS
 * connection
 * @TRACKER_NETWORK_TYPE_EDGE: Network is connected over an EDGE
 * connection
 * @TRACKER_NETWORK_TYPE_3G: Network is connected over a 3G or
 * faster (HSDPA, UMTS, ...) connection
 * @TRACKER_NETWORK_TYPE_LAN: Network is connected over a local
 * network connection. This can be ethernet, wifi, etc.
 *
 * Enumerates the different types of connections that the device might
 * use when connected to internet. Note that not all providers might
 * provide this information.
 *
 * Since: 0.18
 **/
typedef enum {
	TRACKER_NETWORK_TYPE_NONE,
	TRACKER_NETWORK_TYPE_UNKNOWN,
	TRACKER_NETWORK_TYPE_GPRS,
	TRACKER_NETWORK_TYPE_EDGE,
	TRACKER_NETWORK_TYPE_3G,
	TRACKER_NETWORK_TYPE_LAN
} TrackerNetworkType;

G_END_DECLS

#endif /* __TRACKER_MINER_ENUMS_H__ */
