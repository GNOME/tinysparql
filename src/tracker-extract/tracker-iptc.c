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
	gboolean          multi; /* Does the field have multiple values */
	IptcPostProcessor post;
} IptcTagType;

static gchar *fix_iptc_orientation (const gchar *orientation);

static IptcTagType iptctags[] = {
        { 2, IPTC_TAG_KEYWORDS, "Image:Keywords", TRUE, NULL },
	/*	{ 2, IPTC_TAG_CONTENT_LOC_NAME, "Image:Location", NULL }, */
	{ 2, IPTC_TAG_SUBLOCATION, "Image:Location", FALSE, NULL },
        { 2, IPTC_TAG_DATE_CREATED, "Image:Date", FALSE, NULL },
        { 2, IPTC_TAG_BYLINE, "Image:Creator", FALSE, NULL },
        { 2, IPTC_TAG_CITY, "Image:City", FALSE, NULL },
        { 2, IPTC_TAG_COUNTRY_NAME, "Image:Country", FALSE, NULL },
        { 2, IPTC_TAG_CREDIT, "Image:Creator", FALSE, NULL },
        { 2, IPTC_TAG_COPYRIGHT_NOTICE, "File:Copyright", FALSE, NULL },
        { 2, IPTC_TAG_IMAGE_ORIENTATION, "Image:Orientation", FALSE, fix_iptc_orientation },
#ifdef ENABLE_DETAILED_METADATA
        { 2, IPTC_TAG_ORIGINATING_PROGRAM, "Image:Software", FALSE, NULL },
#endif /* ENABLE_DETAILED_METADATA */
	{ -1, -1, NULL, FALSE, NULL }
};

static void
metadata_append (GHashTable *metadata, gchar *key, gchar *value, gboolean append, gboolean set_additional)
{
	gchar   *new_value;
	gchar   *orig;
	gchar  **list;
	gboolean found = FALSE;
	guint    i;

	/* Iptc is always low priority, don't overwrite */
	if ( (orig = g_hash_table_lookup (metadata, key)) != NULL) {
		if (append) {
			gchar *escaped;

			escaped = tracker_escape_metadata (value);

			list = g_strsplit (orig, "|", -1);
			for (i=0; list[i]; i++) {
				/* Check 32 first characters (truncated) */
				if (strncmp (list[i], escaped, 32) == 0) {
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
		}
	} else {
		new_value = tracker_escape_metadata (value);
		g_hash_table_insert (metadata, g_strdup (key), new_value);

		/* FIXME Postprocessing is evil and should be elsewhere */
		if (set_additional && strcmp (key, "Image:Keywords") == 0) {
			g_hash_table_insert (metadata,
					     g_strdup ("Image:HasKeywords"),
					     tracker_escape_metadata ("1"));			
		}		
	}

	/* Giving certain properties HasKeywords FIXME Postprocessing is evil */
	if ((strcmp (key, "Image:Title") == 0) ||
	    (strcmp (key, "Image:Description") == 0) ) {
		g_hash_table_insert (metadata,
				     g_strdup ("Image:HasKeywords"),
				     tracker_escape_metadata ("1"));
	}
	/* Adding certain fields also to keywords */
	if ((strcmp (key, "Image:Location") == 0) ||
	    (strcmp (key, "Image:Sublocation") == 0) ||
	    (strcmp (key, "Image:Country") == 0) ||
	    (strcmp (key, "Image:City") == 0) ) {
		metadata_append (metadata, "Image:Keywords", value, TRUE, FALSE);
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

	/* FIXME According to valgrind this is leaking (together with the unref). Problem in libiptc */
	iptc = iptc_data_new ();
       	if (!iptc)
		return;
	if (iptc_data_load (iptc, buffer, len) < 0) {
		iptc_data_unref (iptc);
		return;
	}

	for (p = iptctags; p->name; ++p) {
		IptcDataSet *dataset = NULL;

		while ( (dataset = iptc_data_get_next_dataset (iptc, dataset, p->record, p->tag) ) ) {
			gchar buffer[1024];
			
			iptc_dataset_get_as_str (dataset, buffer, 1024);
			
			if (p->post) {
				metadata_append (metadata,p->name,(*p->post) (buffer), p->multi, TRUE);
			} else {
				metadata_append (metadata, p->name, buffer, p->multi, TRUE);
			}
		}
	}
	iptc_data_unref (iptc);

#endif
}
