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

#ifndef __TRACKER_EVOLUTION_IMAP_H__
#define __TRACKER_EVOLUTION_IMAP_H__

#include <stdlib.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <tracker-indexer/tracker-module-file.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_EVOLUTION_IMAP_FILE         (tracker_evolution_imap_file_get_type())
#define TRACKER_EVOLUTION_IMAP_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_EVOLUTION_IMAP_FILE, TrackerEvolutionImapFile))
#define TRACKER_EVOLUTION_IMAP_FILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_EVOLUTION_IMAP_FILE, TrackerEvolutionImapFileClass))
#define TRACKER_IS_EVOLUTION_IMAP_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_EVOLUTION_IMAP_FILE))
#define TRACKER_IS_EVOLUTION_IMAP_FILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_EVOLUTION_IMAP_FILE))
#define TRACKER_EVOLUTION_IMAP_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_EVOLUTION_IMAP_FILE, TrackerEvolutionImapFileClass))


typedef struct TrackerEvolutionImapFile TrackerEvolutionImapFile;
typedef struct TrackerEvolutionImapFileClass TrackerEvolutionImapFileClass;

struct TrackerEvolutionImapFile {
        TrackerModuleFile parent_instance;

        gchar *imap_dir;

        gint fd;
        FILE *summary;
        guint n_messages;
        guint cur_message;
        gchar *cur_message_uid;

        GList *mime_parts;
        GList *current_mime_part;
};

struct TrackerEvolutionImapFileClass {
        TrackerModuleFileClass parent_class;
};

GType               tracker_evolution_imap_file_get_type (void) G_GNUC_CONST;

void                tracker_evolution_imap_file_register (GTypeModule *module);

TrackerModuleFile * tracker_evolution_imap_file_new      (GFile *file);


G_END_DECLS

#endif /* __TRACKER_EVOLUTION_IMAP_H__ */
