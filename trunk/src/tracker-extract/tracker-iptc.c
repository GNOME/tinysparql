/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Tracker Iptc - Iptc helper functions
 * Copyright (C) 2009, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include "tracker-iptc.h"
#include "tracker-main.h"

#include <glib.h>
#include <string.h>

#ifdef HAVE_LIBIPTCDATA

#include <libiptcdata/iptc-data.h>
#include <libiptcdata/iptc-dataset.h>

typedef gchar * (*IptcPostProcessor) (const gchar*);

typedef struct {
	IptcRecord        record;
	IptcTag           tag;
	gchar	         *name;
	IptcPostProcessor post;
} IptcTagType;

static gchar *fix_iptc_orientation (const gchar *orientation);

static IptcTagType iptctags[] = {
        { 2, IPTC_TAG_KEYWORDS, "Image:Keywords", NULL },
	{ 2, IPTC_TAG_CONTENT_LOC_NAME, "Image:Location", NULL },
        { 2, IPTC_TAG_DATE_CREATED, "Image:Date", NULL },
        { 2, IPTC_TAG_ORIGINATING_PROGRAM, "Image:Software", NULL },
        { 2, IPTC_TAG_BYLINE, "Image:Creator", NULL },
        { 2, IPTC_TAG_CITY, "Image:City", NULL },
        { 2, IPTC_TAG_COUNTRY_NAME, "Image:Country", NULL },
        { 2, IPTC_TAG_CREDIT, "Image:Author", NULL },
        { 2, IPTC_TAG_COPYRIGHT_NOTICE, "File:Copyright", NULL },
        { 2, IPTC_TAG_IMAGE_ORIENTATION, "Image:Orientation", fix_iptc_orientation },
	{ -1, -1, NULL, NULL }
};

static void
metadata_append (GHashTable *metadata, gchar *key, gchar *value)
{
	gchar   *new_value;
	gchar   *orig;
	gchar  **list;
	gboolean found = FALSE;
	guint    i;

	if (g_hash_table_lookup_extended (metadata, key, NULL, (gpointer) &orig)) {
		gchar *escaped;
		
		escaped = tracker_escape_metadata (value);

		list = g_strsplit (orig, "|", -1);			
		for (i=0; list[i]; i++) {
			if (strcmp (list[i], escaped) == 0) {
				found = TRUE;
				break;
			}
		}			
		g_strfreev(list);

		if (!found) {
			new_value = g_strconcat (orig, "|", escaped, NULL);
			g_hash_table_insert (metadata, g_strdup (key), new_value);
		}

		g_free (escaped);		
	} else {
		new_value = tracker_escape_metadata (value);
		g_hash_table_insert (metadata, g_strdup (key), new_value);

		/* FIXME Postprocessing is evil and should be elsewhere */
		if (strcmp (key, "Image:Keywords") == 0) {
			g_hash_table_insert (metadata,
					     g_strdup ("Image:HasKeywords"),
					     tracker_escape_metadata ("1"));
		}
	}
}

static gchar *
fix_iptc_orientation (const gchar *orientation)
{
	if (strcmp(orientation, "P")==0) {
		return "3";
	}
	
	return "1"; /* We take this as default */
}

#endif


void
tracker_read_iptc (const unsigned char *buffer,
		   size_t		len,
		   GHashTable	       *metadata)
{
#ifdef HAVE_LIBIPTCDATA
	IptcData     *iptc = NULL;
	IptcTagType  *p = NULL;

	iptc = iptc_data_new_from_data ((unsigned char *) buffer, len);

	for (p = iptctags; p->name; ++p) {
		IptcDataSet *dataset = NULL;

		while ( (dataset = iptc_data_get_next_dataset (iptc, dataset, p->record, p->tag) ) ) {
			gchar buffer[1024];
			
			iptc_dataset_get_as_str (dataset, buffer, 1024);
			
			if (p->post) {
				metadata_append (metadata,p->name,(*p->post) (buffer));
			} else {
				metadata_append (metadata, p->name, buffer);
			}			
		}
	}
	iptc_data_unref (iptc);

#endif
}
