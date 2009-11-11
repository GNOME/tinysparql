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
 */

#include "tracker-writeback.h"

#define TRACKER_TYPE_WRITEBACK_DUMMY    (tracker_writeback_dummy_get_type ())

typedef struct TrackerWritebackDummy TrackerWritebackDummy;
typedef struct TrackerWritebackDummyClass TrackerWritebackDummyClass;

struct TrackerWritebackDummy {
        TrackerWriteback parent_instance;
};

struct TrackerWritebackDummyClass {
        TrackerWritebackClass parent_class;
};

static GType tracker_writeback_dummy_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (TrackerWritebackDummy, tracker_writeback_dummy, TRACKER_TYPE_WRITEBACK);

static void
tracker_writeback_dummy_class_init (TrackerWritebackDummyClass *klass)
{
}

static void
tracker_writeback_dummy_class_finalize (TrackerWritebackDummyClass *klass)
{
}

static void
tracker_writeback_dummy_init (TrackerWritebackDummy *dummy)
{
}

TrackerWriteback *
writeback_module_get (GTypeModule *module)
{
        tracker_writeback_dummy_register_type (module);

        return g_object_new (TRACKER_TYPE_WRITEBACK_DUMMY, NULL);
}
