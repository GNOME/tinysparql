/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKER_WRITEBACK_MODULE_H__
#define __TRACKER_WRITEBACK_MODULE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_WRITEBACK_MODULE    (tracker_writeback_module_get_type ())
#define TRACKER_WRITEBACK_MODULE(module) (G_TYPE_CHECK_INSTANCE_CAST ((module), TRACKER_TYPE_WRITEBACK_MODULE, TrackerWritebackModule))

typedef struct TrackerWritebackModule TrackerWritebackModule;
typedef struct TrackerWritebackModuleClass TrackerWritebackModuleClass;

struct TrackerWritebackModule {
	GTypeModule parent_instance;

	GModule *module;
	gchar *name;

        void (* initialize) (GTypeModule *module);
        void (* shutdown)   (void);
};

struct TrackerWritebackModuleClass {
	GTypeModuleClass parent_class;
};


GType                     tracker_writeback_module_get_type               (void) G_GNUC_CONST;

TrackerWritebackModule *  tracker_writeback_module_get                    (const gchar *name);
GList *                   tracker_writeback_modules_list                  (void);


G_END_DECLS

#endif /* __TRACKER_WRITEBACK_MODULE_H__ */
