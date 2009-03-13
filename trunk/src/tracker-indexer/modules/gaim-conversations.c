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

#include <tracker-indexer/tracker-module.h>
#include <time.h>

#define METADATA_CONVERSATION_USER_ACCOUNT "Conversation:UserAccount"
#define METADATA_CONVERSATION_PEER_ACCOUNT "Conversation:PeerAccount"
#define METADATA_CONVERSATION_CHAT_ROOM    "Conversation:ChatRoom"
#define METADATA_CONVERSATION_PROTOCOL     "Conversation:Protocol"
#define METADATA_CONVERSATION_DATE         "Conversation:Date"
#define METADATA_CONVERSATION_TEXT         "Conversation:Text"

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


static GType         gaim_file_get_type         (void) G_GNUC_CONST;

static gchar *       gaim_file_get_text         (TrackerModuleFile *file);
static TrackerModuleMetadata *
                     gaim_file_get_metadata     (TrackerModuleFile *file);


G_DEFINE_DYNAMIC_TYPE (GaimFile, gaim_file, TRACKER_TYPE_MODULE_FILE)

static void
gaim_file_class_init (GaimFileClass *klass)
{
        TrackerModuleFileClass *file_class = TRACKER_MODULE_FILE_CLASS (klass);

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

static gchar *
gaim_file_get_text (TrackerModuleFile *file)
{
	return tracker_module_metadata_utils_get_text (tracker_module_file_get_file (file));
}

static TrackerModuleMetadata *
gaim_file_get_metadata (TrackerModuleFile *file)
{
	TrackerModuleMetadata *metadata;
	GFile *f;
	gchar *path;
	gchar **path_decomposed;
	guint len;
	struct tm tm;

	f = tracker_module_file_get_file (file);
	path = g_file_get_path (f);

	if (!g_str_has_suffix (path, ".txt") &&
	    !g_str_has_suffix (path, ".html")) {
		/* Not a log file */
		g_free (path);
		return NULL;
	}

	path_decomposed = g_strsplit (path, G_DIR_SEPARATOR_S, -1);
	len = g_strv_length (path_decomposed);

	metadata = tracker_module_metadata_new ();

	tracker_module_metadata_add_string (metadata, METADATA_CONVERSATION_USER_ACCOUNT, path_decomposed [len - 3]);
	tracker_module_metadata_add_string (metadata, METADATA_CONVERSATION_PEER_ACCOUNT, path_decomposed [len - 2]);
	tracker_module_metadata_add_string (metadata, METADATA_CONVERSATION_PROTOCOL, path_decomposed [len - 4]);

	if (strptime (path_decomposed [len - 1], "%Y-%m-%d.%H%M%S%z", &tm) != NULL) {
		tracker_module_metadata_add_date (metadata, METADATA_CONVERSATION_DATE, mktime (&tm));
	}

	g_strfreev (path_decomposed);
	g_free (path);

	return metadata;
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
