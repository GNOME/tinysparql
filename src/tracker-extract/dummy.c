/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include "tracker-main.h"

/*
 * Prototype of the parsing function. 
 */
static void extract_dummy (const gchar *filename,
			   GHashTable  *metadata);

/*
 * Link between mimetype and parsing function
 */
static TrackerExtractData data[] = {
	{ "mimetype/x-dummy", extract_dummy },
	{ NULL, NULL }
};

/*
 * Implementation of the parsing function
 */
static void
extract_function (const gchar *filename,
		  GHashTable  *metadata)
{

	/*
	 * Open the file and do whatever you need to do with it.
	 *
	 * The extracted properties must be added to the metadata
	 * hash table. 
	 */
	g_hash_table_insert (metadata,
			     g_strdup ("Dummy:DummyProp"),
			     g_strdup ("Value"));
}

/*
 * Dont touch this function! Keep it in your module with this exact name.
 * It is the "public" function used to load the module.
 */
TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
