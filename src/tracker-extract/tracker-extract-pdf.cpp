/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
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
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>

#include <glib.h>

/* Poppler includes*/
#include <GlobalParams.h>
#include <PDFDoc.h>
#include <Outline.h>
#include <ErrorCodes.h>
#include <UnicodeMap.h>
#include <PDFDocEncoding.h>
#include <TextOutputDev.h>
#include <Gfx.h>
#include <Link.h>

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"

typedef struct {
	gchar *title;
	gchar *subject;
	gchar *creation_date;
	gchar *author;
	gchar *date;
	gchar *keywords;
} PDFData;

static void extract_pdf (const gchar          *uri,
                         TrackerSparqlBuilder *preupdate,
                         TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "application/pdf", extract_pdf },
	{ NULL, NULL }
};

/**
 * Philip ported this from a poppler-glib based version to a C++ libpopler
 * version because the TextOutputDev allows us to extract text and metadata much
 * faster than the default CairoOutputDev that poppler-glib uses in case it got
 * compiled with support for Cairo. Regretfully can't this be selected at
 * runtime in the poppler-glib bindings. Apologies to the GObject/GLib fans. */

static gchar *
unicode_to_char (Unicode *unicode,
                 int      len)
{
	static UnicodeMap *uMap = NULL;
	if (uMap == NULL) {
		GooString *enc = new GooString("UTF-8");
		uMap = globalParams->getUnicodeMap(enc);
		uMap->incRefCnt ();
		delete enc;
	}

	GooString gstr;
	gchar buf[8]; /* 8 is enough for mapping an unicode char to a string */
	int i, n;

	for (i = 0; i < len; ++i) {
		n = uMap->mapUnicode(unicode[i], buf, sizeof(buf));
		gstr.append(buf, n);
	}

	return g_strdup (gstr.getCString ());
}

static void
read_toc (GooList  *items,
          GString **toc)
{
	guint length, i;

	if (!items)
		return;

	if (!*toc) {
		*toc = g_string_new ("");
	}

	length = items->getLength ();

	for (i = 0; i < length; i++) {
		OutlineItem *item;
		LinkAction *link_action;

		item = (OutlineItem *) items->get (i);

		link_action = item->getAction ();

		if (!link_action) {
			continue;
		}

		switch (link_action->getKind()) {
			case actionGoTo: {
				LinkGoTo *gto = dynamic_cast <LinkGoTo *> (link_action);

				if (gto) {
					guint title_length = item->getTitleLength ();
					GooString *named_dest = gto->getNamedDest ();

					if (title_length > 0) {
						gchar *str = unicode_to_char (item->getTitle(),
						                              title_length);
						g_string_append_printf (*toc, "%s ", str);
						g_free (str);
					}

					if (named_dest)
						g_string_append_printf (*toc, "%s ", named_dest->getCString ());
				}

				break;
			}

			case actionLaunch: {
				LinkLaunch *lan = dynamic_cast <LinkLaunch *> (link_action);

				if (lan) {
					guint title_length = item->getTitleLength ();
					GooString *filen, *param;

					filen = lan->getFileName();
					param = lan->getParams();

					if (title_length > 0) {
						gchar *str = unicode_to_char (item->getTitle(),
						                              title_length);
						g_string_append_printf (*toc, "%s ", str);
						g_free (str);
					}

					if (filen)
						g_string_append_printf (*toc, "%s ", filen->getCString ());

					if (param)
						g_string_append_printf (*toc, "%s ", param->getCString ());
				}

				break;
			}

			case actionURI: {
				LinkURI *uri = dynamic_cast <LinkURI *> (link_action);

				if (uri) {
					GooString *muri;

					muri = uri->getURI();

					if (muri)
						g_string_append_printf (*toc, "%s ", muri->getCString ());
				}

				break;
			}

			case actionNamed: {
				LinkNamed *named = dynamic_cast <LinkNamed *> (link_action);

				if (named) {
					GooString *named_dest = named->getName ();
					guint title_length = item->getTitleLength ();

					if (title_length > 0) {
						gchar *str = unicode_to_char (item->getTitle(),
						                              title_length);
						g_string_append_printf (*toc, "%s ", str);
						g_free (str);
					}

					if (named_dest)
						g_string_append_printf (*toc, "%s ", named_dest->getCString ());
				}

				break;
			}

			case actionMovie: {
				guint title_length = item->getTitleLength ();

				if (title_length > 0) {
					gchar *str = unicode_to_char (item->getTitle(),
					                              title_length);
					g_string_append_printf (*toc, "%s ", str);
					g_free (str);
				}

				break;
			}

			case actionRendition:
			case actionSound:
			case actionJavaScript:
			case actionUnknown:
			case actionGoToR:
				/* Do nothing */
				break;
		}

		if (item->hasKids ())
			read_toc (item->getKids (), toc);
	}

}

static void
read_outline (PDFDoc               *document,
              TrackerSparqlBuilder *metadata)
{
	Outline *outline;
	GString *toc = NULL;
	GooList *items;

	outline = document->getOutline();

	if (!outline) {
		return;
	}

	items = outline->getItems ();

	if (items == NULL)
		return;

	read_toc (items, &toc);

	if (toc) {
		if (toc->len > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:tableOfContents");
			tracker_sparql_builder_object_unvalidated (metadata, toc->str);
		}

		g_string_free (toc, TRUE);
	}
}


static void
page_get_size (Page    *page,
               gdouble *width,
               gdouble *height)
{
  gdouble page_width, page_height;
  gint rotate;

  rotate = page->getRotate ();

  if (rotate == 90 || rotate == 270) {
    page_height = page->getCropWidth ();
    page_width = page->getCropHeight ();
  } else {
    page_width = page->getCropWidth ();
    page_height = page->getCropHeight ();
  }

  if (width != NULL)
    *width = page_width;
  if (height != NULL)
    *height = page_height;
}

static gchar *
extract_content (PDFDoc *document,
                 gsize   n_bytes)
{
	Page *page;
	Catalog *catalog;
	GString *string;
	gint n_pages, i;
	gsize n_bytes_remaining;
	GTimer *timer;

	n_pages = document->getNumPages();
	string = g_string_new ("");
	i = 0;
	n_bytes_remaining = n_bytes;
	catalog = document->getCatalog();

	timer = g_timer_new ();

	while (i < n_pages && n_bytes_remaining > 0 && g_timer_elapsed (timer, NULL) < 5) {
		Gfx *gfx;
		GooString *sel_text;
		TextOutputDev *text_dev;
		PDFRectangle pdf_selection;
		gdouble height = 0, width = 0;
		gsize len_to_validate;

		page = catalog->getPage (i + 1);
		i++;

		text_dev = new TextOutputDev (NULL, gTrue, gFalse, gFalse);
		gfx = page->createGfx (text_dev,
		                       72.0, 72.0, 0,
		                       gFalse, /* useMediaBox */
		                       gTrue, /* Crop */
		                       -1, -1, -1, -1,
		                       gFalse, /* printing */
		                       catalog,
		                       NULL, NULL, NULL, NULL);

		page->display(gfx);
		text_dev->endPage();

		page_get_size (page, &width, &height);

		pdf_selection.x1 = 0;
		pdf_selection.y1 = 0;
		pdf_selection.x2 = width;
		pdf_selection.y2 = height;

		sel_text = text_dev->getSelectionText (&pdf_selection, selectionStyleWord);

		len_to_validate = MIN (n_bytes_remaining, strlen (sel_text->getCString ()));

		if (tracker_text_validate_utf8 (sel_text->getCString (),
		                                len_to_validate,
		                                &string,
		                                NULL)) {
			/* A whitespace is added to separate next strings appended */
			g_string_append_c (string, ' ');
		}

		/* Update accumulated UTF-8 bytes read */
		n_bytes_remaining -= len_to_validate;

		delete gfx;
		delete text_dev;
		delete sel_text;
	}

	g_timer_destroy (timer);

	return g_string_free (string, FALSE);
}

static void
write_pdf_data (PDFData               data,
                TrackerSparqlBuilder *metadata,
                GPtrArray            *keywords)
{
	if (!tracker_is_empty_string (data.title)) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, data.title);
	}

	if (!tracker_is_empty_string (data.subject)) {
		tracker_sparql_builder_predicate (metadata, "nie:subject");
		tracker_sparql_builder_object_unvalidated (metadata, data.subject);
	}

	if (!tracker_is_empty_string (data.author)) {
		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");
		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, data.author);
		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (!tracker_is_empty_string (data.date)) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, data.date);
	}

	if (!tracker_is_empty_string (data.keywords)) {
		tracker_keywords_parse (keywords, data.keywords);
	}
}


static PDFDoc*
poppler_document_new_pdf_from_file (const char  *uri,
                                    const char  *password)
{
	PDFDoc *newDoc;
	GooString *filename_g;
	GooString *password_g;
	gchar *filename;

	if (!globalParams) {
		globalParams = new GlobalParams();
	}

	filename = g_filename_from_uri (uri, NULL, NULL);
	if (!filename)
		return NULL;

	filename_g = new GooString (filename);
	g_free (filename);

	password_g = NULL;
	if (password != NULL) {
		if (g_utf8_validate (password, -1, NULL)) {
			gchar *password_latin;

			password_latin = g_convert (password, -1,
			                            "ISO-8859-1",
			                            "UTF-8",
			                            NULL, NULL, NULL);
			password_g = new GooString (password_latin);
			g_free (password_latin);
		} else {
			password_g = new GooString (password);
		}
	}

	newDoc = new PDFDoc(filename_g, password_g, password_g);
	delete password_g;

	return newDoc;
}

static gchar*
info_dict_get_string (Dict *info_dict, const gchar *key)
{
	Object obj;
	GooString *goo_value;
	gchar *result;

	if (!info_dict->lookup ((gchar *)key, &obj)->isString ()) {
		obj.free ();
		return NULL;
	}

	goo_value = obj.getString ();

	if (goo_value->hasUnicodeMarker()) {
		result = g_convert (goo_value->getCString () + 2,
		                    goo_value->getLength () - 2,
		                    "UTF-8", "UTF-16BE", NULL, NULL, NULL);
	} else {
		int len;
		gunichar *ucs4_temp;
		int i;

		len = goo_value->getLength ();
		ucs4_temp = g_new (gunichar, len + 1);
		for (i = 0; i < len; ++i) {
			ucs4_temp[i] = pdfDocEncoding[(unsigned char)goo_value->getChar(i)];
		}
		ucs4_temp[i] = 0;
		result = g_ucs4_to_utf8 (ucs4_temp, -1, NULL, NULL, NULL);
		g_free (ucs4_temp);
	}

	obj.free ();

	return result;
}

static void
extract_pdf (const gchar          *uri,
             TrackerSparqlBuilder *preupdate,
             TrackerSparqlBuilder *metadata)
{
	TrackerConfig *config;
	TrackerXmpData *xd = NULL;
	PDFData pd = { 0 }; /* actual data */
	PDFData md = { 0 }; /* for merging */
	PDFDoc *document;
	gchar *content;
	gsize n_bytes;
	Object obj;
	Catalog *catalog;
	GPtrArray *keywords;
	guint i;

	g_type_init ();

	document = poppler_document_new_pdf_from_file (uri, NULL);

	if (!document) {
		g_warning ("Could not create PopplerDocument from uri:'%s', "
		           "NULL returned without an error",
		           uri);
		return;
	}

	if (!document->isOk()) {
		int fopen_errno;
		switch (document->getErrorCode()) {
			case errEncrypted:
				tracker_sparql_builder_predicate (metadata, "a");
				tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");
				tracker_sparql_builder_predicate (metadata, "nfo:isContentEncrypted");
				tracker_sparql_builder_object_boolean (metadata, TRUE);
				break;
			case errBadCatalog:
				g_warning ("Couldn't create PopplerDocument from uri:'%s', Failed to read the document catalog", uri);
				break;
			case errDamaged:
				g_warning ("Couldn't create PopplerDocument from uri:'%s', PDF document is damaged", uri);
				break;
			case errOpenFile:
				fopen_errno = document->getFopenErrno();
				g_warning ("Couldn't create PopplerDocument from uri:'%s', %s",
				           uri, g_strerror (fopen_errno));
				break;
			default:
				g_warning ("Couldn't create PopplerDocument from uri:'%s', no error given", uri);
				break;
		}

		delete document;
		return;
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	document->getDocInfo (&obj);
	if (obj.isDict ()) {
		gchar *creation_date;
		Dict *info_dict = obj.getDict();
		pd.title = info_dict_get_string (info_dict, "Title");
		pd.author = info_dict_get_string (info_dict, "Author");
		pd.subject = info_dict_get_string (info_dict, "Subject");
		pd.keywords = info_dict_get_string (info_dict, "Keywords");
		creation_date = info_dict_get_string (info_dict, "CreationDate");
		pd.creation_date = tracker_date_guess (creation_date);
		g_free (creation_date);
	}
	obj.free ();

	keywords = g_ptr_array_new ();

	catalog = document->getCatalog ();
	if (catalog && catalog->isOk ()) {
		GooString *s = catalog->readMetadata ();
		if (s != NULL) {
			const gchar *xml;

			xml = s->getCString();
			xd = tracker_xmp_new (xml, strlen (xml), uri);

			if (!xd) {
				xd = g_new0 (TrackerXmpData, 1);
			}

			delete s;

			/* The casts here are well understood and known */
			md.title = (gchar *) tracker_coalesce_strip (4, pd.title, xd->title, xd->title2, xd->pdf_title);
			md.subject = (gchar *) tracker_coalesce_strip (2, pd.subject, xd->subject);
			md.date = (gchar *) tracker_coalesce_strip (3, pd.creation_date, xd->date, xd->time_original);
			md.author = (gchar *) tracker_coalesce_strip (2, pd.author, xd->creator);

			write_pdf_data (md, metadata, keywords);

			if (xd->keywords) {
				tracker_keywords_parse (keywords, xd->keywords);
			}

			if (xd->pdf_keywords) {
				tracker_keywords_parse (keywords, xd->pdf_keywords);
			}

			if (xd->publisher) {
				tracker_sparql_builder_predicate (metadata, "nco:publisher");
				tracker_sparql_builder_object_blank_open (metadata);
				tracker_sparql_builder_predicate (metadata, "a");
				tracker_sparql_builder_object (metadata, "nco:Contact");
				tracker_sparql_builder_predicate (metadata, "nco:fullname");
				tracker_sparql_builder_object_unvalidated (metadata, xd->publisher);
				tracker_sparql_builder_object_blank_close (metadata);
			}

			if (xd->type) {
				tracker_sparql_builder_predicate (metadata, "dc:type");
				tracker_sparql_builder_object_unvalidated (metadata, xd->type);
			}

			if (xd->format) {
				tracker_sparql_builder_predicate (metadata, "dc:format");
				tracker_sparql_builder_object_unvalidated (metadata, xd->format);
			}

			if (xd->identifier) {
				tracker_sparql_builder_predicate (metadata, "dc:identifier");
				tracker_sparql_builder_object_unvalidated (metadata, xd->identifier);
			}

			if (xd->source) {
				tracker_sparql_builder_predicate (metadata, "dc:source");
				tracker_sparql_builder_object_unvalidated (metadata, xd->source);
			}

			if (xd->language) {
				tracker_sparql_builder_predicate (metadata, "dc:language");
				tracker_sparql_builder_object_unvalidated (metadata, xd->language);
			}

			if (xd->relation) {
				tracker_sparql_builder_predicate (metadata, "dc:relation");
				tracker_sparql_builder_object_unvalidated (metadata, xd->relation);
			}

			if (xd->coverage) {
				tracker_sparql_builder_predicate (metadata, "dc:coverage");
				tracker_sparql_builder_object_unvalidated (metadata, xd->coverage);
			}

			if (xd->license) {
				tracker_sparql_builder_predicate (metadata, "nie:license");
				tracker_sparql_builder_object_unvalidated (metadata, xd->license);
			}

			if (xd->make || xd->model) {
				gchar *camera;

				if ((xd->make == NULL || xd->model == NULL) ||
					(xd->make && xd->model && strstr (xd->model, xd->make) == NULL)) {
					camera = tracker_merge_const (" ", 2, xd->make, xd->model);
				} else {
					camera = g_strdup (xd->model);
				}

				tracker_sparql_builder_predicate (metadata, "nfo:device");
				tracker_sparql_builder_object_unvalidated (metadata, camera);
				g_free (camera);
			}

			if (xd->orientation) {
				tracker_sparql_builder_predicate (metadata, "nfo:orientation");
				tracker_sparql_builder_object (metadata, xd->orientation);
			}

			if (xd->rights) {
				tracker_sparql_builder_predicate (metadata, "nie:copyright");
				tracker_sparql_builder_object_unvalidated (metadata, xd->rights);
			}

			if (xd->white_balance) {
				tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
				tracker_sparql_builder_object (metadata, xd->white_balance);
			}

			if (xd->fnumber) {
				gdouble value;

				value = g_strtod (xd->fnumber, NULL);
				tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
				tracker_sparql_builder_object_double (metadata, value);
			}

			if (xd->flash) {
				tracker_sparql_builder_predicate (metadata, "nmm:flash");
				tracker_sparql_builder_object (metadata, xd->flash);
			}

			if (xd->focal_length) {
				gdouble value;

				value = g_strtod (xd->focal_length, NULL);
				tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
				tracker_sparql_builder_object_double (metadata, value);
			}

			/* Question: Shouldn't xd->Artist be merged with md.author instead? */

			if (xd->artist || xd->contributor) {
				const gchar *artist;

				artist = tracker_coalesce_strip (2, xd->artist, xd->contributor);
				tracker_sparql_builder_predicate (metadata, "nco:contributor");
				tracker_sparql_builder_object_blank_open (metadata);
				tracker_sparql_builder_predicate (metadata, "a");
				tracker_sparql_builder_object (metadata, "nco:Contact");
				tracker_sparql_builder_predicate (metadata, "nco:fullname");
				tracker_sparql_builder_object_unvalidated (metadata, artist);
				tracker_sparql_builder_object_blank_close (metadata);
			}

			if (xd->exposure_time) {
				gdouble value;

				value = g_strtod (xd->exposure_time, NULL);
				tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
				tracker_sparql_builder_object_double (metadata, value);
			}

			if (xd->iso_speed_ratings) {
				gdouble value;

				value = g_strtod (xd->iso_speed_ratings, NULL);
				tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
				tracker_sparql_builder_object_double (metadata, value);
			}

			if (xd->description) {
				tracker_sparql_builder_predicate (metadata, "nie:description");
				tracker_sparql_builder_object_unvalidated (metadata, xd->description);
			}

			if (xd->metering_mode) {
				tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
				tracker_sparql_builder_object (metadata, xd->metering_mode);
			}

			if (xd->address || xd->country || xd->city) {
				tracker_sparql_builder_predicate (metadata, "mlo:location");

				tracker_sparql_builder_object_blank_open (metadata);
				tracker_sparql_builder_predicate (metadata, "a");
				tracker_sparql_builder_object (metadata, "mlo:GeoPoint");

				if (xd->address) {
					tracker_sparql_builder_predicate (metadata, "mlo:address");
					tracker_sparql_builder_object_unvalidated (metadata, xd->address);
				}

				if (xd->state) {
					tracker_sparql_builder_predicate (metadata, "mlo:state");
					tracker_sparql_builder_object_unvalidated (metadata, xd->state);
				}

				if (xd->city) {
					tracker_sparql_builder_predicate (metadata, "mlo:city");
					tracker_sparql_builder_object_unvalidated (metadata, xd->city);
				}

				if (xd->country) {
					tracker_sparql_builder_predicate (metadata, "mlo:country");
					tracker_sparql_builder_object_unvalidated (metadata, xd->country);
				}

				tracker_sparql_builder_object_blank_close (metadata);
			}

			/* PDF keywords aren't used ATM (why not?) */
			g_free (pd.keywords);

			g_free (pd.title);
			g_free (pd.subject);
			g_free (pd.creation_date);
			g_free (pd.author);
			g_free (pd.date);

			tracker_xmp_free (xd);
		}
	} else {
		/* So if we are here we have NO XMP data and we just
		 * write what we know from Poppler.
		 */
		write_pdf_data (pd, metadata, keywords);

		g_free (pd.keywords);
		g_free (pd.title);
		g_free (pd.subject);
		g_free (pd.creation_date);
		g_free (pd.author);
		g_free (pd.date);
	}

	for (i = 0; i < keywords->len; i++) {
		gchar *p;

		p = (gchar *) g_ptr_array_index (keywords, i);

		tracker_sparql_builder_predicate (metadata, "nao:hasTag");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nao:Tag");
		tracker_sparql_builder_predicate (metadata, "nao:prefLabel");
		tracker_sparql_builder_object_unvalidated (metadata, p);
		tracker_sparql_builder_object_blank_close (metadata);
		g_free (p);
	}
	g_ptr_array_free (keywords, TRUE);

	tracker_sparql_builder_predicate (metadata, "nfo:pageCount");
	tracker_sparql_builder_object_int64 (metadata, document->getNumPages());

	config = tracker_main_get_config ();
	n_bytes = tracker_config_get_max_bytes (config);
	content = extract_content (document, n_bytes);

	if (content) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}

	read_outline (document, metadata);

	delete document;
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
