/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
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
 */

#ifndef __TRACKER_DATA_MANAGER_H__
#define __TRACKER_DATA_MANAGER_H__

#include <glib.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-field.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-interface.h>
#include <libtracker-db/tracker-db-file-info.h>
#include <libtracker-db/tracker-db-index.h>

G_BEGIN_DECLS

void                tracker_data_manager_init              (TrackerConfig      *config,
							    TrackerLanguage    *language,
							    TrackerDBIndex     *file_index,
							    TrackerDBIndex     *email_index);
void                tracker_data_manager_shutdown          (void);
TrackerConfig *     tracker_data_manager_get_config        (void);
TrackerLanguage *   tracker_data_manager_get_language      (void);

/* Operations for TrackerDBInterface */
TrackerDBResultSet *tracker_data_manager_exec_proc         (TrackerDBInterface *iface,
							    const gchar        *procedure,
							    ...);
gboolean            tracker_data_manager_exec_no_reply     (TrackerDBInterface *iface,
							    const gchar        *query,
							    ...);
TrackerDBResultSet *tracker_data_manager_exec              (TrackerDBInterface *iface,
							    const char         *query,
							    ...);
gint                tracker_data_manager_get_db_option_int (const gchar        *option);
void                tracker_data_manager_set_db_option_int (const gchar        *option,
							    gint                value);

G_END_DECLS

#endif /* __TRACKER_DATA_MANAGER_H__ */
