/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_INDEXER_PUSH_H__
#define __TRACKER_INDEXER_PUSH_H__

#include <tracker-indexer/tracker-indexer.h>
#include <libtracker-common/tracker-config.h>

G_BEGIN_DECLS

void tracker_push_init            (TrackerConfig *config, TrackerIndexer *indexer);
void tracker_push_shutdown        (void);

G_END_DECLS

#endif /* __TRACKER_INDEXER_PUSH_H__ */
