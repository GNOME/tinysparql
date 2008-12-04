/* Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <libtracker-common/tracker-file-utils.h>
#include "tracker-module-metadata-private.h"
#include "tracker-module-file.h"

#define METADATA_FILE_PATH	     "File:Path"
#define METADATA_FILE_NAME	     "File:Name"

#define TRACKER_MODULE_FILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MODULE_FILE, TrackerModuleFilePrivate))

typedef struct TrackerModuleFilePrivate TrackerModuleFilePrivate;

struct TrackerModuleFilePrivate {
        GFile *file;
};

enum {
        PROP_0,
        PROP_FILE
};


static void   tracker_module_file_finalize     (GObject      *object);
static void   tracker_module_file_constructed  (GObject      *object);
static void   tracker_module_file_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec);
static void   tracker_module_file_get_property (GObject      *object,
                                                guint         prop_id,
                                                GValue       *value,
                                                GParamSpec   *pspec);



G_DEFINE_ABSTRACT_TYPE (TrackerModuleFile, tracker_module_file, G_TYPE_OBJECT)

static void
tracker_module_file_class_init (TrackerModuleFileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = tracker_module_file_finalize;
        object_class->constructed = tracker_module_file_constructed;
        object_class->set_property = tracker_module_file_set_property;
        object_class->get_property = tracker_module_file_get_property;

        g_object_class_install_property (object_class,
					 PROP_FILE,
					 g_param_spec_object ("file",
                                                              "File",
                                                              "File corresponding to the TrackerModuleFile",
                                                              G_TYPE_FILE,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_type_class_add_private (object_class, sizeof (TrackerModuleFilePrivate));
}

static void
tracker_module_file_init (TrackerModuleFile *file)
{
}

static void
tracker_module_file_finalize (GObject *object)
{
        TrackerModuleFilePrivate *priv;

        priv = TRACKER_MODULE_FILE_GET_PRIVATE (object);

        g_object_unref (priv->file);

        G_OBJECT_CLASS (tracker_module_file_parent_class)->finalize (object);
}

static void
tracker_module_file_constructed (GObject *object)
{
        if (TRACKER_MODULE_FILE_GET_CLASS (object)->initialize) {
                TrackerModuleFile *file;

                file = TRACKER_MODULE_FILE (object);
                TRACKER_MODULE_FILE_GET_CLASS (object)->initialize (file);
        }

        if (G_OBJECT_CLASS (tracker_module_file_parent_class)->constructed) {
                G_OBJECT_CLASS (tracker_module_file_parent_class)->constructed (object);
        }
}

static void
tracker_module_file_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        TrackerModuleFilePrivate *priv;

        priv = TRACKER_MODULE_FILE_GET_PRIVATE (object);

        switch (prop_id) {
        case PROP_FILE:
                priv->file = g_value_dup_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }

}

static void
tracker_module_file_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        TrackerModuleFilePrivate *priv;

        priv = TRACKER_MODULE_FILE_GET_PRIVATE (object);

        switch (prop_id) {
        case PROP_FILE:
                g_value_set_object (value, priv->file);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

GFile *
tracker_module_file_get_file (TrackerModuleFile *file)
{
        TrackerModuleFilePrivate *priv;

        priv = TRACKER_MODULE_FILE_GET_PRIVATE (file);

        return priv->file;
}

G_CONST_RETURN gchar *
tracker_module_file_get_service_type (TrackerModuleFile *file)
{
        if (TRACKER_MODULE_FILE_GET_CLASS (file)->get_service_type == NULL) {
                return NULL;
        }

        return TRACKER_MODULE_FILE_GET_CLASS (file)->get_service_type (file);
}

gchar *
tracker_module_file_get_uri (TrackerModuleFile *file)
{
        gchar *uri = NULL;

        if (TRACKER_MODULE_FILE_GET_CLASS (file)->get_uri) {
                uri = TRACKER_MODULE_FILE_GET_CLASS (file)->get_uri (file);
        }

        if (!uri) {
                GFile *f;

                f = tracker_module_file_get_file (file);

                /* FIXME: When we agree on storing URIs in the
                 * database, this should stop returning the path
                 */
                uri = g_file_get_path (f);
        }

        return uri;
}

gchar *
tracker_module_file_get_text (TrackerModuleFile *file)
{
        if (TRACKER_MODULE_FILE_GET_CLASS (file)->get_text == NULL) {
                return NULL;
        }

        return TRACKER_MODULE_FILE_GET_CLASS (file)->get_text (file);
}

TrackerModuleMetadata *
tracker_module_file_get_metadata (TrackerModuleFile *file)
{
        TrackerModuleMetadata *metadata = NULL;

        if (TRACKER_MODULE_FILE_GET_CLASS (file)->get_metadata != NULL) {
                metadata = TRACKER_MODULE_FILE_GET_CLASS (file)->get_metadata (file);
        }

        if (metadata &&
            !tracker_module_metadata_lookup (metadata, METADATA_FILE_PATH, NULL) &&
            !tracker_module_metadata_lookup (metadata, METADATA_FILE_NAME, NULL)) {
                gchar *uri, *dirname, *basename;

                uri = tracker_module_file_get_uri (file);
                tracker_file_get_path_and_name (uri, &dirname, &basename);

                tracker_module_metadata_add_string (metadata, METADATA_FILE_PATH, dirname);
                tracker_module_metadata_add_string (metadata, METADATA_FILE_NAME, basename);

                g_free (dirname);
                g_free (basename);
                g_free (uri);
        }

        return metadata;
}
