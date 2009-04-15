/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
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

#include <string.h>
#include <glib.h>

#include <libxml/HTMLparser.h>
#include <libtracker-common/tracker-statement-list.h>

#include "tracker-main.h"

#include <libtracker-common/tracker-ontology.h>

#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

typedef enum {
	READ_TITLE,
} tag_type;

typedef struct {
	GPtrArray *metadata;
	tag_type current;
	const gchar *uri;
} HTMLParseInfo;

static void extract_html (const gchar *filename,
			  GPtrArray   *metadata);

static TrackerExtractData data[] = {
	{ "text/html",		   extract_html },
	{ "application/xhtml+xml", extract_html },
	{ NULL, NULL }
};

static gboolean
has_attribute (const xmlChar **atts,
	       const gchar    *attr,
	       const gchar    *val)
{
	gint i;

	if (!(atts && attr && val)) {
		return FALSE;
	}

	for (i = 0; atts[i] && atts[i + 1]; i += 2) {
		if (strcasecmp ((gchar*) atts[i], attr) == 0) {
			if (strcasecmp ((gchar*) atts[i + 1], val) == 0) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

static const xmlChar *
lookup_attribute (const xmlChar **atts,
		  const gchar	 *attr)
{
	gint i;

	if (!atts || !attr) {
		return NULL;
	}

	for (i = 0; atts[i] && atts[i + 1]; i += 2) {
		if (strcasecmp ((gchar*) atts[i], attr) == 0) {
			return atts[i + 1];
		}
	}

	return NULL;
}

void
startElement (void	     *info_,
	      const xmlChar  *name,
	      const xmlChar **atts)
{
	HTMLParseInfo* info = info_;

	if (!(info && name)) {
		return;
	}

	/* Look for RDFa triple describing the license */
	if (strcasecmp ((gchar*) name, "a") == 0) {
		/* This tag is a license.  Ignore, however, if it is
		 * referring to another document.
		 */
		if (has_attribute (atts, "rel", "license") &&
		    has_attribute (atts, "about", NULL) == FALSE) {
			const xmlChar *href;

			href = lookup_attribute (atts, "href");

			if (href) {
				tracker_statement_list_insert (info->metadata,
							  info->uri, NIE_PREFIX "license", 
							  (const gchar *)  href);
			}
		}
	} else if (strcasecmp ((gchar*)name, "title") == 0) {
		info->current = READ_TITLE;
	} else if (strcasecmp ((gchar*)name, "meta") == 0) {
		if (has_attribute (atts, "name", "Author")) {
			const xmlChar *author;

			author = lookup_attribute (atts, "content");

			if (author) {
				tracker_statement_list_insert (info->metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
				tracker_statement_list_insert (info->metadata, ":", NCO_PREFIX "fullname", author);
				tracker_statement_list_insert (info->metadata, info->uri, NCO_PREFIX "creator", ":");
			}
		}

		if (has_attribute (atts, "name", "DC.Description")) {
			const xmlChar *desc;

			desc = lookup_attribute (atts,"content");

			if (desc) {
				tracker_statement_list_insert (info->metadata,
							  info->uri, NIE_PREFIX "comment",
							  (const gchar *) desc);
			}
		}

		if (has_attribute (atts, "name", "KEYWORDS") ||
		    has_attribute (atts, "name", "keywords")) {
			const xmlChar* k = lookup_attribute (atts, "content");

			if (k) {
				gchar *keywords = g_strdup (k);
				char *lasts, *keyw;

				for (keyw = strtok_r (keywords, ",;", &lasts); keyw; 
				     keyw = strtok_r (NULL, ",;", &lasts)) {
					tracker_statement_list_insert (info->metadata,
							  info->uri, NIE_PREFIX "keyword",
							  (const gchar*) keyw);
				}

				g_free (keywords);
			}
		}
	}
}

void
characters (void	  *info_,
	    const xmlChar *ch,
	    int		   len)
{
	HTMLParseInfo* info = info_;

	switch (info->current) {
	case READ_TITLE:
		tracker_statement_list_insert (info->metadata,
					  info->uri, NIE_PREFIX "title",
					  (const gchar*) ch);
		break;
	default:
		break;
	}

	info->current = -1;
}

static void
extract_html (const gchar *uri,
	      GPtrArray   *metadata)
{
	gchar *filename = g_filename_from_uri (uri, NULL, NULL);
	xmlSAXHandler SAXHandlerStruct = {
			NULL, /* internalSubset */
			NULL, /* isStandalone */
			NULL, /* hasInternalSubset */
			NULL, /* hasExternalSubset */
			NULL, /* resolveEntity */
			NULL, /* getEntity */
			NULL, /* entityDecl */
			NULL, /* notationDecl */
			NULL, /* attributeDecl */
			NULL, /* elementDecl */
			NULL, /* unparsedEntityDecl */
			NULL, /* setDocumentLocator */
			NULL, /* startDocument */
			NULL, /* endDocument */
			startElement, /* startElement */
			NULL, /* endElement */
			NULL, /* reference */
			characters, /* characters */
			NULL, /* ignorableWhitespace */
			NULL, /* processingInstruction */
			NULL, /* comment */
			NULL, /* xmlParserWarning */
			NULL, /* xmlParserError */
			NULL, /* xmlParserError */
			NULL, /* getParameterEntity */
			NULL, /* cdataBlock */
			NULL, /* externalSubset */
			1,    /* initialized */
			NULL, /* private */
			NULL, /* startElementNsSAX2Func */
			NULL, /* endElementNsSAX2Func */
			NULL  /* xmlStructuredErrorFunc */
	};

	HTMLParseInfo	info = { metadata, -1, uri };

	htmlDocPtr doc;
	doc = htmlSAXParseFile (filename, NULL, &SAXHandlerStruct, &info);
	if (doc) {

		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NFO_PREFIX "Document");

		xmlFreeDoc (doc);
	}

	g_free (filename);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
