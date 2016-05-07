/*
 * Copyright (C) 2011, ARQ Media <sam.thursfield@codethink.co.uk>
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
 * Author: Sam Thursfield <sam.thursfield@codethink.co.uk>
 */

#ifndef __TRACKER_EXTRACT_CUE_SHEET_H__
#define __TRACKER_EXTRACT_CUE_SHEET_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

G_BEGIN_DECLS

typedef struct {
	/* Values in seconds */
	gdouble start;
	gdouble duration;
	GstTagList *tag_list;
} TrackerTocEntry;

typedef struct {
	GstTagList *tag_list;
	GList *entry_list;
} TrackerToc;

void        tracker_toc_free            (TrackerToc  *toc);
TrackerToc *tracker_toc_new             (void);

void        tracker_toc_add_entry       (TrackerToc *toc,
                                         GstTagList *tags,
                                         gdouble     start,
                                         gdouble     duration);

TrackerToc *tracker_cue_sheet_parse     (const gchar *cue_sheet);
TrackerToc *tracker_cue_sheet_parse_uri (const gchar *uri);

G_END_DECLS

#endif /* __TRACKER_EXTRACT_CUE_SHEET_H__ */
