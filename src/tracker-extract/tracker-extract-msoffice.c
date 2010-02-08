/*
 * Copyright (C) 2006, Edward Duffy (eduffy@gmail.com)
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

#include <gsf/gsf.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-utils.h>

#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"

#define NIE_PREFIX                              TRACKER_NIE_PREFIX
#define NFO_PREFIX                              TRACKER_NFO_PREFIX
#define NCO_PREFIX                              TRACKER_NCO_PREFIX

#define RDF_PREFIX                              TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX                     "type"


/*
 * Powerpoint files comprise of structures. Each structure contains a header.
 * Within that header is a record type that specifies what strcture it is. It is
 * called record type.
 *
 * Here are are some record types and description of the structure (called atom)
 * they contain.
 */

/*
 * An atom record that specifies Unicode characters with no high byte of a UTF-16
 * Unicode character. High byte is always 0.
 */
#define TEXTBYTESATOM_RECORD_TYPE               0x0FA0

/*
 * An atom record that specifies Unicode characters.
 */
#define TEXTCHARSATOM_RECORD_TYPE               0x0FA8

/*
 * A container record that specifies information about the powerpoint document.
 */
#define DOCUMENTCONTAINER_RECORD_TYPE           0x1000

/*
 * Variant type of record. Within Powerpoint text extraction we are interested
 * of SlideListWithTextContainer type that contains the textual content
 * of the slide(s).
 *
 */

#define SLIDELISTWITHTEXT_RECORD_TYPE           0x0FF0

static void extract_msoffice   (const gchar          *uri,
                                TrackerSparqlBuilder *preupdate,
                                TrackerSparqlBuilder *metadata);
static void extract_powerpoint (const gchar          *uri,
                                TrackerSparqlBuilder *preupdate,
                                TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "application/msword",            extract_msoffice },
	/* Powerpoint files */
	{ "application/vnd.ms-powerpoint", extract_powerpoint },
	{ "application/vnd.ms-*",          extract_msoffice },
	{ NULL, NULL }
};

typedef struct {
	TrackerSparqlBuilder *metadata;
	const gchar *uri;
} ForeachInfo;

static void
add_gvalue_in_metadata (TrackerSparqlBuilder *metadata,
                        const gchar          *uri,
                        const gchar          *key,
                        GValue const         *val,
                        const gchar          *type,
                        const gchar          *predicate)
{
	gchar *s;

	g_return_if_fail (metadata != NULL);
	g_return_if_fail (key != NULL);

	if (!val) {
		return;
	}

	s = g_strdup_value_contents (val);

	if (!s) {
		return;
	}

	if (!tracker_is_empty_string (s)) {
		gchar *str_val;

		/* Some fun: strings are always written "str" with double quotes
		 * around, but not numbers! */
		if (s[0] == '"') {
			size_t len;

			len = strlen (s);

			if (s[len - 1] == '"') {
				str_val = (len > 2 ? g_strndup (s + 1, len - 2) : NULL);
			} else {
				/* We have a string that begins with a double
				 * quote but which finishes by something
				 * different... We copy the string from the
				 * beginning. */
				str_val = g_strdup (s);
			}
		} else {
			/* Here, we probably have a number */
			str_val = g_strdup (s);
		}

		if (str_val) {
			if (type && predicate) {
				tracker_sparql_builder_predicate (metadata, key);

				tracker_sparql_builder_object_blank_open (metadata);
				tracker_sparql_builder_predicate (metadata, "a");
				tracker_sparql_builder_object (metadata, type);

				tracker_sparql_builder_predicate (metadata, predicate);
				tracker_sparql_builder_object_unvalidated (metadata, str_val);
				tracker_sparql_builder_object_blank_close (metadata);
			} else {
				tracker_sparql_builder_predicate (metadata, key);
				tracker_sparql_builder_object_unvalidated (metadata, str_val);
			}

			g_free (str_val);
		}
	}

	g_free (s);
}

static void
metadata_cb (gpointer key,
             gpointer value,
             gpointer user_data)
{
	ForeachInfo          *info = user_data;
	gchar                *name;
	GsfDocProp           *property;
	TrackerSparqlBuilder *metadata = info->metadata;
	GValue const         *val;
	const gchar          *uri = info->uri;

	name = key;
	property = value;
	metadata = info->metadata;
	val = gsf_doc_prop_get_val (property);

	if (g_strcmp0 (name, "dc:title") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nie:title", val, NULL, NULL);
	} else if (g_strcmp0 (name, "dc:subject") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nie:subject", val, NULL, NULL);
	} else if (g_strcmp0 (name, "dc:creator") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nco:creator", val, "nco:Contact", "nco:fullname");
	} else if (g_strcmp0 (name, "dc:keywords") == 0) {
		gchar *keywords = g_strdup_value_contents (val);
		char  *lasts, *keyw;
		size_t len;

		keyw = keywords;
		keywords = strchr (keywords, '"');
		if (keywords)
			keywords++;
		else
			keywords = keyw;

		len = strlen (keywords);
		if (keywords[len - 1] == '"')
			keywords[len - 1] = '\0';

		for (keyw = strtok_r (keywords, ",; ", &lasts); keyw;
		     keyw = strtok_r (NULL, ",; ", &lasts)) {
			tracker_sparql_builder_predicate (metadata, "nie:keyword");
			tracker_sparql_builder_object_unvalidated (metadata, keyw);
		}

		g_free (keyw);
	} else if (g_strcmp0 (name, "dc:description") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nie:comment", val, NULL, NULL);
	} else if (g_strcmp0 (name, "gsf:page-count") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nfo:pageCount", val, NULL, NULL);
	} else if (g_strcmp0 (name, "gsf:word-count") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nfo:wordCount", val, NULL, NULL);
	} else if (g_strcmp0 (name, "meta:creation-date") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nie:contentCreated", val, NULL, NULL);
	} else if (g_strcmp0 (name, "meta:generator") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nie:generator", val, NULL, NULL);
	}
}

static void
doc_metadata_cb (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
	ForeachInfo          *info = user_data;
	gchar                *name;
	GsfDocProp           *property;
	TrackerSparqlBuilder *metadata = info->metadata;
	GValue const         *val;
	const gchar          *uri = info->uri;

	name = key;
	property = value;
	metadata = user_data;
	val = gsf_doc_prop_get_val (property);

	if (g_strcmp0 (name, "CreativeCommons_LicenseURL") == 0) {
		add_gvalue_in_metadata (metadata, uri, "nie:license", val, NULL, NULL);
	}
}

/**
 * @brief Read 16 bit unsigned integer
 * @param buffer data to read integer from
 * @return 16 bit unsigned integer
 */
static gint
read_16bit (const guint8* buffer)
{
	return buffer[0] + (buffer[1] << 8);
}

/**
 * @brief Read 32 bit unsigned integer
 * @param buffer data to read integer from
 * @return 32 bit unsigned integer
 */
static gint
read_32bit (const guint8* buffer)
{
	return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}

/**
 * @brief Header for all powerpoint structures
 *
 * A structure at the beginning of each container record and each atom record in
 * the file. The values in the record header and the context of the record are
 * used to identify and interpret the record data that follows.
 */
typedef struct {
	/**
	 * @brief An unsigned integer that specifies the version of the record
	 * data that follows the record header. A value of 0xF specifies that the
	 * record is a container record.
	 */
	guint recVer;

	/**
	 * @brief An unsigned integer that specifies the record instance data.
	 * Interpretation of the value is dependent on the particular record
	 * type.
	 */
	guint recInstance;

	/**
	 * @brief A RecordType enumeration that specifies the type of the record
	 * data that follows the record header.
	 */
	gint recType;

	/**
	 * @brief An unsigned integer that specifies the length, in bytes, of the
	 * record data that follows the record header.
	 */
	guint recLen;
}RecordHeader;

/**
 * @brief Read header data from given stream
 * @param stream Stream to read header data
 * @param header Pointer to header where to store results
 */
static gboolean
read_header (GsfInput *stream, RecordHeader *header) {
	guint8 buffer[8] = {0};

	g_return_val_if_fail(stream,FALSE);
	g_return_val_if_fail(header,FALSE);
	g_return_val_if_fail(!gsf_input_eof(stream),FALSE);


	/* Header is always 8 bytes, read it */
	g_return_val_if_fail(gsf_input_read(stream,8,buffer),FALSE);

	/*
	 * Then parse individual details
	 *
	 * Record header is 8 bytes long. Data is split as follows:
	 * recVer (4 bits)
	 * recInstance (12 bits)
	 * recType (2 bytes)
	 * recLen (4 bytes)
	 *
	 * See RecordHeader for more detailed explanation of each field.
	 *
	 * Here we parse each of those fields.
	 */

	header->recType = read_16bit(&buffer[2]);
	header->recLen = read_32bit(&buffer[4]);
	header->recVer = (read_16bit(buffer) & 0xF000) >> 12;
	header->recInstance = read_16bit(buffer) & 0x0FFF;

	return TRUE;
}

/**
 * @brief Read powerpoint text from given stream.
 *
 * Powerpoint contains texts in either TextBytesAtom or TextCharsAtom. Below
 * are excerpt from [MS-PPT].pdf file describing the ppt file struture:
 *
 * TextCharsAtom contains an array of UTF-16 Unicode [RFC2781] characters that
 * specifies the characters of the corresponding text. The length, in bytes, of
 * the array is specified by rh.recLen. The array MUST NOT contain the NUL
 * character 0x0000.
 *
 * TextBytesAtom contains an array of bytes that specifies the characters of the
 * corresponding text. Each item represents the low byte of a UTF-16 Unicode
 * [RFC2781] character whose high byte is 0x00. The length, in bytes, of the
 * array is specified by rh.recLen. The array MUST NOT contain a 0x00 byte.
 *
 * @param stream Stream to read text bytes/chars atom
 * @return read text or NULL if no text was read. Has to be freed by the caller
 */
static gchar*
read_text (GsfInput *stream)
{
	gint          i = 0;
	RecordHeader  header;
	guint8       *data = NULL;
	gsize         written = 0;
	gchar        *converted = 0;

	g_return_val_if_fail (stream,NULL);

	/*
	 * First read the header that describes the structures type
	 * (TextBytesAtom or TextCharsAtom) and it's length.
	 */
	g_return_val_if_fail (read_header(stream, &header),NULL);

	/*
	 * We only want header with type either TEXTBYTESATOM_RECORD_TYPE
	 * (TextBytesAtom) or TEXTCHARSATOM_RECORD_TYPE (TextCharsAtom).
	 *
	 * We don't care about anything else
	 */
	if (header.recType != TEXTBYTESATOM_RECORD_TYPE &&
	    header.recType != TEXTCHARSATOM_RECORD_TYPE) {
		return NULL;
	}

	/* Then we'll allocate data for the actual texts */
	if (header.recType == TEXTBYTESATOM_RECORD_TYPE) {
		/*
		 * TextBytesAtom doesn't include high bytes propably in order to
		 * save space on the ppt files. We'll have to allocate double the
		 * size for it to get the high bytes
		 */
		data = g_try_new0 (guint8,header.recLen * 2);
	} else {
		data = g_try_new0 (guint8,header.recLen);
	}

	g_return_val_if_fail (data,NULL);

	/* Then read the textual data from the stream */
	if (!gsf_input_read (stream,header.recLen,data)) {
		g_free (data);
		return NULL;
	}


	/*
	 * Again if we are reading TextBytesAtom we'll need to add those utf16
	 * high bytes ourselves. They are zero as specified in [MS-PPT].pdf
	 * and this function's comments
	 */
	if (header.recType == TEXTBYTESATOM_RECORD_TYPE) {
		for(i = 0; i < header.recLen; i++) {

			/*
			 * We'll add an empty 0 byte between each byte in the
			 * array
			 */
			data[(header.recLen - i - 1) * 2] = data[header.recLen - i - 1];
			if ((header.recLen - i - 1) % 2) {
				data[header.recLen - i - 1] = 0;
			}
		}

		/*
		 * Then double the recLen now that we have the high bytes added
		 * between read bytes
		 */
		header.recLen *= 2;
	}

	/*
	 * Then we'll convert the text from UTF-16 to UTF-8 for the tracker
	 */
	converted = g_convert(data,header.recLen,
	                      "UTF-8",
	                      "UTF-16",
	                      NULL,
	                      &written,
	                      NULL);

	/*
	 * And free the data
	 */
	g_free(data);

	/* Return read text */
	return converted;
}

/**
 * @brief Find a specific header from given stream
 * @param stream Stream to parse headers from
 * @param type1 first type of header to look for
 * @param type2 convenience parameter if we are looking for either of two
 * header types
 * @param rewind if a proper header is found should this function seek
 * to the start of the header (TRUE)
 * @return TRUE if either of specified headers was found
 */
static gboolean
seek_header (GsfInput *stream,
             gint      type1,
             gint      type2,
             gboolean  rewind)
{
	RecordHeader header;

	g_return_val_if_fail(stream,FALSE);

	/*
	 * Read until we reach eof
	 */
	while(!gsf_input_eof(stream)) {

		/*
		 * Read first header
		 */
		g_return_val_if_fail(read_header(stream, &header),FALSE);

		/*
		 * Check if it's the correct type
		 */
		if (header.recType == type1 || header.recType == type2) {

			/*
			 * Sometimes it's needed to rewind to the start of the
			 * header
			 */
			if (rewind) {
				gsf_input_seek(stream,-8,G_SEEK_CUR);
			}
			return TRUE;
		}

		/*
		 * If it's not the correct type, seek to the beginning of the
		 * next header
		 */
		g_return_val_if_fail(!gsf_input_seek(stream,
		                                     header.recLen,
		                                     G_SEEK_CUR),
		                     FALSE);
	}

	return FALSE;
}

/**
 * @brief Normalize and append given text to all_texts variable
 * @param text text to append
 * @param all_texts GString to append text after normalizing it
 * @param words number of words already in all_texts
 * @param max_words maximum number of words allowed in all_texts
 * @return number of words appended to all_text
 */
static gint
append_text (gchar   *text,
             GString *all_texts,
             gint     words,
             gint     max_words)
{
	guint count = 0;
	gchar *normalized_text;

	g_return_val_if_fail(text,-1);
	g_return_val_if_fail(all_texts,-1);

	normalized_text = tracker_text_normalize(text,
	                                         max_words - words,
	                                         &count);

	if (normalized_text) {
		/*
		 * If the last added text didn't end in a space, we'll append a
		 * space between this text and previous text so the last word of
		 * previous text and first word of this text don't become one big
		 * word.
		 */
		if (all_texts->len > 0 &&
		    all_texts->str[all_texts->len-1] != ' ') {

			g_string_append_c(all_texts,' ');
		}

		g_string_append(all_texts,normalized_text);
		g_free(normalized_text);
	}

	g_free(text);
	return count;
}

static void
read_powerpoint (GsfInfile            *infile,
                 TrackerSparqlBuilder *metadata,
                 gint                  max_words)
{
	/*
	 * Try to find Powerpoint Document stream
	 */
	gsf_off_t  lastDocumentContainer = -1;
	GsfInput  *stream = gsf_infile_child_by_name(infile,
	                                             "PowerPoint Document");

	g_return_if_fail (stream);

	/*
	 * Powerpoint documents have a "editing history" stored within them.
	 * There is a structure that defines what changes were made each time
	 * but it is just easier to get the current/latest version just by
	 * finding the last occurrence of DocumentContainer structure
	 */

	lastDocumentContainer = -1;

	/*
	 * Read until we reach eof.
	 */
	while(!gsf_input_eof (stream)) {
		RecordHeader header;

		/*
		 * We only read headers of data structures
		 */
		if (!read_header (stream,&header)) {
			break;
		}

		/*
		 * And we only care about headers with type 1000,
		 * DocumentContainer
		 */

		if (header.recType == DOCUMENTCONTAINER_RECORD_TYPE) {
			lastDocumentContainer = gsf_input_tell(stream);
		}

		/*
		 * and then seek to the start of the next data structure so it is
		 * fast and we don't have to read through the whole file
		 */
		if (gsf_input_seek (stream, header.recLen, G_SEEK_CUR)) {
			break;
		}
	}

	/*
	 * If a DocumentContainer was found and we are able to seek to it.
	 *
	 * Then we'll have to find the second header with type
	 * SLIDELISTWITHTEXT_RECORD_TYPE since DocumentContainer contains
	 * MasterListWithTextContainer and SlideListWithTextContainer structures
	 * with both having the same header type. We however only want
	 * SlideListWithTextContainer which contains the textual content
	 * of the power point file.
	 */
	if (lastDocumentContainer >= 0 &&
	    !gsf_input_seek(stream,lastDocumentContainer,G_SEEK_SET) &&
	    seek_header (stream,
	                 SLIDELISTWITHTEXT_RECORD_TYPE,
	                 SLIDELISTWITHTEXT_RECORD_TYPE,
	                 FALSE) &&
	    seek_header (stream,
	                 SLIDELISTWITHTEXT_RECORD_TYPE,
	                 SLIDELISTWITHTEXT_RECORD_TYPE,
	                 FALSE)) {

		GString *all_texts = g_string_new ("");
		int word_count = 0;

		/*
		 * Read while we have either TextBytesAtom or
		 * TextCharsAtom and we have read less than max_words
		 * amount of words
		 */
		while(seek_header (stream,
		                   TEXTBYTESATOM_RECORD_TYPE,
		                   TEXTCHARSATOM_RECORD_TYPE,
		                   TRUE) &&
		      word_count < max_words) {

			gchar *text = read_text(stream);

			int count = append_text (text,
			                         all_texts,
			                         word_count,
			                         max_words);

			if (count < 0) {
				break;
			}

			word_count += count;
		}

		/*
		 * If we have any text read
		 */
		if (all_texts->len > 0) {
			/*
			 * Send it to tracker
			 */
			tracker_sparql_builder_predicate (metadata,
			                                  "nie:plainTextContent");
			tracker_sparql_builder_object_unvalidated (metadata,
			                                           all_texts->str);
		}

		g_string_free (all_texts,TRUE);

	}

	g_object_unref (stream);
}

/* This function was programmed by using ideas and algorithms from 
 * b2xtranslator project (http://b2xtranslator.sourceforge.net/) */

static gchar* 
extract_msword_content (GsfInfile *infile, 
                        gint       n_words,
                        gboolean  *is_encrypted) 
{
	GsfInput *document_stream = NULL, *table_stream = NULL;
	gint16 i = 0;
	guint8 tmp_buffer[4] = {0};
	gint fcClx, lcbClx;
	guint8 *piece_table = NULL;
	guint8 *clx = NULL;
	gint lcb_piece_table;
	gint piece_count;
	gint piece_start;
	gint piece_end;
	guint8 *piece_descriptor = NULL;
	gint piece_size;
	gint32 fc;
	guint32 is_ansi;
	guint8 *text_buffer = NULL;
	gchar *converted_text = NULL;
	GString *content = NULL;
	gchar *normalized = NULL;

	document_stream = gsf_infile_child_by_name (infile, "WordDocument");
	if (document_stream == NULL) {
		return NULL;
	}

	/* abort if FIB can't be found from beginning of WordDocument stream */
	gsf_input_seek (document_stream, 0, G_SEEK_SET);
	gsf_input_read (document_stream, 2, tmp_buffer);
	if (read_16bit (tmp_buffer) != 0xa5ec) {
		g_object_unref (document_stream);
		return NULL;
	}

	/* abort if document is encrypted */
	gsf_input_seek (document_stream, 11, G_SEEK_SET);
	gsf_input_read (document_stream, 1, tmp_buffer);
	if ((tmp_buffer[0] & 0x1) == 0x1) {
		g_object_unref (document_stream);
		*is_encrypted = TRUE;
		return NULL;
	} else
		*is_encrypted = FALSE;

	/* document can have 0Table or 1Table or both. If flag 0x0200 is 
	 * set to true in word 0x000A of the FIB then 1Table is used */

	gsf_input_seek (document_stream, 0x000A, G_SEEK_SET);
	gsf_input_read (document_stream, 2, tmp_buffer);
	i = read_16bit (tmp_buffer);

	if ((i & 0x0200) == 0x0200) {
		table_stream = gsf_infile_child_by_name (infile, "1Table");
	}
	else {
		table_stream = gsf_infile_child_by_name (infile, "0Table");
	}

	if (table_stream == NULL) {
		g_object_unref (G_OBJECT (document_stream));
		return NULL;
	}

	/* find out location and length of piece table from FIB */
	gsf_input_seek (document_stream, 418, G_SEEK_SET);
	gsf_input_read (document_stream, 4, tmp_buffer);
	fcClx = read_32bit (tmp_buffer);
	gsf_input_read (document_stream, 4, tmp_buffer);
	lcbClx = read_32bit (tmp_buffer);

	/* copy the structure holding the piece table into the clx array. */
	clx = g_malloc (lcbClx);
	gsf_input_seek (table_stream, fcClx, G_SEEK_SET);
	gsf_input_read (table_stream, lcbClx, clx);

	/* find out piece table from clx and set piece_table -pointer to it */
	i = 0;
	lcb_piece_table = 0;
	while (TRUE) {
		if (clx[i] == 2) {
			lcb_piece_table = read_32bit (clx+(i+1));
			piece_table = clx+i+5;
			piece_count = (lcb_piece_table - 4) / 12;
			break;
		}
		else if (clx[i] == 1) {
			i = i + 2 + clx[i+1];
		}
		else {
			break;
		}
	}

	/* iterate over pieces and save text to the content -variable */
	for (i = 0; i < piece_count; i++) {
		/* logical position of the text piece in the document_stream */
		piece_start = read_32bit (piece_table+(i*4));
		piece_end = read_32bit (piece_table+((i+1)*4));

		/* descriptor of single piece from piece table */
		piece_descriptor = piece_table + ((piece_count+1)*4) + (i*8);

		/* file character position */
		fc = read_32bit (piece_descriptor+2);

		/* second bit is set to 1 if text is saved in ANSI encoding */
		is_ansi = ((fc & 0x40000000) == 0x40000000);

		/* modify file character position according to text encoding */
		if (!is_ansi) {
			fc = (fc & 0xBFFFFFFF);
		}
		else {
			fc = (fc & 0xBFFFFFFF) >> 1;
		}

		/* unicode uses twice as many bytes as CP1252 */
		piece_size  = piece_end - piece_start;
		if (!is_ansi) {
			piece_size *= 2;
		}

		if (piece_size < 1) {
			continue;
		}

		/* read single text piece from document_stream */
		text_buffer = g_malloc (piece_size);
		gsf_input_seek (document_stream, fc, G_SEEK_SET);
		gsf_input_read (document_stream, piece_size, text_buffer);

		/* pieces can have different encoding */
		if(is_ansi) {
			converted_text = g_convert (text_buffer, 
			                            piece_size, 
			                            "UTF-8", 
			                            "CP1252", 
			                            NULL, 
			                            NULL, 
			                            NULL);
		}
		else {
			converted_text = g_convert (text_buffer, 
			                            piece_size, 
			                            "UTF-8", 
			                            "UTF-16", 
			                            NULL, 
			                            NULL, 
			                            NULL);
		}

		if (converted_text) {
			if (!content)
				content = g_string_new (converted_text);
			else
				g_string_append (content, converted_text);

			g_free (converted_text);
		}

		g_free (text_buffer);
	}

	g_object_unref (document_stream);
	g_object_unref (table_stream);
	g_free (clx);

	if (content) {
		normalized = tracker_text_normalize (content->str, n_words, NULL);
		g_string_free (content, TRUE);
	}

	return normalized;
}

/**
 * @brief get maximum number of words to index
 * @return maximum number of words to index
 */
static gint
max_words (void)
{
	TrackerFTSConfig *fts_config = tracker_main_get_fts_config ();
	return tracker_fts_config_get_max_words_to_index (fts_config);
}

/**
 * @brief Extract summary OLE stream from specified uri
 * @param metadata where to store summary
 * @param infile file to read summary from
 * @param uri uri of the file
 */
static void
extract_summary (TrackerSparqlBuilder *metadata,
                 GsfInfile            *infile,
                 const gchar          *uri)
{
	GsfInput *stream;
	gchar    *content;
	gboolean  is_encrypted = FALSE;

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	stream = gsf_infile_child_by_name (infile, "\05SummaryInformation");

	if (stream) {
		GsfDocMetaData *md;
		GError *err = NULL;
		ForeachInfo     info = { metadata, uri };

		md = gsf_doc_meta_data_new ();
		err = gsf_msole_metadata_read (stream, md);

		if (err) {
			g_error_free (err);
			g_object_unref (md);
			g_object_unref (stream);
			gsf_shutdown ();
			return;
		}

		gsf_doc_meta_data_foreach (md, metadata_cb, &info);

		g_object_unref (md);
		g_object_unref (stream);
	}

	stream = gsf_infile_child_by_name (infile, "\05DocumentSummaryInformation");

	if (stream) {
		GsfDocMetaData *md;
		GError *err = NULL;
		ForeachInfo     info = { metadata, uri };

		md = gsf_doc_meta_data_new ();

		err = gsf_msole_metadata_read (stream, md);
		if (err) {
			g_error_free (err);
			g_object_unref (md);
			g_object_unref (stream);
			gsf_shutdown ();
			return;
		}

		gsf_doc_meta_data_foreach (md, doc_metadata_cb, &info);

		g_object_unref (md);
		g_object_unref (stream);
	}

	content = extract_msword_content(infile, max_words (), &is_encrypted);

	if (content) {
		tracker_sparql_builder_predicate (metadata,
		                                  "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}

	if (is_encrypted) {
		tracker_sparql_builder_predicate (metadata,
		                                  "nfo:isContentEncrypted");
		tracker_sparql_builder_object_boolean (metadata, TRUE);
	}
}

/**
 * @brief Open specified uri for reading and initialize gsf
 * @param uri URI of the file to open
 * @return GsfInFile of the opened file or NULL if failed to open file
 */
static GsfInfile *
open_uri (const gchar *uri)
{
	GsfInput  *input;
	GsfInfile *infile;
	gchar     *filename;

	gsf_init ();

	filename = g_filename_from_uri (uri, NULL, NULL);

	input = gsf_input_stdio_new (filename, NULL);

	if (!input) {
		g_free (filename);
		gsf_shutdown ();
		return NULL;
	}

	infile = gsf_infile_msole_new (input, NULL);
	g_object_unref (G_OBJECT (input));

	if (!infile) {
		g_free (filename);
		gsf_shutdown ();
		return NULL;
	}

	g_free (filename);
	return infile;
}

/**
 * @brief Extract data from generic office files
 *
 * At the moment only extracts document summary from summary OLE stream.
 * @param uri URI of the file to extract data
 * @param metadata where to store extracted data to
 */
static void
extract_msoffice (const gchar          *uri,
                  TrackerSparqlBuilder *preupdate,
                  TrackerSparqlBuilder *metadata)
{
	GsfInfile *infile = open_uri(uri);
	extract_summary(metadata,infile,uri);
	g_object_unref (infile);
	gsf_shutdown ();
}

/**
 * @brief Extract data from powerpoin files
 *
 * At the moment can extract textual content and summary.
 * @param uri URI of the file to extract data
 * @param metadata where to store extracted data to
 */
static void
extract_powerpoint (const gchar          *uri,
                    TrackerSparqlBuilder *preupdate,
                    TrackerSparqlBuilder *metadata)
{
	GsfInfile *infile = open_uri(uri);
	extract_summary(metadata,infile,uri);
	read_powerpoint(infile,metadata,max_words());

	g_object_unref (infile);
	gsf_shutdown ();
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
