/*
 * Copyright (C) 2014 Carlos Garnacho  <carlosg@gnome.org>
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
 */

#ifndef __LIBTRACKER_MINER_DECORATOR_FS_H__
#define __LIBTRACKER_MINER_DECORATOR_FS_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include "tracker-decorator.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_DECORATOR_FS         (tracker_decorator_fs_get_type())
#define TRACKER_DECORATOR_FS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DECORATOR_FS, TrackerDecoratorFS))
#define TRACKER_DECORATOR_FS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_DECORATOR_FS, TrackerDecoratorFSClass))
#define TRACKER_IS_DECORATOR_FS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DECORATOR_FS))
#define TRACKER_IS_DECORATOR_FS_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_DECORATOR_FS))
#define TRACKER_DECORATOR_FS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_DECORATOR_FS, TrackerDecoratorFSClass))

typedef struct _TrackerDecoratorFS TrackerDecoratorFS;
typedef struct _TrackerDecoratorFSClass TrackerDecoratorFSClass;

/**
 * TrackerDecoratorFS:
 *
 * A decorator object.
 **/
struct _TrackerDecoratorFS {
	TrackerDecorator parent_instance;
	gpointer priv;
};

/**
 * TrackerDecoratorFSClass:
 * @parent_class: parent object class.
 * @padding: Reserved for future API improvements.
 *
 * A class that takes care of resources on mount points added or
 * removed, this is based on #TrackerDecoratorClass.
 **/
struct _TrackerDecoratorFSClass {
	TrackerDecoratorClass parent_class;

	/* <Private> */
	gpointer padding[10];
};

GType              tracker_decorator_fs_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __LIBTRACKER_MINER_DECORATOR_FS_H__ */
