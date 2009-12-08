/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
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
#include "tracker-msword.h"

#include <glib.h>
#include <glib/gprintf.h>

#include <wv2/wvlog.h>
#include <wv2/parser.h>
#include <wv2/handlers.h>
#include <wv2/parserfactory.h>
#include <wv2/word97_generated.h>
#include <wv2/ustring.h>


extern "C" {
#include <libtracker-common/tracker-utils.h>
}


using namespace wvWare;


class TextExtractor : public TextHandler
{
public:
	UString content;
	virtual void runOfText (const UString                &text, 
	                        SharedPtr<const Word97::CHP> chp);
}; 


void 
TextExtractor::runOfText (const  UString               &text,  
                          SharedPtr<const Word97::CHP> chp)
{
	content += text;
}


static gchar* 
ustring2utf (const UString& ustr, guint n_words) 
{
	CString cstring = ustr.cstring();
	gchar *unicode_str = g_convert (cstring.c_str (), cstring.length (), 
	                                "UTF-8", "ISO-8859-1", 
	                                NULL, NULL, NULL);

	if(unicode_str) { 
		gchar *normalized = tracker_text_normalize (unicode_str, n_words, NULL);
		g_free (unicode_str);
		return normalized;
	}
	
	return NULL;
}

gchar* 
extract_msword_content (const gchar *uri, gint max_words)
{
	gchar *filename = g_filename_from_uri (uri, NULL, NULL);
	gchar *str;
	
	if(!filename) {
		return NULL;
	}

	SharedPtr<Parser> parser (ParserFactory::createParser (filename));

	if (!parser) {
		g_free(filename);
		return NULL;
	}

	TextExtractor* extractor = new TextExtractor;
	if (!extractor) {
		g_free (filename);
		return NULL;
	}

	parser->setTextHandler (extractor);
	if (!parser->parse ()) {
		g_free (filename);
		delete extractor;
		return NULL;
	}
	
	str = ustring2utf (extractor->content, max_words);
	
	delete extractor;
	g_free (filename);
	
	return str;
}
