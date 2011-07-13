/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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
 */

#ifndef __TRACKER_WRITEBACK_FILE_H__
#define __TRACKER_WRITEBACK_FILE_H__

#include <gio/gio.h>

#include "tracker-writeback-module.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_WRITEBACK_FILE         (tracker_writeback_file_get_type())
#define TRACKER_WRITEBACK_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_WRITEBACK_FILE, TrackerWritebackFile))
#define TRACKER_WRITEBACK_FILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_WRITEBACK_FILE, TrackerWritebackFileClass))
#define TRACKER_IS_WRITEBACK_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_WRITEBACK_FILE))
#define TRACKER_IS_WRITEBACK_FILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_WRITEBACK_FILE))
#define TRACKER_WRITEBACK_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_WRITEBACK_FILE, TrackerWritebackFileClass))

typedef struct TrackerWritebackFile TrackerWritebackFile;
typedef struct TrackerWritebackFileClass TrackerWritebackFileClass;

struct TrackerWritebackFile {
	TrackerWriteback parent_instance;
};

struct TrackerWritebackFileClass {
	TrackerWritebackClass parent_class;

	gboolean              (* update_file_metadata) (TrackerWritebackFile    *writeback_file,
	                                                GFile                   *file,
	                                                GPtrArray               *values,
	                                                TrackerSparqlConnection *connection);
	const gchar * const * (* content_types)        (TrackerWritebackFile    *writeback_file);

};

GType tracker_writeback_file_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __TRACKER_WRITEBACK_FILE_H__ */
