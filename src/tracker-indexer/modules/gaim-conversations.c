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

#include "config.h"

#include <libtracker-data/tracker-data-metadata.h>

#include <tracker-indexer/tracker-module-file.h>
#include <tracker-indexer/tracker-module-iteratable.h>

#define GAIM_TYPE_FILE    (gaim_file_get_type ())
#define GAIM_FILE(module) (G_TYPE_CHECK_INSTANCE_CAST ((module), GAIM_TYPE_FILE, GaimFile))

#define MODULE_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init)		   \
	{								   \
		const GInterfaceInfo g_implement_interface_info = {	   \
			(GInterfaceInitFunc) iface_init, NULL, NULL	   \
		};							   \
									   \
		g_type_module_add_interface (type_module,		   \
					     g_define_type_id,		   \
					     TYPE_IFACE,		   \
					     &g_implement_interface_info); \
	}

typedef struct GaimFile GaimFile;
typedef struct GaimFileClass GaimFileClass;

struct GaimFile {
        TrackerModuleFile parent_instance;
};

struct GaimFileClass {
        TrackerModuleFileClass parent_class;
};


static void          gaim_file_iteratable_init  (TrackerModuleIteratableIface *iface);

static void          gaim_file_finalize         (GObject           *object);

static void          gaim_file_initialize       (TrackerModuleFile *file);
static const gchar * gaim_file_get_service_type (TrackerModuleFile *file);
static gchar *       gaim_file_get_uri          (TrackerModuleFile *file);
static gchar *       gaim_file_get_text         (TrackerModuleFile *file);
static TrackerDataMetadata *
                     gaim_file_get_metadata     (TrackerModuleFile *file);

static gboolean      gaim_file_iter_contents    (TrackerModuleIteratable *iteratable);
static guint         gaim_file_get_count        (TrackerModuleIteratable *iteratable);


G_DEFINE_DYNAMIC_TYPE_EXTENDED (GaimFile, gaim_file, TRACKER_TYPE_MODULE_FILE, 0,
                                MODULE_IMPLEMENT_INTERFACE (TRACKER_TYPE_MODULE_ITERATABLE,
                                                            gaim_file_iteratable_init))

static void
gaim_file_class_init (GaimFileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerModuleFileClass *file_class = TRACKER_MODULE_FILE_CLASS (klass);

        object_class->finalize = gaim_file_finalize;

        file_class->initialize = gaim_file_initialize;
        file_class->get_service_type = gaim_file_get_service_type;
        file_class->get_uri = gaim_file_get_uri;
        file_class->get_text = gaim_file_get_text;
        file_class->get_metadata = gaim_file_get_metadata;
}

static void
gaim_file_class_finalize (GaimFileClass *klass)
{
}

static void
gaim_file_init (GaimFile *file)
{
}

static void
gaim_file_iteratable_init (TrackerModuleIteratableIface *iface)
{
        iface->iter_contents = gaim_file_iter_contents;
        iface->get_count = gaim_file_get_count;
}

static void
gaim_file_finalize (GObject *object)
{
        /* Free here all resources allocated by the object, if any */

        /* Chain up to parent implementation */
        G_OBJECT_CLASS (gaim_file_parent_class)->finalize (object);
}

static void
gaim_file_initialize (TrackerModuleFile *file)
{
        /* Allocate here all resources for the file, if any */
}

static const gchar *
gaim_file_get_service_type (TrackerModuleFile *file)
{
        /* Implementing this function is optional.
         *
         * Return the service type for the given file.
         *
         * If this function is not implemented, the indexer will use
         * whatever service name is specified in the module configuration
         * file.
         */
        return NULL;
}

static gchar *
gaim_file_get_uri (TrackerModuleFile *file)
{
        /* Implementing this function is optional
         *
         * Return URI for the current item, with this method
         * modules can specify different URIs for different
         * elements contained in the file. See also
         * TrackerModuleIteratable.
         */
        return NULL;
}

static gchar *
gaim_file_get_text (TrackerModuleFile *file)
{
	/* Implementing this function is optional
	 *
	 * Return here full text for file, given the current state,
	 * see also TrackerModuleIteratable.
	 */
	return NULL;
}

static TrackerDataMetadata *
gaim_file_get_metadata (TrackerModuleFile *file)
{
	/* Return a TrackerDataMetadata filled with metadata for file,
         * given the current state. Also see TrackerModuleIteratable.
	 */
	return NULL;
}

static gboolean
gaim_file_iter_contents (TrackerModuleIteratable *iteratable)
{
	/* This function is meant to iterate the internal state,
	 * so it points to the next entity inside the file.
	 * In case there is such next entity, this function must
	 * return TRUE, else, returning FALSE will make the indexer
	 * think it is done with this file and move on to the next one.
	 *
	 * What an "entity" is considered is left to the module
	 * implementation.
	 */
        return FALSE;
}

static guint
gaim_file_get_count (TrackerModuleIteratable *iteratable)
{
        /* This function is meant to return the number of entities
         * contained in the file, what an "entity" is considered is
         * left to the module implementation.
         */
        return 0;
}

void
indexer_module_initialize (GTypeModule *module)
{
        gaim_file_register_type (module);
}

void
indexer_module_shutdown (void)
{
}

TrackerModuleFile *
indexer_module_create_file (GFile *file)
{
        return g_object_new (GAIM_TYPE_FILE,
                             "file", file,
                             NULL);
}
