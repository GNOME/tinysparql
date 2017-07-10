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

/**
 * TrackerDecorator:
 *
 * Abstract miner object for passive extended metadata indexing, i.e.
 * data past the basic information such as file name, size, etc.
 **/
struct _TrackerDecorator {
	TrackerMiner parent_instance;
	gpointer priv;
};

/**
 * TrackerDecoratorClass:
 * @parent_class: parent object class.
 * @items_available: Called when there are resources to be processed.
 * @finished: Called when all resources have been processed.
 * @padding: Reserved for future API improvements.
 *
 * An implementation that takes care of extracting extra metadata
 * specific to file types by talking to tracker-extract.
 *
 * Based on #TrackerMinerClass.
 **/
struct _TrackerDecoratorClass {
	TrackerMinerClass parent_class;

	void (* items_available) (TrackerDecorator *decorator);
	void (* finished)        (TrackerDecorator *decorator);

	/* <Private> */
	gpointer padding[10];
};


/**
 * TrackerDecoratorError:
 * @TRACKER_DECORATOR_ERROR_EMPTY: There is no item to be processed
 * next. It is entirely possible to have a ::items_available signal
 * emitted and then have this error when calling
 * tracker_decorator_next_finish() because the signal may apply to a
 * class which we're not interested in. For example, a new nmo:Email
 * might have been added to Tracker, but we might only be interested
 * in nfo:Document. This case would give this error.
 * @TRACKER_DECORATOR_ERROR_PAUSED: No work was done or will be done
 * because the miner is currently paused.
 *
 * Possible errors returned when calling tracker_decorator_next_finish().
 **/
typedef enum {
	TRACKER_DECORATOR_ERROR_EMPTY,
	TRACKER_DECORATOR_ERROR_PAUSED
} TrackerDecoratorError;


GType         tracker_decorator_get_type          (void) G_GNUC_CONST;
GQuark        tracker_decorator_error_quark       (void);

const gchar * tracker_decorator_get_data_source   (TrackerDecorator     *decorator);
const gchar** tracker_decorator_get_class_names   (TrackerDecorator     *decorator);
guint         tracker_decorator_get_n_items       (TrackerDecorator     *decorator);

void          tracker_decorator_prepend_id        (TrackerDecorator     *decorator,
                                                   gint                  id,
                                                   gint                  class_name_id);
void          tracker_decorator_delete_id         (TrackerDecorator     *decorator,
                                                   gint                  id);

void          tracker_decorator_next              (TrackerDecorator     *decorator,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              user_data);

TrackerDecoratorInfo *
              tracker_decorator_next_finish       (TrackerDecorator     *decorator,
                                                   GAsyncResult         *result,
                                                   GError              **error);

void          tracker_decorator_set_priority_rdf_types (TrackerDecorator    *decorator,
                                                        const gchar * const *rdf_types);

GType         tracker_decorator_info_get_type     (void) G_GNUC_CONST;

TrackerDecoratorInfo *
              tracker_decorator_info_ref          (TrackerDecoratorInfo *info);
void          tracker_decorator_info_unref        (TrackerDecoratorInfo *info);
const gchar * tracker_decorator_info_get_urn      (TrackerDecoratorInfo *info);
const gchar * tracker_decorator_info_get_url      (TrackerDecoratorInfo *info);
const gchar * tracker_decorator_info_get_mimetype (TrackerDecoratorInfo *info);
GTask       * tracker_decorator_info_get_task     (TrackerDecoratorInfo *info);
void          tracker_decorator_info_complete     (TrackerDecoratorInfo *info,
                                                   gchar                *sparql);
void          tracker_decorator_info_complete_error (TrackerDecoratorInfo *info,
                                                     GError               *error);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_DECORATOR_H__ */
