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

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-statement-list.h>

#include "tracker-iptc.h"
#include "tracker-main.h"

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX

#include <glib.h>
#include <string.h>

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-file-utils.h>

#ifdef HAVE_LIBIPTCDATA

#include <libiptcdata/iptc-data.h>
#include <libiptcdata/iptc-dataset.h>

#define IPTC_DATE_FORMAT "%Y %m %d"

typedef const gchar * (*IptcPostProcessor) (const gchar*);

typedef struct {
	IptcRecord        record;
	IptcTag           tag;
	const gchar      *name;
	IptcPostProcessor post;
	const gchar      *urn;
	const gchar      *rdf_type;
	const gchar      *predicate;
} IptcTagType;

static const gchar *fix_iptc_orientation (const gchar *orientation);

static gchar *
date_to_iso8601 (const gchar *date)
{
	/* From: ex; date "2007:04:15 15:35:58"
	* To : ex. "2007-04-15T17:35:58+0200 where +0200 is localtime
	*/
	return tracker_date_format_to_iso8601 (date, IPTC_DATE_FORMAT);
}

static IptcTagType iptctags[] = {
        { 2, IPTC_TAG_KEYWORDS, NIE_PREFIX "keyword", NULL, NULL, NULL, NULL }, /* We might have to strtok_r this one? */
	/*	{ 2, IPTC_TAG_CONTENT_LOC_NAME, "Image:Location", NULL, NULL, NULL, NULL }, */
	/*{ 2, IPTC_TAG_SUBLOCATION, "Image:Location", NULL, NULL, NULL, NULL },*/
        { 2, IPTC_TAG_DATE_CREATED, NIE_PREFIX "contentCreated", date_to_iso8601, NULL, NULL, NULL },
        /*{ 2, IPTC_TAG_ORIGINATING_PROGRAM, "Image:Software", NULL, NULL, NULL, NULL },*/
        { 2, IPTC_TAG_BYLINE, NCO_PREFIX "creator", NULL, ":", NCO_PREFIX "Contact", NCO_PREFIX "fullname" },
        /*{ 2, IPTC_TAG_CITY, "Image:City", NULL, NULL, NULL, NULL },*/
        /*{ 2, IPTC_TAG_COUNTRY_NAME, "Image:Country", NULL, NULL, NULL, NULL },*/
	{ 2, IPTC_TAG_CREDIT, NCO_PREFIX "creator", NULL, ":", NCO_PREFIX "Contact", NCO_PREFIX "fullname" },
        { 2, IPTC_TAG_COPYRIGHT_NOTICE, NIE_PREFIX "copyright", NULL, NULL, NULL, NULL },
        { 2, IPTC_TAG_IMAGE_ORIENTATION, NFO_PREFIX "orientation", fix_iptc_orientation, NULL, NULL, NULL },
	{ -1, -1, NULL, NULL, NULL, NULL, NULL }
};

static const gchar *
fix_iptc_orientation (const gchar *orientation)
{
	if (strcmp(orientation, "P")==0) {
		return "nfo:orientation-left";
	}
	
	return "nfo:orientation-top"; /* We take this as default */
}

#endif


void
tracker_read_iptc (const unsigned char *buffer,
		   size_t		len,
		   const gchar         *uri,
		   GPtrArray	       *metadata)
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
			const gchar *what_i_need;

			iptc_dataset_get_as_str (dataset, buffer, 1024);
			
			if (p->post) {
				what_i_need = (*p->post) (buffer);
			} else {
				what_i_need = buffer;
			}

			if (p->urn) {
				tracker_statement_list_insert (metadata, p->urn, RDF_TYPE, p->rdf_type);
				tracker_statement_list_insert (metadata, p->urn, p->predicate, what_i_need);
				tracker_statement_list_insert (metadata, uri, p->name, p->urn);
			} else {
				tracker_statement_list_insert (metadata, uri, p->name, what_i_need);
			}
		}
	}
	iptc_data_unref (iptc);

#endif
}
