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

#ifndef __TRACKER_EVOLUTION_POP_H__
#define __TRACKER_EVOLUTION_POP_H__

#include <glib.h>

#include <gmime/gmime.h>

#include <tracker-indexer/tracker-module-file.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_EVOLUTION_POP_FILE         (tracker_evolution_pop_file_get_type())
#define TRACKER_EVOLUTION_POP_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_EVOLUTION_POP_FILE, TrackerEvolutionPopFile))
#define TRACKER_EVOLUTION_POP_FILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_EVOLUTION_POP_FILE, TrackerEvolutionPopFileClass))
#define TRACKER_IS_EVOLUTION_POP_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_EVOLUTION_POP_FILE))
#define TRACKER_IS_EVOLUTION_POP_FILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_EVOLUTION_POP_FILE))
#define TRACKER_EVOLUTION_POP_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_EVOLUTION_POP_FILE, TrackerEvolutionPopFileClass))


typedef struct TrackerEvolutionPopFile TrackerEvolutionPopFile;
typedef struct TrackerEvolutionPopFileClass TrackerEvolutionPopFileClass;

struct TrackerEvolutionPopFile {
        TrackerModuleFile parent_instance;

        gchar *local_folder;

	GMimeStream *stream;
	GMimeParser *parser;
	GMimeMessage *message;

	GList *mime_parts;
	GList *current_mime_part;
};

struct TrackerEvolutionPopFileClass {
        TrackerModuleFileClass parent_class;
};

GType               tracker_evolution_pop_file_get_type (void) G_GNUC_CONST;

void                tracker_evolution_pop_file_register (GTypeModule *module);

TrackerModuleFile * tracker_evolution_pop_file_new      (GFile *file);


G_END_DECLS

#endif /* __TRACKER_EVOLUTION_POP_H__ */
