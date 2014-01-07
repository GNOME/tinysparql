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

#ifndef __LIBTRACKER_MINER_DECORATOR_H__
#define __LIBTRACKER_MINER_DECORATOR_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include "tracker-miner-object.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_DECORATOR         (tracker_decorator_get_type())
#define TRACKER_DECORATOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DECORATOR, TrackerDecorator))
#define TRACKER_DECORATOR_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_DECORATOR, TrackerDecoratorClass))
#define TRACKER_IS_DECORATOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DECORATOR))
#define TRACKER_IS_DECORATOR_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_DECORATOR))
#define TRACKER_DECORATOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_DECORATOR, TrackerDecoratorClass))

typedef struct _TrackerDecorator TrackerDecorator;
typedef struct _TrackerDecoratorClass TrackerDecoratorClass;
typedef struct _TrackerDecoratorInfo TrackerDecoratorInfo;

struct _TrackerDecorator {
	TrackerMiner parent_instance;
	gpointer priv;
};

struct _TrackerDecoratorClass {
	TrackerMinerClass parent_class;

	void (* items_available) (TrackerDecorator *decorator);
	void (* finished)        (TrackerDecorator *decorator);
};

GType         tracker_decorator_get_type          (void) G_GNUC_CONST;

const gchar * tracker_decorator_get_data_source   (TrackerDecorator     *decorator);
const gchar** tracker_decorator_get_class_names   (TrackerDecorator     *decorator);
guint         tracker_decorator_get_n_items       (TrackerDecorator     *decorator);

void          tracker_decorator_prepend_ids       (TrackerDecorator     *decorator,
                                                   gint                 *ids,
                                                   gint                  n_ids);
void          tracker_decorator_delete_ids        (TrackerDecorator     *decorator,
                                                   gint                 *ids,
                                                   gint                  n_ids);

void          tracker_decorator_next              (TrackerDecorator     *decorator,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   func,
                                                   gpointer              user_data);

TrackerDecoratorInfo *
              tracker_decorator_next_finish       (TrackerDecorator     *decorator,
                                                   GAsyncResult         *result,
                                                   GError              **error);

const gchar * tracker_decorator_info_get_urn      (TrackerDecoratorInfo *info);
const gchar * tracker_decorator_info_get_url      (TrackerDecoratorInfo *info);
const gchar * tracker_decorator_info_get_mimetype (TrackerDecoratorInfo *info);
GTask       * tracker_decorator_info_get_task     (TrackerDecoratorInfo *info);
TrackerSparqlBuilder *
              tracker_decorator_info_get_sparql   (TrackerDecoratorInfo *info);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_DECORATOR_H__ */
