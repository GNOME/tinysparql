/*
 * Copyright (C) 2014, Softathome <contact@softathome.com>
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
 * Author: Martyn Russell <martyn@lanedo.com>
 */

#ifndef __LIBTRACKER_MINER_FILE_DATA_PROVIDER_H__
#define __LIBTRACKER_MINER_FILE_DATA_PROVIDER_H__

#include <gio/gio.h>

#include "tracker-data-provider.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_FILE_DATA_PROVIDER         (tracker_file_data_provider_get_type ())
#define TRACKER_FILE_DATA_PROVIDER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_FILE_DATA_PROVIDER, TrackerFileDataProvider))
#define TRACKER_FILE_DATA_PROVIDER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TRACKER_TYPE_FILE_DATA_PROVIDER, TrackerFileDataProviderClass))
#define TRACKER_IS_FILE_DATA_PROVIDER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_FILE_DATA_PROVIDER))
#define TRACKER_IS_FILE_DATA_PROVIDER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_FILE_DATA_PROVIDER))
#define TRACKER_FILE_DATA_PROVIDER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_FILE_DATA_PROVIDER, TrackerFileDataProviderClass))

/**
 * TrackerFileDataProvider:
 *
 * An implementation of the #TrackerDataProvider interface.
 **/
typedef struct _TrackerFileDataProvider        TrackerFileDataProvider;
typedef struct _TrackerFileDataProviderClass   TrackerFileDataProviderClass;
typedef struct _TrackerFileDataProviderPrivate TrackerFileDataProviderPrivate;

/**
 * TrackerFileDataProviderClass:
 * @parent_class: Parent object class.
 *
 * Prototype for the class implementation.
 **/
struct _TrackerFileDataProviderClass {
	GObjectClass parent_class;
};

GType                 tracker_file_data_provider_get_type (void) G_GNUC_CONST;
TrackerDataProvider * tracker_file_data_provider_new      (void);

G_END_DECLS

#endif /* __LIBTRACKERMINER_FILE_DATA_PROVIDER_H__ */
