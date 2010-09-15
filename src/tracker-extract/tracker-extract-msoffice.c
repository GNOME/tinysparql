/*
 * Copyright (C) 2006, Edward Duffy <eduffy@gmail.com>
 * Copyright (C) 2006, Laurent Aguerreche <laurent.aguerreche@free.fr>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
#include <gsf/gsf-infile-zip.h>

#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-os-dependant.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"
#include "tracker-gsf.h"

/* Powerpoint files comprise of structures. Each structure contains a
 * header. Within that header is a record type that specifies what
 * strcture it is. It is called record type.
 *
 * Here are are some record types and description of the structure
 * (called atom) they contain.
 */

/* An atom record that specifies Unicode characters with no high byte
 * of a UTF-16 Unicode character. High byte is always 0.
 * http://msdn.microsoft.com/en-us/library/dd947905%28v=office.12%29.aspx
 */
#define TEXTBYTESATOM_RECORD_TYPE      0x0FA8

/* An atom record that specifies Unicode characters.
 * http://msdn.microsoft.com/en-us/library/dd772921%28v=office.12%29.aspx
 */
#define TEXTCHARSATOM_RECORD_TYPE      0x0FA0

/* A container record that specifies information about the powerpoint
 * document.
 */
#define DOCUMENTCONTAINER_RECORD_TYPE  0x03E8

/* Variant type of record. Within Powerpoint text extraction we are
 * interested of SlideListWithTextContainer type that contains the
 * textual content of the slide(s).
 */
#define SLIDELISTWITHTEXT_RECORD_TYPE  0x0FF0

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
} PowerPointRecordHeader;

/* Excel spec record type to read shared string */
typedef enum {
	RECORD_TYPE_SST      = 252,
	RECORD_TYPE_CONTINUE = 60,
	RECORD_TYPE_EOF      = 10
} ExcelRecordType;

/* ExcelBiffHeader to read excel spec header */
typedef struct {
	ExcelRecordType id;
	guint length;
} ExcelBiffHeader;

/* ExtendendString Record offset in stream and length */
typedef struct {
	gsf_off_t offset; /* 64 bits!! */
	gsize     length;
} ExcelExtendedStringRecord;

typedef enum {
	MS_OFFICE_XML_TAG_INVALID,
	MS_OFFICE_XML_TAG_TITLE,
	MS_OFFICE_XML_TAG_SUBJECT,
	MS_OFFICE_XML_TAG_AUTHOR,
	MS_OFFICE_XML_TAG_MODIFIED,
	MS_OFFICE_XML_TAG_COMMENTS,
	MS_OFFICE_XML_TAG_CREATED,
	MS_OFFICE_XML_TAG_GENERATOR,
	MS_OFFICE_XML_TAG_NUM_OF_PAGES,
	MS_OFFICE_XML_TAG_NUM_OF_CHARACTERS,
	MS_OFFICE_XML_TAG_NUM_OF_WORDS,
	MS_OFFICE_XML_TAG_NUM_OF_LINES,
	MS_OFFICE_XML_TAG_APPLICATION,
	MS_OFFICE_XML_TAG_NUM_OF_PARAGRAPHS,
	MS_OFFICE_XML_TAG_SLIDE_TEXT,
	MS_OFFICE_XML_TAG_WORD_TEXT,
	MS_OFFICE_XML_TAG_XLS_SHARED_TEXT,
	MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA,
	MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA
} MsOfficeXMLTagType;

typedef enum {
	FILE_TYPE_INVALID,
	FILE_TYPE_PPTX,
	FILE_TYPE_PPSX,
	FILE_TYPE_DOCX,
	FILE_TYPE_XLSX
} MsOfficeXMLFileType;

typedef struct {
	TrackerSparqlBuilder *metadata;
	MsOfficeXMLFileType file_type;
	MsOfficeXMLTagType tag_type;
	gboolean style_element_present;
	gboolean preserve_attribute_present;
	const gchar *uri;
	GString *content;
	gboolean title_already_set;
} MsOfficeXMLParserInfo;

typedef struct {
	TrackerSparqlBuilder *metadata;
	const gchar *uri;
} MetadataInfo;

static void extract_msoffice     (const gchar          *uri,
                                  TrackerSparqlBuilder *preupdate,
                                  TrackerSparqlBuilder *metadata);
static void extract_msoffice_xml (const gchar          *uri,
                                  TrackerSparqlBuilder *preupdate,
                                  TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "application/msword",            extract_msoffice },
	/* Powerpoint files */
	{ "application/vnd.ms-powerpoint", extract_msoffice },
	{ "application/vnd.ms-excel",	   extract_msoffice },
	{ "application/vnd.ms-*",          extract_msoffice },
	/* MSoffice2007*/
	{ "application/vnd.openxmlformats-officedocument.presentationml.presentation", extract_msoffice_xml },
	{ "application/vnd.openxmlformats-officedocument.presentationml.slideshow",    extract_msoffice_xml },
	{ "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",         extract_msoffice_xml },
	{ "application/vnd.openxmlformats-officedocument.wordprocessingml.document",   extract_msoffice_xml },
	{ NULL, NULL }
};

static void
metadata_add_gvalue (TrackerSparqlBuilder *metadata,
                     const gchar          *uri,
                     const gchar          *key,
                     GValue const         *val,
                     const gchar          *type,
                     const gchar          *predicate,
                     gboolean              is_date)
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
		 * around, but not numbers!
		 */
		if (s[0] == '"') {
			size_t len;

			len = strlen (s);

			if (s[len - 1] == '"') {
				if (is_date) {
					if (len > 2) {
						gchar *str = g_strndup (s + 1, len - 2);
						str_val = tracker_date_guess (str);
						g_free (str);
					} else {
						str_val = NULL;
					}
				} else {
					str_val = len > 2 ? g_strndup (s + 1, len - 2) : NULL;
				}
			} else {
				/* We have a string that begins with a double
				 * quote but which finishes by something
				 * different... We copy the string from the
				 * beginning.
				 */
				if (is_date) {
					str_val = tracker_date_guess (s);
				} else {
					str_val = g_strdup (s);
				}
			}
		} else {
			/* Here, we probably have a number */
			if (is_date) {
				str_val = tracker_date_guess (s);
			} else {
				str_val = g_strdup (s);
			}
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
summary_metadata_cb (gpointer key,
                     gpointer value,
                     gpointer user_data)
{
	MetadataInfo *info = user_data;
	GValue const *val;

	val = gsf_doc_prop_get_val (value);

	if (g_strcmp0 (key, "dc:title") == 0) {
		metadata_add_gvalue (info->metadata, info->uri, "nie:title", val, NULL, NULL, FALSE);
	} else if (g_strcmp0 (key, "dc:subject") == 0) {
		metadata_add_gvalue (info->metadata, info->uri, "nie:subject", val, NULL, NULL, FALSE);
	} else if (g_strcmp0 (key, "dc:creator") == 0) {
		metadata_add_gvalue (info->metadata, info->uri, "nco:creator", val, "nco:Contact", "nco:fullname", FALSE);
	} else if (g_strcmp0 (key, "dc:keywords") == 0) {
		gchar *keywords = g_strdup_value_contents (val);
		gchar *lasts, *keyw;
		size_t len;

		keyw = keywords;
		keywords = strchr (keywords, '"');

		if (keywords) {
			keywords++;
		} else {
			keywords = keyw;
		}

		len = strlen (keywords);
		if (keywords[len - 1] == '"') {
			keywords[len - 1] = '\0';
		}

		for (keyw = strtok_r (keywords, ",; ", &lasts); keyw;
		     keyw = strtok_r (NULL, ",; ", &lasts)) {
			tracker_sparql_builder_predicate (info->metadata, "nie:keyword");
			tracker_sparql_builder_object_unvalidated (info->metadata, keyw);
		}

		g_free (keyw);
	} else if (g_strcmp0 (key, "dc:description") == 0) {
		metadata_add_gvalue (info->metadata, info->uri, "nie:comment", val, NULL, NULL, FALSE);
	} else if (g_strcmp0 (key, "gsf:page-count") == 0) {
		metadata_add_gvalue (info->metadata, info->uri, "nfo:pageCount", val, NULL, NULL, FALSE);
	} else if (g_strcmp0 (key, "gsf:word-count") == 0) {
		metadata_add_gvalue (info->metadata, info->uri, "nfo:wordCount", val, NULL, NULL, FALSE);
	} else if (g_strcmp0 (key, "meta:creation-date") == 0) {
		metadata_add_gvalue (info->metadata, info->uri, "nie:contentCreated", val, NULL, NULL, TRUE);
	} else if (g_strcmp0 (key, "meta:generator") == 0) {
		metadata_add_gvalue (info->metadata, info->uri, "nie:generator", val, NULL, NULL, FALSE);
	}
}

static void
document_metadata_cb (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
	if (g_strcmp0 (key, "CreativeCommons_LicenseURL") == 0) {
		MetadataInfo *info = user_data;

		metadata_add_gvalue (info->metadata,
		                     info->uri,
		                     "nie:license",
		                     gsf_doc_prop_get_val (value),
		                     NULL,
		                     NULL,
		                     FALSE);
	}
}

/**
 * @brief Read 8 bit unsigned integer
 * @param buffer data to read integer from
 * @return 16 bit unsigned integer
 */
static guint
read_8bit (const guint8 *buffer)
{
	return buffer[0];
}

/**
 * @brief Read 16 bit unsigned integer
 * @param buffer data to read integer from
 * @return 16 bit unsigned integer
 */
static guint16
read_16bit (const guint8 *buffer)
{
	return buffer[0] + (buffer[1] << 8);
}

/**
 * @brief Read 32 bit unsigned integer
 * @param buffer data to read integer from
 * @return 32 bit unsigned integer
 */
static guint32
read_32bit (const guint8 *buffer)
{
	return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}

/**
 * @brief Common conversion and normalization method for all msoffice type
 *  documents.
 * @param buffer Input buffer with the string contents
 * @param chunk_size Number of valid bytes in the input buffer
 * @param is_ansi If %TRUE, input text should be encoded in CP1252, and
 *  in UTF-16 otherwise.
 * @param p_bytes_remaining Pointer to #gsize specifying how many bytes
 *  should still be considered.
 * @param p_content Pointer to a #GString where the output normalized words
 *  will be appended.
 */
static void
msoffice_convert_and_normalize_chunk (guint8    *buffer,
                                      gsize      chunk_size,
                                      gboolean   is_ansi,
                                      gsize     *bytes_remaining,
                                      GString  **content)
{
	gsize n_bytes_utf8;
	gchar *converted_text;
	GError *error = NULL;

	g_return_if_fail (buffer != NULL);
	g_return_if_fail (chunk_size > 0);
	g_return_if_fail (bytes_remaining != NULL);
	g_return_if_fail (content != NULL);

	/* chunks can have different encoding
	 *
	 * TODO: Using g_iconv, this extra heap allocation could be
	 * avoided, re-using over and over again the same output buffer
	 * for the UTF-8 encoded string
	 */
	converted_text = g_convert (buffer,
	                            chunk_size,
	                            "UTF-8",
	                            is_ansi ? "CP1252" : "UTF-16",
	                            NULL,
	                            &n_bytes_utf8,
	                            &error);

	if (converted_text) {
		gsize len_to_validate;

		len_to_validate = MIN (*bytes_remaining, n_bytes_utf8);

		if (tracker_text_validate_utf8 (converted_text,
		                                len_to_validate,
		                                content,
		                                NULL)) {
			/* A whitespace is added to separate next strings appended */
			g_string_append_c (*content, ' ');
		}

		/* Update accumulated UTF-8 bytes read */
		*bytes_remaining -= len_to_validate;
		g_free (converted_text);
	} else {
		g_warning ("Couldn't convert %" G_GSIZE_FORMAT " bytes from %s to UTF-8: %s",
		           chunk_size,
		           is_ansi ? "CP1252" : "UTF-16",
		           error ? error->message : "no error given");
	}

	/* Note that error may be set even if some converted text is
	 * available, due to G_CONVERT_ERROR_ILLEGAL_SEQUENCE for example */
	g_clear_error (&error);
}

/**
 * @brief Read header data from given stream
 * @param stream Stream to read header data
 * @param header Pointer to header where to store results
 */
static gboolean
ppt_read_header (GsfInput               *stream,
                 PowerPointRecordHeader *header)
{
	guint8 buffer[8] = {0};

	g_return_val_if_fail (stream, FALSE);
	g_return_val_if_fail (header, FALSE);
	g_return_val_if_fail (!gsf_input_eof (stream), FALSE);


	/* Header is always 8 bytes, read it */
	g_return_val_if_fail (gsf_input_read (stream, 8, buffer), FALSE);

	/* Then parse individual details
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

	header->recType = read_16bit (&buffer[2]);
	header->recLen = read_32bit (&buffer[4]);
	header->recVer = (read_16bit (buffer) & 0xF000) >> 12;
	header->recInstance = read_16bit (buffer) & 0x0FFF;

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
static void
ppt_read_text (GsfInput  *stream,
               guint8   **p_buffer,
               gsize     *p_buffer_size,
               gsize     *p_read_size)
{
	PowerPointRecordHeader header;
	gsize required_size;

	g_return_if_fail (stream);
	g_return_if_fail (p_buffer);
	g_return_if_fail (p_buffer_size);
	g_return_if_fail (p_read_size);

	/* First read the header that describes the structures type
	 * (TextBytesAtom or TextCharsAtom) and it's length.
	 */
	g_return_if_fail (ppt_read_header (stream, &header));

	/* We only want header with type either TEXTBYTESATOM_RECORD_TYPE
	 * (TextBytesAtom) or TEXTCHARSATOM_RECORD_TYPE (TextCharsAtom).
	 *
	 * We don't care about anything else
	 */
	if (header.recType != TEXTBYTESATOM_RECORD_TYPE &&
	    header.recType != TEXTCHARSATOM_RECORD_TYPE) {
		return;
	}

	/* Then we'll allocate data for the actual texts */
	if (header.recType == TEXTBYTESATOM_RECORD_TYPE) {
		/* TextBytesAtom doesn't include high bytes propably in order to
		 * save space on the ppt files. We'll have to allocate double the
		 * size for it to get the high bytes
		 */
		required_size = header.recLen * 2;
	} else {
		required_size = header.recLen;
	}

	/* Resize reused buffer if needed */
	if (required_size > *p_buffer_size) {
		*p_buffer = g_realloc (*p_buffer, required_size);
		*p_buffer_size = required_size;
	}

	/* Then read the textual data from the stream */
	if (!gsf_input_read (stream, header.recLen, *p_buffer)) {
		return;
	}

	/* Again if we are reading TextBytesAtom we'll need to add those utf16
	 * high bytes ourselves. They are zero as specified in [MS-PPT].pdf
	 * and this function's comments
	 */
	if (header.recType == TEXTBYTESATOM_RECORD_TYPE) {
		gint i;

		for (i = 0; i < header.recLen; i++) {
			/* We'll add an empty 0 byte between each byte in the array */
			(*p_buffer)[(header.recLen - i - 1) * 2] = (*p_buffer)[header.recLen - i - 1];
			(*p_buffer)[((header.recLen - i - 1) * 2) + 1] = '\0';
		}
	}

	/* Set read size as output */
	*p_read_size = required_size;
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
ppt_seek_header (GsfInput *stream,
                 gint      type1,
                 gint      type2,
                 gboolean  rewind)
{
	PowerPointRecordHeader header;

	g_return_val_if_fail (stream,FALSE);

	/* Read until we reach eof */
	while (!gsf_input_eof (stream)) {
		/* Read first header */
		g_return_val_if_fail (ppt_read_header (stream, &header), FALSE);

		/* Check if it's the correct type */
		if (header.recType == type1 || header.recType == type2) {
			/* Sometimes it's needed to rewind to the start of the
			 * header
			 */
			if (rewind) {
				gsf_input_seek (stream, -8, G_SEEK_CUR);
			}

			return TRUE;
		}

		/* If it's not the correct type, seek to the beginning of the
		 * next header
		 */
		g_return_val_if_fail (!gsf_input_seek (stream,
		                                       header.recLen,
		                                       G_SEEK_CUR),
		                      FALSE);
	}

	return FALSE;
}

static gchar *
extract_powerpoint_content (GsfInfile *infile,
                            gsize      max_bytes,
                            gboolean  *is_encrypted)
{
	/* Try to find Powerpoint Document stream */
	GsfInput *stream;
	GString *all_texts = NULL;
	gsf_off_t last_document_container;

	/* If no content requested, return */
	if (max_bytes == 0) {
		return NULL;
	}

	stream = gsf_infile_child_by_name (infile, "PowerPoint Document");

	if (is_encrypted) {
		*is_encrypted = FALSE;
	}

	if (!stream) {
		return NULL;
	}

	/* Powerpoint documents have a "editing history" stored within them.
	 * There is a structure that defines what changes were made each time
	 * but it is just easier to get the current/latest version just by
	 * finding the last occurrence of DocumentContainer structure
	 */
	last_document_container = -1;

	/* Read until we reach eof. */
	while (!gsf_input_eof (stream)) {
		PowerPointRecordHeader header;

		/*
		 * We only read headers of data structures
		 */
		if (!ppt_read_header (stream, &header)) {
			break;
		}

		/* And we only care about headers with type 1000,
		 * DocumentContainer
		 */

		if (header.recType == DOCUMENTCONTAINER_RECORD_TYPE) {
			last_document_container = gsf_input_tell (stream);
		}

		/* and then seek to the start of the next data
		 * structure so it is fast and we don't have to read
		 * through the whole file
		 */
		if (gsf_input_seek (stream, header.recLen, G_SEEK_CUR)) {
			break;
		}
	}

	/* If a DocumentContainer was found and we are able to seek to it.
	 *
	 * Then we'll have to find the second header with type
	 * SLIDELISTWITHTEXT_RECORD_TYPE since DocumentContainer
	 * contains MasterListWithTextContainer and
	 * SlideListWithTextContainer structures with both having the
	 * same header type. We however only want
	 * SlideListWithTextContainer which contains the textual
	 * content of the power point file.
	 */
	if (last_document_container >= 0 &&
	    !gsf_input_seek (stream, last_document_container, G_SEEK_SET) &&
	    ppt_seek_header (stream,
	                     SLIDELISTWITHTEXT_RECORD_TYPE,
	                     SLIDELISTWITHTEXT_RECORD_TYPE,
	                     FALSE) &&
	    ppt_seek_header (stream,
	                     SLIDELISTWITHTEXT_RECORD_TYPE,
	                     SLIDELISTWITHTEXT_RECORD_TYPE,
	                     FALSE)) {
		gsize bytes_remaining = max_bytes;
		guint8 *buffer = NULL;
		gsize buffer_size = 0;

		/*
		 * Read while we have either TextBytesAtom or
		 * TextCharsAtom and we have read less than max_bytes
		 * (in UTF-8)
		 */
		while (bytes_remaining > 0 &&
		       ppt_seek_header (stream,
		                        TEXTBYTESATOM_RECORD_TYPE,
		                        TEXTCHARSATOM_RECORD_TYPE,
		                        TRUE)) {
			gsize read_size = 0;

			/* Read the UTF-16 text in the reused buffer, and also get
			 *  number of read bytes */
			ppt_read_text (stream, &buffer, &buffer_size, &read_size);

			/* Avoid empty strings */
			if (read_size > 0) {
				/* Convert, normalize and limit max words & bytes.
				 * NOTE: `is_ansi' argument is FALSE, as the string is
				 *  always in UTF-16 */
				msoffice_convert_and_normalize_chunk (buffer,
				                                      read_size,
				                                      FALSE, /* Always UTF-16 */
				                                      &bytes_remaining,
				                                      &all_texts);
			}
		}

		g_free (buffer);
	}

	g_object_unref (stream);

	return all_texts ? g_string_free (all_texts, FALSE) : NULL;
}

/**
 * @brief Open specified uri for reading and initialize gsf
 * @param uri URI of the file to open
 * @return GsfInFile of the opened file or NULL if failed to open file
 */
static GsfInfile *
open_uri (const gchar *uri)
{
	GsfInput *input;
	GsfInfile *infile;
	gchar *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);
	input = gsf_input_stdio_new (filename, NULL);
	g_free (filename);

	if (!input) {
		return NULL;
	}

	infile = gsf_infile_msole_new (input, NULL);
	g_object_unref (G_OBJECT (input));

	return infile;
}

/* This function was programmed by using ideas and algorithms from
 * b2xtranslator project (http://b2xtranslator.sourceforge.net/)
 */
static gchar *
extract_msword_content (GsfInfile *infile,
                        gsize      n_bytes,
                        gboolean  *is_encrypted)
{
	GsfInput *document_stream, *table_stream;
	gint16 i = 0;
	guint8 tmp_buffer[4] = { 0 };
	gint fcClx, lcbClx;
	guint8 *piece_table = NULL;
	guint8 *clx = NULL;
	gint lcb_piece_table;
	gint piece_count = 0;
	gint32 fc;
	GString *content = NULL;
	guint8 *text_buffer = NULL;
	gint text_buffer_size = 0;
	gsize n_bytes_remaining;

	/* If no content requested, return */
	if (n_bytes == 0) {
		return NULL;
	}

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
	 * set to true in word 0x000A of the FIB then 1Table is used
	 */
	gsf_input_seek (document_stream, 0x000A, G_SEEK_SET);
	gsf_input_read (document_stream, 2, tmp_buffer);
	i = read_16bit (tmp_buffer);

	if ((i & 0x0200) == 0x0200) {
		table_stream = gsf_infile_child_by_name (infile, "1Table");
	} else {
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

	/* If we got an invalid or empty length of piece table, just return
	 * as we cannot iterate over pieces */
	if (lcbClx <= 0) {
		g_object_unref (document_stream);
		g_object_unref (table_stream);
		return NULL;
	}

	/* copy the structure holding the piece table into the clx array. */
	clx = g_malloc (lcbClx);
	gsf_input_seek (table_stream, fcClx, G_SEEK_SET);
	gsf_input_read (table_stream, lcbClx, clx);

	/* find out piece table from clx and set piece_table -pointer to it */
	i = 0;
	lcb_piece_table = 0;

	while (TRUE) {
		if (clx[i] == 2) {
			/* Nice, a proper structure with contents, no need to
			 * iterate more. */
			lcb_piece_table = read_32bit (clx + (i + 1));
			piece_table = clx + i + 5;
			piece_count = (lcb_piece_table - 4) / 12;
			break;
		} else if (clx[i] == 1) {
			/* Oh, a PRC structure with properties of text, not
			 * real text, so skip it */
			guint16 GrpPrl_len;

			GrpPrl_len = read_16bit (&clx[i+1]);
			/* 3 is the length of clxt (1byte) and cbGrpprl(2bytes) */
			i = i + 3 + GrpPrl_len;
		} else {
			break;
		}
	}

	/* Iterate over pieces...
	 *   Loop is halted whenever one of this conditions is met:
	 *     a) Max bytes to be read reached
	 *     b) No more pieces to read
	 */
	i = 0;
	n_bytes_remaining = n_bytes;
	while (n_bytes_remaining > 0 &&
	       i < piece_count) {
		guint8 *piece_descriptor;
		gint piece_start;
		gint piece_end;
		gint piece_size;
		gboolean is_ansi;

		/* logical position of the text piece in the document_stream */
		piece_start = read_32bit (piece_table + (i * 4));
		piece_end = read_32bit (piece_table + ((i + 1) * 4));

		/* descriptor of single piece from piece table */
		piece_descriptor = piece_table + ((piece_count + 1) * 4) + (i * 8);

		/* file character position */
		fc = read_32bit (piece_descriptor + 2);

		/* second bit is set to 1 if text is saved in ANSI encoding */
		is_ansi = (fc & 0x40000000) == 0x40000000;

		/* modify file character position according to text encoding */
		if (!is_ansi) {
			fc = (fc & 0xBFFFFFFF);
		} else {
			fc = (fc & 0xBFFFFFFF) >> 1;
		}

		piece_size  = piece_end - piece_start;

		/* NOTE: Very very long pieces may appear. In fact, a single
		 *  piece document seems to be quite normal. Thus, we limit
		 *  here the number of bytes to read from the stream, based
		 *  on the maximum number of bytes in UTF-8. Assuming, then
		 *  that a safe limit is 2*n_bytes_remaining if UTF-16 input,
		 *  and just n_bytes_remaining in CP1251 input */
		piece_size = MIN (piece_size, n_bytes_remaining);

		/* UTF-16 uses twice as many bytes as CP1252
		 *  NOTE: Not quite sure about this. Some unicode points will be
		 *  encoded using 4 bytes in UTF-16 */
		if (!is_ansi) {
			piece_size *= 2;
		}

		/* Avoid empty pieces */
		if (piece_size >= 1) {

			/* Re-allocate buffer to make it bigger if needed.
			 *  This text buffer is re-used over and over in each
			 *  iteration.  */
			if (piece_size > text_buffer_size) {
				text_buffer = g_realloc (text_buffer, piece_size);
				text_buffer_size = piece_size;
			}

			/* read and parse single text piece from document_stream */
			gsf_input_seek (document_stream, fc, G_SEEK_SET);
			gsf_input_read (document_stream, piece_size, text_buffer);

			msoffice_convert_and_normalize_chunk (text_buffer,
			                                      piece_size,
			                                      is_ansi,
			                                      &n_bytes_remaining,
			                                      &content);
		}

		/* Go on to next piece */
		i++;
	}

	g_free (text_buffer);
	g_object_unref (document_stream);
	g_object_unref (table_stream);
	g_free (clx);

	return content ? g_string_free (content, FALSE) : NULL;
}

/* Reads and interprets the flags of a given string. May be
 *  used just to skip the fields, as when this bitmask-byte
 *  comes as the first byte of a new record.
 * NOTE: For a detailed meaning of each field parsed here,
 *  take a look at the XLUnicodeRichExtendedString format:
 *  http://msdn.microsoft.com/en-us/library/dd943830.aspx
 **/
static void
read_excel_string_flags (GsfInput *stream,
                         gboolean *p_is_high_byte,
                         guint16  *p_c_run,
                         guint16  *p_cb_ext_rst)
{
	guint8 tmp_buffer[4] = { 0 };
	guint8 bit_mask;
	gboolean is_ext_string;
	gboolean is_rich_string;

	/* Note that output arguments may be NULL if we don't need
	 * their values... */

	/* Reading 1 byte for mask */
	gsf_input_read (stream, 1, tmp_buffer);
	bit_mask = read_8bit (tmp_buffer);

	/* Get flags */
	if (p_is_high_byte) {
		*p_is_high_byte = (bit_mask & 0x01) == 0x01;
	}
	is_ext_string = (bit_mask & 0x04) == 0x04;
	is_rich_string = (bit_mask & 0x08) == 0x08;

	/* If the c_run value is required as output, read it */
	if (p_c_run) {
		if (is_rich_string) {
			/* Reading 2 Bytes */
			gsf_input_read (stream, 2, tmp_buffer);

			/* Reading cRun */
			*p_c_run = read_16bit (tmp_buffer);
		} else {
			*p_c_run = 0;
		}
	} else if (is_rich_string) {
		/* If not required, just skip those bytes */
		gsf_input_seek (stream, 2, G_SEEK_CUR);
	}

	/* If the cb_ext_rst value is required as output, read it */
	if (p_cb_ext_rst) {
		if (is_ext_string) {
			/* Reading 4 Bytes */
			gsf_input_read (stream, 4, tmp_buffer);

			/* Reading cRun */
			*p_cb_ext_rst = read_16bit (tmp_buffer);
		} else {
			*p_cb_ext_rst = 0;
		}
	} else if (is_ext_string) {
		/* If not required, just skip those bytes */
		gsf_input_seek (stream, 4, G_SEEK_CUR);
	}
}

/* Returns TRUE if record was changed. BUT, the value of the
 *  current_record should be checked by the caller to know
 *  if there are no more records */
static gboolean
change_excel_record_if_needed (GsfInput *stream,
                               GArray   *record_array,
                               guint    *p_current_record)
{
	ExcelExtendedStringRecord *record;

	/* Get current record */
	record = &g_array_index (record_array,
	                         ExcelExtendedStringRecord,
	                         *p_current_record);

	/* We may already have surpassed the record, so adjust if so */
	if (gsf_input_tell (stream) >= (record->offset + record->length)) {
		/* Switch records and read from the second one... */
		(*p_current_record)++;

		if (*p_current_record < record_array->len) {
			record = &g_array_index (record_array,
			                         ExcelExtendedStringRecord,
			                         *p_current_record);

			gsf_input_seek (stream, record->offset, G_SEEK_SET);
		}

		return TRUE;
	}

	return FALSE;
}

/* Returns TRUE if correctly read
 *
 *  Note that p_current_record may get changed if the required
 *  bytes to read were split into two different records.
 */
static gboolean
read_excel_string (GsfInput *stream,
                   guint8   *buffer,
                   gsize     chunk_size,
                   GArray   *record_array,
                   guint    *p_current_record)
{
	ExcelExtendedStringRecord *record;
	gsf_off_t current_position;
	gsf_off_t current_record_end;

	/* Record may have changed when we want to read the string contents
	 *  This is a pretty special case, where the new CONTINUE record
	 * shouldn't start with a bitmask */
	if (change_excel_record_if_needed (stream, record_array, p_current_record) &&
	    *p_current_record >= record_array->len) {
		/* When reached max number of records, just return */
		return FALSE;
	}

	/* Get current record */
	record = &g_array_index (record_array,
	                         ExcelExtendedStringRecord,
	                         *p_current_record);

	/* Compute current position in the stream and end of current record*/
	current_position = gsf_input_tell (stream);
	current_record_end = record->offset + record->length;

	/* The best case is when the whole number of bytes to read are in the
	 * current record, as no record switching is therefore needed */
	if (current_position + chunk_size <= current_record_end) {
		return gsf_input_read (stream, chunk_size, buffer) != NULL ? TRUE : FALSE;
	} else if (current_record_end < current_position) {
		/* Safety check, actually pretty important */
		return FALSE;
	} else {
		/* Read the string in two chunks */
		gsize chunk_size_first_record;
		gsize chunk_size_second_record;

		/* Compute how much to read in each record */
		chunk_size_first_record = current_record_end - current_position;
		chunk_size_second_record = chunk_size - chunk_size_first_record;

		/* g_debug ("Current position:      %" GSF_OFF_T_FORMAT, current_position); */
		/* g_debug ("Current record index:  %u", *p_current_record); */
		/* g_debug ("Current record start:  %" GSF_OFF_T_FORMAT, record->offset); */
		/* g_debug ("Current record length: %" G_GSIZE_FORMAT, record->length); */
		/* g_debug ("Current record end:    %" GSF_OFF_T_FORMAT, current_record_end); */
		/* g_debug ("Bytes to read:         %" G_GSIZE_FORMAT,   chunk_size); */
		/* g_debug ("Bytes to read (1st):   %" G_GSIZE_FORMAT,   chunk_size_first_record); */
		/* g_debug ("Bytes to read (2nd):   %" G_GSIZE_FORMAT,   chunk_size_second_record); */

		/* Now, read from first record... */
		if (gsf_input_read (stream,
		                    chunk_size_first_record,
		                    buffer)) {
			/* Now switch records and read from the second one... */
			(*p_current_record)++;

			if (*p_current_record < record_array->len) {
				record = &g_array_index (record_array,
				                         ExcelExtendedStringRecord,
				                         *p_current_record);

				/* g_debug ("New record index:  %u", *p_current_record); */
				/* g_debug ("New record start:  %" GSF_OFF_T_FORMAT, record->offset); */
				/* g_debug ("New record length: %" G_GSIZE_FORMAT, record->length); */

				/* Move stream pointer to the new location, beginning of next record */
				gsf_input_seek (stream, record->offset, G_SEEK_SET);

				/* Every CONTINUE records starts with a bitmask + optional fields that
				 * should be skipped properly */
				read_excel_string_flags (stream, NULL, NULL, NULL);

				/* And finally, read the second part */
				if (gsf_input_read (stream,
				                    chunk_size_second_record,
				                    &buffer[chunk_size_first_record])) {
					/* All OK! */
					return TRUE;
				}
			}
		}

		return FALSE;
	}
}



/**
 * [MS-XLS] — v20090708
 * Excel Binary File Format (.xls) Structure Specification
 * Copyright © 2009 Microsoft Corporation.
 *  Release: Wednesday, July 8, 2009
 *
 * 2.5.293 XLUnicodeRichExtendedString
 * This structure specifies a Unicode string, which can contain
 * formatting information and phoneticstring data.

 * This structure‘s non-variable fields MUST be specified in the same
 * record. This structure‘s variable fields can be extended with
 * Continue records. A value from the table for fHighByte MUST be
 * specified in the first byte of the continue field of the Continue
 * record followed by the remaining portions of this structure‘s
 * variable fields.
 *                       1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                            cch    A B C D reserved2 cRun (optional)
 *               ...                   cbExtRst (optional)
 *               ...                   rgb (variable)
 *               ...
 *                         rgRun (variable, optional)
 *               ...
 *                         ExtRst (variable, optional)
 *               ...
 * cch (2 bytes): An unsigned integer that specifies the count of
 * characters in the string.
 *
 * A - fHighByte (1 bit): A bit that specifies whether the characters
 * in rgb are double-byte characters. MUST be a value from the
 * following table:
 *
 *  Value  Meaning
 *  0x0    All the characters in the string have a high byte of 0x00
 *         and only the low bytes are in rgb.
 *  0x1    All the characters in the string are saved as double-byte
 *         characters in rgb.
 * B - reserved1 (1 bit): MUST be zero, and MUST be ignored.
 * C - fExtSt (1 bit): A bit that specifies whether the string
 *     contains phonetic string data.
 * D - fRichSt (1 bit): A bit that specifies whether the string is a
 *     rich string and the string has at least two character formats
 *     applied.
 *
 * reserved2 (4 bits): MUST be zero, and MUST be ignored.
 *
 * cRun (2 bytes): An optional unsigned integer that specifies the
 * number of elements in rgRun. MUST exist if and only if fRichSt is
 * 0x1.
 *
 * cbExtRst (4 bytes): An optional signed integer that specifies the
 * byte count of ExtRst. MUST exist if and only if fExtSt is 0x1. MUST
 * be zero or greater.
 *
 * rgb (variable): An array of bytes that specifies the characters in
 * the string. If fHighByte is 0x0, the size of the array is cch. If
 * fHighByte is 0x1, the size of the array is cch*2. If fHighByte is
 * 0x1 and rgb is extended with a Continue record the break MUST occur
 * at the double-byte character boundary.
 *
 * rgRun (variable): An optional array of FormatRun structures that
 * specifies the formatting for each text run. The number of elements
 * in the array is cRun. MUST exist if and only if fRichSt is 0x1.
 *
 * ExtRst (variable): An optional ExtRst that specifies the phonetic
 * string data. The size of this field is cbExtRst. MUST exist if and
 * only if fExtSt is 0x1.
 */
static void
xls_get_extended_record_string (GsfInput  *stream,
                                GArray    *list,
                                gsize     *p_bytes_remaining,
                                GString  **p_content)
{
	ExcelExtendedStringRecord *record;
	guint32 cst_total;
	guint32 cst_unique;
	guint parsing_record = 0;
	guint8 tmp_buffer[4] = { 0 };
	guint i;
	guint8 *buffer = NULL;
	gsize buffer_size = 0;

	/* Parsing the record from the list */
	record = &g_array_index (list, ExcelExtendedStringRecord, parsing_record);

	/* First record parsing */
	if (gsf_input_seek (stream, record->offset, G_SEEK_SET)) {
		return;
	}

	/* Note: The first record is ALWAYS the SST, so coming with cst_total and
	 * cst_unique values.
	 * Some extra background: Records with data longer than 8,224 bytes MUST be
	 * split into several records, so in this case, if the SST record is big
	 * enough, it will have one or more CONTINUE records
	 *
	 * SST record: http://msdn.microsoft.com/en-us/library/dd773037%28v=office.12%29.aspx
	 * CONTINUE record: http://msdn.microsoft.com/en-us/library/dd949081%28v=office.12%29.aspx
	 **/

	/* Reading cst total */
	gsf_input_read (stream, 4, tmp_buffer);
	cst_total = read_32bit (tmp_buffer);

	/* Reading cst unique */
	gsf_input_read (stream, 4, tmp_buffer);
	cst_unique = read_32bit (tmp_buffer);

	/* Iterate over chunks...
	 *   Loop is halted whenever one of this conditions is met:
	 *     a) Max bytes to be read reached
	 *     b) No more chunks to read
	 */
	i = 0;
	while (*p_bytes_remaining > 0 &&
	       i < cst_unique) {
		guint16 cch;
		guint16 c_run;
		guint16 cb_ext_rst;
		gboolean is_high_byte;
		gsize chunk_size;

		/* RECORD may have been changed here */
		if (change_excel_record_if_needed (stream, list, &parsing_record) &&
		    parsing_record >= list->len) {
			/* When reached max number of records, stop loop */
			break;
		}

		/* Reading 2 bytes for cch */
		gsf_input_read (stream, 2, tmp_buffer);

		/* Reading cch - char count of current string */
		cch = read_16bit (tmp_buffer);

		/* Read string flags */
		read_excel_string_flags (stream,
		                         &is_high_byte,
		                         &c_run,
		                         &cb_ext_rst);

		/* RECORD may have been changed here, but it is managed when reading the
		 *  string contents */


		/* NOTE: In order to avoid reading unnecessary bytes, limit it based
		 * on the number of bytes remaining */
		chunk_size = MIN (cch, *p_bytes_remaining);

		/* If High Byte, chunk size *2 as stream is in UTF-16 */
		if (is_high_byte) {
			chunk_size *= 2;
		}

		/* If the new chunk size is longer than our reused buffer,
		 * make the buffer bigger */
		if (chunk_size > buffer_size) {
			buffer = g_realloc (buffer, chunk_size);
			buffer_size = chunk_size;
		}

		/* Read the chunk! NOTE that it may be split in several records... */
		if (!read_excel_string (stream, buffer, chunk_size, list, &parsing_record)) {
			break;
		}

		/* Read whole stream in one operation */
		msoffice_convert_and_normalize_chunk (buffer,
		                                      chunk_size,
		                                      !is_high_byte,
		                                      p_bytes_remaining,
		                                      p_content);

		/* Formatting string */
		if (c_run > 0) {
			/* rgRun (variable): An optional array of
			 * FormatRun structures that specifies the
			 * formatting for each ext run. The number of
			 * elements in the array is cRun. MUST exist
			 * if and only if fRichSt is 0x1.
			 *
			 * Note: As defined in MSDN, a FormatRun structure has a size
			 *  of 4 bytes, so the size of this rgRun variable is really
			 *  (4*cRun) bytes.
			 *  http://msdn.microsoft.com/en-us/library/dd921712.aspx
			 *
			 * Skiping this as it will not be useful in
			 * our case.
			 */
			gsf_input_seek (stream, 4 * c_run, G_SEEK_CUR);
			/* Note that we may be now out of the current record after having
			 * done this seek operation. */
		}

		/* ExtString */
		if (cb_ext_rst > 0) {
			/* Again its not so clear may be it will not
			 * useful in our case.
			 */
			gsf_input_seek (stream, cb_ext_rst, G_SEEK_CUR);
			/* Note that we may be now out of the current record after having
			 * done this seek operation. */
		}

		/* Go to next chunk */
		i++;
	}
}

/**
 * @brief Extract excel content from specified infile
 * @param infile file to read summary from
 * @param n_words number of max words to extract
 * @param n_bytes max number of bytes to extract
 * @param is_encrypted
 * @Notes :- About SST record
 *
 * This record specifies string constants.
 * [MS-XLS] — v20090708
 * Excel Binary File Format (.xls) Structure Specification
 * Copyright © 2009 Microsoft Corporation.
 * Release: Wednesday, July 8, 2009
 *
 * Each string constant in this record has one or more references in
 * the workbook, with the goal of improving performance in opening and
 * saving the file. The LabelSst record specifies how to make a
 * reference to a string in this record.
 *                     1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                           cstTotal
 *                           cstUnique
 *                           rgb (variable)
 *                           ...
 * cstTotal (4 bytes): A signed integer that specifies the total
 * number of references in the workbook to the strings in the shared
 * string table. MUST be greater than or equal to 0.
 *
 * cstUnique (4 bytes): A signed integer that specifies the number of
 * unique strings in the shared string table. MUST be greater than or
 * equal to 0.
 *
 * rgb (variable): An array of XLUnicodeRichExtendedString structures.
 * Records in this array are unique.
 */
static gchar*
extract_excel_content (GsfInfile *infile,
                       gsize      n_bytes,
                       gboolean  *is_encrypted)
{
	ExcelBiffHeader header1;
	GString *content = NULL;
	GsfInput *stream;
	guint saved_offset;
	gsize n_bytes_remaining = n_bytes;

	/* If no content requested, return */
	if (n_bytes == 0) {
		return NULL;
	}

	stream = gsf_infile_child_by_name (infile, "Workbook");

	if (!stream) {
		return NULL;
	}

	/* Read until we reach eof or any of our limits reached */
	while (n_bytes_remaining > 0 &&
	       !gsf_input_eof (stream)) {
		guint8 tmp_buffer[4] = { 0 };

		/* Reading 4 bytes to read header */
		gsf_input_read (stream, 4, tmp_buffer);
		header1.id = read_16bit (tmp_buffer);
		header1.length = read_16bit (tmp_buffer + 2);

		/* g_debug ("id: %d , length %d", header.id, header.length); */

		/* We are interested only in SST record */
		if (header1.id == RECORD_TYPE_SST) {
			ExcelExtendedStringRecord record;
			ExcelBiffHeader header2;
			GArray *list;
			guint length = 0;

			/* Saving length and offset so that will
			 * return to saved once we are done!!
			 */
			length = header1.length;
			saved_offset = gsf_input_tell (stream);

			/* Saving ExtendendString Record offset and
			 * length.
			 */
			record.offset = gsf_input_tell (stream);
			record.length = length;

			/* g_debug ("record.offset: %u record.length:%d",  */
			/*           record.offset, record.length); */

			/* Allocation new array of ExtendendString Record */
			list = g_array_new (TRUE, TRUE, sizeof (ExcelExtendedStringRecord));

			if (!list) {
				break;
			}

			g_array_append_val (list, record);

			/* Reading to parse continue record.
			 *
			 * Note: we are justing parsing notrequired
			 * to read data so passing null data
			 */
			gsf_input_seek (stream, length, G_SEEK_CUR);

			/* Reading & Assigning biff header 4 bytes */
			gsf_input_read (stream, 4, tmp_buffer);

			header2.id = read_16bit (tmp_buffer);
			header2.length = read_16bit (tmp_buffer + 2);

			/* g_debug ("bf id :%d length:%d", header2.id, header2.length); */
			/* g_debug ("offset: %u", (guint) gsf_input_tell (stream)); */

			while (header2.id == RECORD_TYPE_CONTINUE) {
				/* Assigning to linkedlist we will use
				 * it to read data
				 */
				record.offset = gsf_input_tell (stream);
				record.length = header2.length;
				g_array_append_val (list, record);

				/* g_debug ("record.offset: %u record.length:%d", */
				/*           record.offset, record.length); */

				/* Then parse the data from the stream */
				gsf_input_seek (stream, header2.length, G_SEEK_CUR);

				/* Reading and assigning biff header */
				gsf_input_read (stream, 4, tmp_buffer);
				header2.id = read_16bit (tmp_buffer);
				header2.length = read_16bit (tmp_buffer + 2);

				/* g_debug ("bf id :%d length:%d", header2.id, header2.length); */
			};

			/* Read extended string */
			xls_get_extended_record_string (stream,
			                                list,
			                                &n_bytes_remaining,
			                                &content);

			g_array_unref (list);

			/* Restoring the old_offset */
			gsf_input_seek (stream, saved_offset, G_SEEK_SET);
			break;
		}

		/* Moving stream pointer to record length */
		if (gsf_input_seek (stream, header1.length, G_SEEK_CUR)) {
			break;
		}
	}

	g_object_unref (stream);

	g_debug ("Bytes extracted: %" G_GSIZE_FORMAT,
	         n_bytes - n_bytes_remaining);

	return content ? g_string_free (content, FALSE) : NULL;
}

/**
 * @brief Extract summary OLE stream from specified uri
 * @param metadata where to store summary
 * @param infile file to read summary from
 * @param uri uri of the file
 */
static gboolean
extract_summary (TrackerSparqlBuilder *metadata,
                 GsfInfile            *infile,
                 const gchar          *uri)
{
	GsfInput *stream;

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	stream = gsf_infile_child_by_name (infile, "\05SummaryInformation");

	if (stream) {
		GsfDocMetaData *md;
		MetadataInfo info;
		GError *error = NULL;

		md = gsf_doc_meta_data_new ();
		error = gsf_msole_metadata_read (stream, md);

		if (error) {
			g_warning ("Could not extract summary information, %s",
			           error->message ? error->message : "no error given");

			g_error_free (error);
			g_object_unref (md);
			g_object_unref (stream);
			gsf_shutdown ();

			return FALSE;
		}

		info.metadata = metadata;
		info.uri = uri;

		gsf_doc_meta_data_foreach (md, summary_metadata_cb, &info);

		g_object_unref (md);
		g_object_unref (stream);
	}

	stream = gsf_infile_child_by_name (infile, "\05DocumentSummaryInformation");

	if (stream) {
		GsfDocMetaData *md;
		MetadataInfo info;
		GError *error = NULL;

		md = gsf_doc_meta_data_new ();

		error = gsf_msole_metadata_read (stream, md);
		if (error) {
			g_warning ("Could not extract document summary information, %s",
			           error->message ? error->message : "no error given");

			g_error_free (error);
			g_object_unref (md);
			g_object_unref (stream);
			gsf_shutdown ();

			return FALSE;
		}

		info.metadata = metadata;
		info.uri = uri;

		gsf_doc_meta_data_foreach (md, document_metadata_cb, &info);

		g_object_unref (md);
		g_object_unref (stream);
	}

	return TRUE;
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
	TrackerConfig *config;
	GFile *file = NULL;
	GFileInfo *file_info = NULL;
	const gchar *mime_used;
	GsfInfile *infile = NULL;
	gchar *content = NULL;
	gboolean is_encrypted = FALSE;
	gsize max_bytes;

	file = g_file_new_for_uri (uri);

	if (!file) {
		g_warning ("Could not create GFile for URI:'%s'",
		           uri);
		return;
	}

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
	                               G_FILE_QUERY_INFO_NONE,
	                               NULL,
	                               NULL);
	g_object_unref (file);

	if (!file_info) {
		g_warning ("Could not get GFileInfo for URI:'%s'",
		           uri);
		return;
	}

	gsf_init ();

	infile = open_uri (uri);
	if (!infile) {
		g_object_unref (file_info);
		gsf_shutdown ();
		return;
	}

	/* Extracting summary */
	extract_summary (metadata, infile, uri);

	mime_used = g_file_info_get_content_type (file_info);

	/* Set max bytes to read from content */
	config = tracker_main_get_config ();
	max_bytes = tracker_config_get_max_bytes (config);

	if (g_ascii_strcasecmp (mime_used, "application/msword") == 0) {
		/* Word file */
		content = extract_msword_content (infile, max_bytes, &is_encrypted);
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.ms-powerpoint") == 0) {
		/* PowerPoint file */
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nfo:Presentation");

		content = extract_powerpoint_content (infile, max_bytes, &is_encrypted);
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.ms-excel") == 0) {
		/* Excel File */
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nfo:Spreadsheet");

		content = extract_excel_content (infile, max_bytes, &is_encrypted);
	} else {
		g_message ("Mime type was not recognised:'%s'", mime_used);
	}

	if (content) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}

	if (is_encrypted) {
		tracker_sparql_builder_predicate (metadata, "nfo:isContentEncrypted");
		tracker_sparql_builder_object_boolean (metadata, TRUE);
	}

	g_object_unref (infile);
	g_object_unref (file_info);
	gsf_shutdown ();
}

static void
xml_start_element_handler_text_data (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     gpointer              user_data,
                                     GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;
	const gchar **a;
	const gchar **v;

	switch (info->file_type) {
	case FILE_TYPE_DOCX:
		if (g_ascii_strcasecmp (element_name, "w:pStyle") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "w:val") != 0) {
					continue;
				}

				if (g_ascii_strncasecmp (*v, "Heading", 7) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "TOC", 3) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "Section", 7) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "Title", 5) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "Subtitle", 8) == 0) {
					info->style_element_present = TRUE;
				}
			}
		} else if (g_ascii_strcasecmp (element_name, "w:rStyle") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "w:val") != 0) {
					continue;
				}

				if (g_ascii_strncasecmp (*v, "SubtleEmphasis", 14) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "SubtleReference", 15) == 0) {
					info->style_element_present = TRUE;
				}
			}
		} else if (g_ascii_strcasecmp (element_name, "w:sz") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "w:val") != 0) {
					continue;
				}

				if (atoi (*v) >= 38) {
					info->style_element_present = TRUE;
				}
			}
		} else if (g_ascii_strcasecmp (element_name, "w:smartTag") == 0) {
			info->style_element_present = TRUE;
		} else if (g_ascii_strcasecmp (element_name, "w:sdtContent") == 0) {
			info->style_element_present = TRUE;
		} else if (g_ascii_strcasecmp (element_name, "w:hyperlink") == 0) {
			info->style_element_present = TRUE;
		} else if (g_ascii_strcasecmp (element_name, "w:t") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "xml:space") != 0) {
					continue;
				}

				if (g_ascii_strncasecmp (*v, "preserve", 8) == 0) {
					info->preserve_attribute_present = TRUE;
				}
			}

			info->tag_type = MS_OFFICE_XML_TAG_WORD_TEXT;
		}
		break;

	case FILE_TYPE_XLSX:
		if (g_ascii_strcasecmp (element_name, "sheet") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "name") == 0) {
					info->tag_type = MS_OFFICE_XML_TAG_XLS_SHARED_TEXT;
				}
			}

		} else if (g_ascii_strcasecmp (element_name, "t") == 0) {
			info->tag_type = MS_OFFICE_XML_TAG_XLS_SHARED_TEXT;
		}
		break;

	case FILE_TYPE_PPTX:
	case FILE_TYPE_PPSX:
		info->tag_type = MS_OFFICE_XML_TAG_SLIDE_TEXT;
		break;

	case FILE_TYPE_INVALID:
		g_message ("Microsoft document type:%d invalid", info->file_type);
		break;
	}
}

static void
xml_end_element_handler_document_data (GMarkupParseContext  *context,
                                       const gchar          *element_name,
                                       gpointer              user_data,
                                       GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;

	if (g_ascii_strcasecmp (element_name, "w:p") == 0) {
		info->style_element_present = FALSE;
		info->preserve_attribute_present = FALSE;
	}

	((MsOfficeXMLParserInfo*) user_data)->tag_type = MS_OFFICE_XML_TAG_INVALID;
}

static void
xml_start_element_handler_core_data	(GMarkupParseContext  *context,
	 const gchar           *element_name,
	 const gchar          **attribute_names,
	 const gchar          **attribute_values,
	 gpointer               user_data,
	 GError               **error)
{
	MsOfficeXMLParserInfo *info = user_data;

	if (g_ascii_strcasecmp (element_name, "dc:title") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_TITLE;
	} else if (g_ascii_strcasecmp (element_name, "dc:subject") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_SUBJECT;
	} else if (g_ascii_strcasecmp (element_name, "dc:creator") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_AUTHOR;
	} else if (g_ascii_strcasecmp (element_name, "dc:description") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_COMMENTS;
	} else if (g_ascii_strcasecmp (element_name, "dcterms:created") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_CREATED;
	} else if (g_ascii_strcasecmp (element_name, "meta:generator") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_GENERATOR;
	} else if (g_ascii_strcasecmp (element_name, "dcterms:modified") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_MODIFIED;
	} else if (g_ascii_strcasecmp (element_name, "cp:lastModifiedBy") == 0) {
		/* Do nothing ? */
	} else if (g_ascii_strcasecmp (element_name, "Pages") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_PAGES;
	} else if (g_ascii_strcasecmp (element_name, "Slides") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_PAGES;
	} else if (g_ascii_strcasecmp (element_name, "Paragraphs") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_PARAGRAPHS;
	} else if (g_ascii_strcasecmp (element_name, "Characters") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_CHARACTERS;
	} else if (g_ascii_strcasecmp (element_name, "Words") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_WORDS;
	} else if (g_ascii_strcasecmp (element_name, "Lines") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_LINES;
	} else if (g_ascii_strcasecmp (element_name, "Application") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_APPLICATION;
	} else {
		info->tag_type = MS_OFFICE_XML_TAG_INVALID;
	}
}

static void
xml_text_handler_document_data (GMarkupParseContext  *context,
                                const gchar          *text,
                                gsize                 text_len,
                                gpointer              user_data,
                                GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;
	static gboolean found = FALSE;
	static gboolean added = FALSE;

	switch (info->tag_type) {
	case MS_OFFICE_XML_TAG_WORD_TEXT:
		if (info->style_element_present) {
			if (atoi (text) == 0) {
				tracker_text_validate_utf8 (text, -1, &info->content, NULL);
				g_string_append_c (info->content, ' ');
			}
		}

		if (info->preserve_attribute_present) {
			gchar *keywords = g_strdup (text);
			if (found) {
				tracker_text_validate_utf8 (text, -1, &info->content, NULL);
				g_string_append_c (info->content, ' ');
				found = FALSE;
			} else {
				gchar *lasts;
				gchar *keyw;

				for (keyw = strtok_r (keywords, ",; ", &lasts);
				     keyw;
				     keyw = strtok_r (NULL, ",; ", &lasts)) {
					if ((g_ascii_strncasecmp (keyw, "Table", 6) == 0) ||
					    (g_ascii_strncasecmp (keyw, "Figure", 6) == 0) ||
					    (g_ascii_strncasecmp (keyw, "Section", 7) == 0) ||
					    (g_ascii_strncasecmp (keyw, "Index", 5) == 0)) {
						found = TRUE;
					}
				}
			}

			g_free (keywords);
		}
		break;

	case MS_OFFICE_XML_TAG_SLIDE_TEXT:
		tracker_text_validate_utf8 (text, -1, &info->content, NULL);
		g_string_append_c (info->content, ' ');
		break;

	case MS_OFFICE_XML_TAG_XLS_SHARED_TEXT:
		if (atoi (text) == 0)  {
			tracker_text_validate_utf8 (text, -1, &info->content, NULL);
			g_string_append_c (info->content, ' ');
		}
		break;

	case MS_OFFICE_XML_TAG_TITLE:
		if (info->title_already_set) {
			g_warning ("Avoiding additional title (%s) in MsOffice XML document '%s'",
			           text, info->uri);
		} else {
			info->title_already_set = TRUE;
			tracker_sparql_builder_predicate (info->metadata, "nie:title");
			tracker_sparql_builder_object_unvalidated (info->metadata, text);
		}
		break;

	case MS_OFFICE_XML_TAG_SUBJECT:
		tracker_sparql_builder_predicate (info->metadata, "nie:subject");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_AUTHOR:
		tracker_sparql_builder_predicate (info->metadata, "nco:publisher");

		tracker_sparql_builder_object_blank_open (info->metadata);
		tracker_sparql_builder_predicate (info->metadata, "a");
		tracker_sparql_builder_object (info->metadata, "nco:Contact");

		tracker_sparql_builder_predicate (info->metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		tracker_sparql_builder_object_blank_close (info->metadata);
		break;

	case MS_OFFICE_XML_TAG_COMMENTS:
		tracker_sparql_builder_predicate (info->metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_CREATED: {
		gchar *date;

		date = tracker_date_guess (text);
		tracker_sparql_builder_predicate (info->metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (info->metadata, date);
		g_free (date);
		break;
	}

	case MS_OFFICE_XML_TAG_GENERATOR:
		if (!added) {
			tracker_sparql_builder_predicate (info->metadata, "nie:generator");
			tracker_sparql_builder_object_unvalidated (info->metadata, text);
			added = TRUE;
		}
		break;

	case MS_OFFICE_XML_TAG_APPLICATION:
		/* FIXME: Same code as MS_OFFICE_XML_TAG_GENERATOR should be
		 * used, but nie:generator has max cardinality of 1
		 * and this would cause errors.
		 */
		break;

	case MS_OFFICE_XML_TAG_MODIFIED: {
		gchar *date;

                date = tracker_date_guess (text);
		tracker_sparql_builder_predicate (info->metadata, "nie:contentLastModified");
		tracker_sparql_builder_object_unvalidated (info->metadata, date);
                g_free (date);
		break;
	}

	case MS_OFFICE_XML_TAG_NUM_OF_PAGES:
		tracker_sparql_builder_predicate (info->metadata, "nfo:pageCount");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_NUM_OF_CHARACTERS:
		tracker_sparql_builder_predicate (info->metadata, "nfo:characterCount");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_NUM_OF_WORDS:
		tracker_sparql_builder_predicate (info->metadata, "nfo:wordCount");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_NUM_OF_LINES:
		tracker_sparql_builder_predicate (info->metadata, "nfo:lineCount");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_NUM_OF_PARAGRAPHS:
		/* TODO: There is no ontology for this. */
		break;

	case MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA:
	case MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA:
		/* Nothing as we are using it in defining type of data */
		break;

	case MS_OFFICE_XML_TAG_INVALID:
		/* Here we cant use log otheriwse it will print for other non useful files */
		break;
	}
}

static gboolean
xml_read (MsOfficeXMLParserInfo *parser_info,
          const gchar           *xml_filename,
          MsOfficeXMLTagType     type)
{
	GMarkupParseContext *context;
	MsOfficeXMLParserInfo info;

	/* FIXME: Can we use the original info here? */
	info.metadata = parser_info->metadata;
	info.file_type = parser_info->file_type;
	info.tag_type = MS_OFFICE_XML_TAG_INVALID;
	info.style_element_present = FALSE;
	info.preserve_attribute_present = FALSE;
	info.uri = parser_info->uri;
	info.content = parser_info->content;
	info.title_already_set = parser_info->title_already_set;

	switch (type) {
	case MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA: {
		GMarkupParser parser = {
			xml_start_element_handler_core_data,
			xml_end_element_handler_document_data,
			xml_text_handler_document_data,
			NULL,
			NULL
		};

		context = g_markup_parse_context_new (&parser,
		                                      0,
		                                      &info,
		                                      NULL);
		break;
	}

	case MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA: {
		GMarkupParser parser = {
			xml_start_element_handler_text_data,
			xml_end_element_handler_document_data,
			xml_text_handler_document_data,
			NULL,
			NULL
		};

		context = g_markup_parse_context_new (&parser,
		                                      0,
		                                      &info,
		                                      NULL);
		break;
	}

	default:
		context = NULL;
		break;
	}

	if (context) {
		/* Load the internal XML file from the Zip archive, and parse it
		 * using the given context */
		tracker_gsf_parse_xml_in_zip (parser_info->uri,
		                              xml_filename,
		                              context, NULL);
		g_markup_parse_context_free (context);
	}

	return TRUE;
}

static void
xml_start_element_handler_content_types (GMarkupParseContext  *context,
                                         const gchar          *element_name,
                                         const gchar         **attribute_names,
                                         const gchar         **attribute_values,
                                         gpointer              user_data,
                                         GError              **error)
{
	MsOfficeXMLParserInfo *info;
	const gchar *part_name;
	const gchar *content_type;
	gint i;

	info = user_data;

	if (g_ascii_strcasecmp (element_name, "Override") != 0) {
		info->tag_type = MS_OFFICE_XML_TAG_INVALID;
		return;
	}

	part_name = NULL;
	content_type = NULL;

	for (i = 0; attribute_names[i]; i++) {
		if (g_ascii_strcasecmp (attribute_names[i], "PartName") == 0) {
			part_name = attribute_values[i];
		} else if (g_ascii_strcasecmp (attribute_names[i], "ContentType") == 0) {
			content_type = attribute_values[i];
		}
	}

	/* Both part_name and content_type MUST be NON-NULL */
	if (!part_name || !content_type) {
		g_message ("Invalid file (part_name:%s, content_type:%s)",
		           part_name ? part_name : "none",
		           content_type ? content_type : "none");
		return;
	}

	if ((g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-package.core-properties+xml") == 0) ||
	    (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.extended-properties+xml") == 0)) {
		xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA);
		return;
	}

	switch (info->file_type) {
	case FILE_TYPE_DOCX:
		if (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml") == 0) {
			xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA);
		}
		break;

	case FILE_TYPE_PPTX:
	case FILE_TYPE_PPSX:
		if ((g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.presentationml.slide+xml") == 0) ||
		    (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.drawingml.diagramData+xml") == 0)) {
			xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA);
		}
		break;

	case FILE_TYPE_XLSX:
		if ((g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml") == 0) ||
		    (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml") == 0)) {
			xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA);
		}
		break;

	case FILE_TYPE_INVALID:
		g_message ("Invalid file type:'%d'", info->file_type);
		break;
	}
}

static void
extract_msoffice_xml (const gchar          *uri,
                      TrackerSparqlBuilder *preupdate,
                      TrackerSparqlBuilder *metadata)
{
	MsOfficeXMLParserInfo info;
	MsOfficeXMLFileType file_type;
	GFile *file;
	GFileInfo *file_info;
	GMarkupParseContext *context = NULL;
	GMarkupParser parser = {
		xml_start_element_handler_content_types,
		xml_end_element_handler_document_data,
		NULL,
		NULL,
		NULL
	};
	const gchar *mime_used;

	file = g_file_new_for_uri (uri);

	if (!file) {
		g_warning ("Could not create GFile for URI:'%s'",
		           uri);
		return;
	}

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
	                               G_FILE_QUERY_INFO_NONE,
	                               NULL,
	                               NULL);
	g_object_unref (file);

	if (!file_info) {
		g_warning ("Could not get GFileInfo for URI:'%s'",
		           uri);
		return;
	}

	mime_used = g_file_info_get_content_type (file_info);

	if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.wordprocessingml.document") == 0) {
		file_type = FILE_TYPE_DOCX;
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.presentationml.presentation") == 0) {
		file_type = FILE_TYPE_PPTX;
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.presentationml.slideshow") == 0) {
		file_type = FILE_TYPE_PPSX;
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") == 0) {
		file_type = FILE_TYPE_XLSX;
	} else {
		g_message ("Mime type was not recognised:'%s'", mime_used);
		file_type = FILE_TYPE_INVALID;
	}

	g_object_unref (file_info);


	g_debug ("Extracting MsOffice XML format...");

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	info.metadata = metadata;
	info.file_type = file_type;
	info.tag_type = MS_OFFICE_XML_TAG_INVALID;
	info.style_element_present = FALSE;
	info.preserve_attribute_present = FALSE;
	info.uri = uri;
	info.content = g_string_new ("");
	info.title_already_set = FALSE;

	context = g_markup_parse_context_new (&parser, 0, &info, NULL);

	/* Load the internal XML file from the Zip archive, and parse it
	 * using the given context */
	tracker_gsf_parse_xml_in_zip (uri,
	                              "[Content_Types].xml",
	                              context, NULL);

	if (info.content) {
		gchar *content;

		content = g_string_free (info.content, FALSE);
		info.content = NULL;

		if (content) {
			tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
			tracker_sparql_builder_object_unvalidated (metadata, content);
			g_free (content);
		}
	}

	g_markup_parse_context_free (context);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
