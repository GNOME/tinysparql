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

#include <string.h>
#include <tracker-indexer/tracker-module.h>

#include "evolution-pop.h"
#include "evolution-imap.h"

typedef enum MailStorageType MailStorageType;

enum MailStorageType {
	MAIL_STORAGE_NONE,
	MAIL_STORAGE_LOCAL,
	MAIL_STORAGE_IMAP,
};

static gchar *local_dir = NULL;
static gchar *imap_dir = NULL;

void
indexer_module_initialize (GTypeModule *module)
{
	g_mime_init (0);

	local_dir = g_build_filename (g_get_home_dir (), ".evolution", "mail", "local", G_DIR_SEPARATOR_S, NULL);
	imap_dir = g_build_filename (g_get_home_dir (), ".evolution", "mail", "imap", G_DIR_SEPARATOR_S, NULL);

        tracker_evolution_pop_file_register (module);
        tracker_evolution_imap_file_register (module);
}

void
indexer_module_shutdown (void)
{
        g_mime_shutdown ();

	g_free (local_dir);
	g_free (imap_dir);
}

static MailStorageType
get_mail_storage_type_from_path (const gchar *path)
{
	MailStorageType type = MAIL_STORAGE_NONE;
	gchar *basenam, *dup_, *t;
	const gchar *ac_path;

	basenam = g_path_get_basename (path);
	dup_ = g_strdup (path);
	t = dup_;

	if (g_str_has_prefix (path, local_dir) &&
	    strchr (basenam, '.') == NULL) {
		type = MAIL_STORAGE_LOCAL;
		dup_ += strlen (local_dir);
		ac_path = local_dir;
	} else if (g_str_has_prefix (path, imap_dir)) {
		if (strcmp (basenam, "summary") == 0) {
			type = MAIL_STORAGE_IMAP;
			dup_ += strlen (imap_dir);
			ac_path = imap_dir;
		}
	}

	/* Exclude non wanted folders */
	if (strcasestr (path, "junk") ||
	    strcasestr (path, "spam") ||
	    strcasestr (path, "trash") ||
	    strcasestr (path, "drafts") ||
	    strcasestr (path, "sent") ||
	    strcasestr (path, "outbox")) {
		type = MAIL_STORAGE_NONE;
	}

	if (type != MAIL_STORAGE_NONE) {
		gchar *ptr = dup_;

		/* If recent Evolution detected, summary files are outdated */

		ptr++;
		ptr = g_utf8_strchr (ptr, -1, G_DIR_SEPARATOR);

		if (ptr) {
			GFile *parent_dir, 
			      *account_dir, 
			      *summary_file;

			*ptr = '\0';
			parent_dir = g_file_new_for_path (ac_path);
			account_dir = g_file_get_child (parent_dir, dup_);
			summary_file = g_file_get_child (account_dir, "folders.db");
			if (g_file_query_exists (summary_file, NULL)) {
				type = MAIL_STORAGE_NONE;
			}
			g_object_unref (parent_dir);
			g_object_unref (account_dir);
			g_object_unref (summary_file);
		}
	}

	g_free (basenam);
	g_free (t);

	return type;
}

TrackerModuleFile *
indexer_module_create_file (GFile *file)
{
	MailStorageType type;
        gchar *path;

        path = g_file_get_path (file);
        type = get_mail_storage_type_from_path (path);
        g_free (path);

        if (type == MAIL_STORAGE_LOCAL) {
                return tracker_evolution_pop_file_new (file);
        } else if (type == MAIL_STORAGE_IMAP) {
                return tracker_evolution_imap_file_new (file);
        }

        return NULL;
}
