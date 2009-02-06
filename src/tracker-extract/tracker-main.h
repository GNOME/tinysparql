/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <glib.h>

#include "tracker-escape.h"

G_BEGIN_DECLS

/* FIXME: We use this to say that we don't actually have any metadata
 * for this keyword. This is bad because it means we have to string
 * check every value returned in clients that use the Tracker API.
 * This will be fixed in due course after January some time, -mr.
 */
#define METADATA_UNKNOWN ""

typedef struct TrackerExtractData TrackerExtractData;

typedef TrackerExtractData * (*TrackerExtractDataFunc)(void);

struct TrackerExtractData {
	const gchar *mime;

	void (* extract) (const gchar *path,
			  GHashTable  *metadata);
};

/* This is defined in each extract */
TrackerExtractData *tracker_get_extract_data            (void);

/* This is used to not shutdown after the default of 30 seconds if we
 * get more work to do.
 */
void                tracker_main_shutdown_timeout_reset (void);

G_END_DECLS

#endif /* __TRACKER_MAIN_H__ */
