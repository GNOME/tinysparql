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

#include "tracker-extract.h"

typedef enum {
	READ_TITLE,
} tag_type;

typedef struct {
	GHashTable *metadata;
	tag_type current;
} HTMLParseInfo;

static void extract_html (const gchar *filename,
			  GHashTable  *metadata);

static TrackerExtractorData data[] = {
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
startElement (void	     *info,
	      const xmlChar  *name,
	      const xmlChar **atts)
{
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
				g_hash_table_insert (((HTMLParseInfo*) info)->metadata,
						     g_strdup ("File:License"),
						     g_strdup ((gchar*)  href));
			}
		}
	} else if (strcasecmp ((gchar*)name, "title") == 0) {
		((HTMLParseInfo*) info)->current = READ_TITLE;
	} else if (strcasecmp ((gchar*)name, "meta") == 0) {
		if (has_attribute (atts, "name", "Author")) {
			const xmlChar *author;

			author = lookup_attribute (atts, "content");

			if (author) {
				g_hash_table_insert (((HTMLParseInfo*) info)->metadata,
						     g_strdup ("Doc:Author"),
						     g_strdup ((gchar*) author));
			}
		}

		if (has_attribute (atts, "name", "DC.Description")) {
			const xmlChar *desc;

			desc = lookup_attribute (atts,"content");

			if (desc) {
				g_hash_table_insert (((HTMLParseInfo*) info)->metadata,
						     g_strdup ("Doc:Comments"),
						     g_strdup ((gchar*) desc));
			}
		}

		if (has_attribute (atts, "name", "KEYWORDS") ||
		    has_attribute (atts, "name", "keywords")) {
			const xmlChar *keywords;

			keywords = lookup_attribute (atts, "content");

			if (keywords) {
				g_hash_table_insert (((HTMLParseInfo*) info)->metadata,
						     g_strdup ("Doc:Keywords"),
						     g_strdup ((gchar*) keywords));
			}
		}
	}
}

void
characters (void	  *info,
	    const xmlChar *ch,
	    int		   len)
{
	switch (((HTMLParseInfo*) info)->current) {
	case READ_TITLE:
		g_hash_table_insert (((HTMLParseInfo*) info)->metadata,
				     g_strdup ("Doc:Title"),
				     g_strdup ((gchar*) ch));
		break;
	default:
		break;
	}

	((HTMLParseInfo*) info)->current = -1;
}

static void
extract_html (const gchar *filename,
	      GHashTable  *metadata)
{
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

	HTMLParseInfo	info = { metadata, -1 };

	htmlDocPtr doc;
	doc = htmlSAXParseFile (filename, NULL, &SAXHandlerStruct, &info);
	if (doc) {
		xmlFreeDoc (doc);
	}
}

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
