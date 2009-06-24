/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#ifndef __EVOLUTION_COMMON_H__
#define __EVOLUTION_COMMON_H__

#include <glib.h>

#include <gmime/gmime.h>

#include <libtracker-common/tracker-ontology.h>

G_BEGIN_DECLS

#define METADATA_EMAIL_RECIPIENT     TRACKER_NMO_PREFIX "recipient"
#define METADATA_EMAIL_DATE          TRACKER_NMO_PREFIX "sentDate"
#define METADATA_EMAIL_SENDER	     TRACKER_NMO_PREFIX "sender"
#define METADATA_EMAIL_SUBJECT	     TRACKER_NMO_PREFIX "messageSubject"
#define METADATA_EMAIL_SENT_TO	     TRACKER_NMO_PREFIX "to"
#define METADATA_EMAIL_CC	         TRACKER_NMO_PREFIX "cc"

enum EvolutionFlags {
	EVOLUTION_MESSAGE_ANSWERED     = 1 << 0,
	EVOLUTION_MESSAGE_DELETED      = 1 << 1,
	EVOLUTION_MESSAGE_DRAFT        = 1 << 2,
	EVOLUTION_MESSAGE_FLAGGED      = 1 << 3,
	EVOLUTION_MESSAGE_SEEN	       = 1 << 4,
	EVOLUTION_MESSAGE_ATTACHMENTS  = 1 << 5,
	EVOLUTION_MESSAGE_ANSWERED_ALL = 1 << 6,
	EVOLUTION_MESSAGE_JUNK	       = 1 << 7,
	EVOLUTION_MESSAGE_SECURE       = 1 << 8
};

GMimeStream *           evolution_common_get_stream           (const gchar      *path,
							       gint              flags,
							       off_t             start);
void                    evolution_common_get_wrapper_metadata (GMimeDataWrapper *wrapper,
							       TrackerSparqlBuilder *metadata, 
							       const gchar *subject);
gchar *                 evolution_common_get_object_encoding  (GMimeObject      *object);

G_END_DECLS

#endif /* __EVOLUTION_COMMON_H__ */
