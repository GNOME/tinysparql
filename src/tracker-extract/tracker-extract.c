/* Tracker Extract - extracts embedded metadata from files
 *
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)	
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */

#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <glib.h>
#include "../libextractor/src/include/extractor.h"


typedef enum {
	IGNORE_METADATA,
	NO_METADATA,
	DOC_METADATA,
	IMAGE_METADATA,
	VIDEO_METADATA,
	AUDIO_METADATA
} MetadataFileType;

struct metadata_format
{
  char metadata_name[20];
  EXTRACTOR_KeywordType key;
}; 

static EXTRACTOR_ExtractorList *plugins;

/* document mime type specific metadata groups - NB mime types below are prefixes */
static  char *doc_mime_types[] = {"application/msword","application/pdf", "application/postscript","application/x-dvi",
				  "application/vnd.ms-excel", "vnd.ms-powerpoint", "application/vnd.oasis",
				  "application/vnd.sun.xml",  "application/vnd.stardivision", "application/x-abiword",
				  "text/html","text/sgml", "text/xml" };


/* libextractor keywords we are inteersted in for each Metadata file type */
static struct metadata_format doc_keywords[] =
{
	{"Doc.Title", EXTRACTOR_TITLE},
	{"Doc.Subject", EXTRACTOR_SUBJECT},
	{"Doc.Author", EXTRACTOR_AUTHOR},
	{"Doc.Keywords", EXTRACTOR_KEYWORDS},
	{"Doc.Comments", EXTRACTOR_DESCRIPTION},
	{"Doc.PageCount", EXTRACTOR_PAGE_COUNT},
	{"Doc.Created", EXTRACTOR_CREATION_DATE},
	{"EOF", EXTRACTOR_UNKNOWN}
};

static struct metadata_format audio_keywords[] =
{
	{"Audio.Title", EXTRACTOR_TITLE},
	{"Audio.Artist", EXTRACTOR_ARTIST},
	{"Audio.Album", EXTRACTOR_ALBUM},
	{"Audio.Year", EXTRACTOR_DATE},
	{"Audio.Comment", EXTRACTOR_COMMENT},
	{"Audio.Genre", EXTRACTOR_GENRE},
	{"Audio.Codec", EXTRACTOR_RESOURCE_TYPE},
	{"Audio.Format", EXTRACTOR_FORMAT},
	{"EOF", EXTRACTOR_UNKNOWN}
};

static struct metadata_format video_keywords[] =
{
	{"Video.Title", EXTRACTOR_TITLE},
	{"Video.Author", EXTRACTOR_ARTIST},
	{"Video.Size", EXTRACTOR_SIZE},
	{"Video.Codec", EXTRACTOR_RESOURCE_TYPE},
	{"Video.Format", EXTRACTOR_FORMAT},
	{"EOF", EXTRACTOR_UNKNOWN}
};

static struct metadata_format image_keywords[] =
{
	{"Image.Size", EXTRACTOR_SIZE},
	{"Image.Date", EXTRACTOR_DATE},
	{"Image.Keywords", EXTRACTOR_KEYWORDS},
	{"Image.Creator", EXTRACTOR_CREATOR},
	{"Image.Comments", EXTRACTOR_COMMENT},
	{"Image.Description", EXTRACTOR_DESCRIPTION},
	{"Image.Software", EXTRACTOR_SOFTWARE},
	{"Image.CameraMake", EXTRACTOR_CAMERA_MAKE},
	{"Image.CameraModel", EXTRACTOR_CAMERA_MODEL},
	{"EOF", EXTRACTOR_UNKNOWN}
};


static MetadataFileType
get_metadata_type (const char *mime)
{
	int i;
	int num_elements;

	if (!mime) {
		return NO_METADATA;
	}
	
	if (g_str_has_prefix (mime, "image")) {
		return IMAGE_METADATA;		
	} else { 
		if (g_str_has_prefix (mime, "video")) {
			return IGNORE_METADATA;		
		} else { 
			if (g_str_has_prefix (mime, "audio")) {
				return AUDIO_METADATA;		
			} else { 
				num_elements = sizeof (doc_mime_types) / sizeof (char *);
				for (i =0; i < num_elements; i++ ) {
					if (g_str_has_prefix (mime, doc_mime_types [i] )) {
						return DOC_METADATA;
					}
				}
			}
		}
	}
	return NO_METADATA;
}

static const char * 
get_metadata_name (MetadataFileType meta_type, EXTRACTOR_KeywordType keyword)
{
	int count = 0;
	
	switch (meta_type) {

  		case IGNORE_METADATA:
  		case NO_METADATA:

			break;
			
		case DOC_METADATA :

			while (doc_keywords[count].key != EXTRACTOR_UNKNOWN) {

				if (doc_keywords[count].key == keyword) {
					return doc_keywords[count].metadata_name;
				}
				count++;
			}
			break;
			
		case IMAGE_METADATA :

			while (image_keywords[count].key != EXTRACTOR_UNKNOWN) {

				if (image_keywords[count].key == keyword) {
					return image_keywords[count].metadata_name;
				}
				count++;
			}
			break;
			
		case VIDEO_METADATA :

			while (video_keywords[count].key != EXTRACTOR_UNKNOWN) {

				if (video_keywords[count].key == keyword) {
					return video_keywords[count].metadata_name;
				}
				count++;
			}
			break;

			
		case AUDIO_METADATA :

			while (audio_keywords[count].key != EXTRACTOR_UNKNOWN) {

				if (audio_keywords[count].key == keyword) {
					return audio_keywords[count].metadata_name;
				}
				count++;
			}
			break;
	
	}

	return NULL;
}

static void
process_metadata (GHashTable *meta_table)
{
	char *str, *key;
	char *sep = NULL, *label = NULL;
	char tmp_buf[20];
	int i;
	
	/* make sure dates are in the correct format (yyyy-mm-dd hh:mm:ss) */
	if (g_hash_table_lookup_extended  (meta_table, "Doc.Created", (gpointer)&key, (gpointer)&str)) {

		char *new_date = g_strstrip (g_strdup (str));

		/* replace 'T' with space in OpenOffice date formats EG 2005-11-19T11:21:33 */ 
      		sep = strchr (new_date, 'T');
      		if (sep) {
			*sep = ' ';
		}

		/* strip any time zone info from dates */
		sep = strchr (new_date, 'Z');
      		if (sep) {
			*sep = '\0';
		}
		sep = strchr (new_date, '-');
      		if (sep) {
			*sep = '\0';
		}
		sep = strchr (new_date, '+');
      		if (sep) {
			*sep = '\0';
		}

		/* format pdf dates which look like yyyymmddhhmmss EG. 20030415145048 */
      		if (strlen (new_date) == 14) {

			tmp_buf[0] = new_date[0];
			tmp_buf[1] = new_date[1];
			tmp_buf[2] = new_date[2];
			tmp_buf[3] = new_date[3];
			tmp_buf[4] = '-';
			tmp_buf[5] = new_date[4];
			tmp_buf[6] = new_date[5];
			tmp_buf[7] = '-';
			tmp_buf[8] = new_date[6];
			tmp_buf[9] = new_date[7];
			tmp_buf[10] = ' ';
			tmp_buf[11] = new_date[8];
			tmp_buf[12] = new_date[9];
			tmp_buf[13] = ':';
			tmp_buf[14] = new_date[10];
			tmp_buf[15] = new_date[11];
			tmp_buf[16] = ':';
			tmp_buf[17] = new_date[12];
			tmp_buf[18] = new_date[13];
			tmp_buf[19] = '\0';
			
			g_free (new_date);
			new_date = g_strdup (tmp_buf);
			
		}
	
		/* remove invalid length dates */
		if (strlen (new_date) != 19) {
			g_hash_table_remove (meta_table, key);
		} else {
			if (strcmp (new_date, str) != 0) {
				g_hash_table_remove (meta_table, key);
				g_hash_table_insert (meta_table, g_strdup ("Doc.Created"), g_strdup (new_date));
				g_free (new_date);
			}
		}

	}


	/* split up image size field into height & width */
	if (g_hash_table_lookup_extended  (meta_table, "Image.Size", (gpointer)&key, (gpointer)&str)) {

		char *image_size = g_strstrip (g_strdup (str));

      		sep = strchr (image_size, 'x');
      		if (sep) {
			*sep = '\0';
			label = g_strdup (sep + 1);
			g_hash_table_insert (meta_table, g_strdup ("Image.Width"), g_strdup (image_size));
			g_hash_table_insert (meta_table, g_strdup ("Image.Height"), g_strdup (label));
			g_free (label);
			
		}
		g_free (image_size);
		g_hash_table_remove (meta_table, key);
	}

	/* split up Audio.Format into bitrate, samplerate, length and channels: 128 kbps, 44100 hz, 5m15 stereo */
	if (g_hash_table_lookup_extended  (meta_table, "Audio.Format", (gpointer)&key, (gpointer)&str)) {
		
		char **strv = g_strsplit (str, ",", 3);
		
		if (strv[0] && strv[1] && strv[2]) {

			char *tmp = g_strstrip (g_strdup (strv[0]));
			
			sep = strchr (tmp, ' ');
      			if (sep) {
				*sep = '\0';
				g_hash_table_insert (meta_table, g_strdup ("Audio.Bitrate"), g_strdup (tmp));
			}
			g_free (tmp);


			tmp =  g_strstrip (g_strdup (strv[1]));

			sep = strchr (tmp, ' ');
      			if (sep) {
				*sep = '\0';
				g_hash_table_insert (meta_table, g_strdup ("Audio.Samplerate"), g_strdup (tmp));
			}
			g_free (tmp);



			tmp =  g_strstrip (g_strdup (strv[2]));

			sep = strchr (tmp, ' ');
      			if (sep) {
				label = g_strdup (sep + 1);

				if ( strcmp (label, "stereo") == 0) {
					g_hash_table_insert (meta_table, g_strdup ("Audio.Channels"), g_strdup ("2"));
				} else {
					g_hash_table_insert (meta_table, g_strdup ("Audio.Channels"), g_strdup ("1"));
				}

				g_free (label);
			}
			g_free (tmp);


		} 
		g_strfreev (strv);   
		g_hash_table_remove (meta_table, key);
	}

}



static GHashTable *
tracker_get_file_metadata (const char *uri, char *mime)
{

	EXTRACTOR_KeywordList   *keywords;
	char 			*value=NULL, *other_values=NULL, *tmp=NULL;
	MetadataFileType 	meta_type;
	GHashTable 		*meta_table;
	char 			*meta_name;

	
	if (!uri) {
		return NULL;
	}

	if (!g_file_test (uri, G_FILE_TEST_EXISTS)) {
		return NULL;
	}

	keywords = EXTRACTOR_getKeywords (plugins, uri);
	EXTRACTOR_removeEmptyKeywords (keywords);

	if (!mime) {
		mime = g_strdup (EXTRACTOR_extractLast (EXTRACTOR_MIMETYPE, keywords));
	}

	EXTRACTOR_removeKeywordsOfType (keywords, EXTRACTOR_MIMETYPE);

	meta_type = get_metadata_type (mime);

	

	/* don't bother with video metadata - its really really slow to extract + there's bugger all metadata anyhow! */
	if (mime && g_str_has_prefix (mime, "video")) {
		EXTRACTOR_freeKeywords (keywords);
		return NULL;		
	}



	meta_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);	

	while (keywords != NULL) {
 
		if (!g_utf8_validate (keywords->keyword, -1, NULL)) {
			value = g_locale_to_utf8 (keywords->keyword, -1, NULL, NULL, NULL);
      		} else {
			value = g_strdup (keywords->keyword);
		}

     		if (value) {
	
			/* replace any embedded semicolons or "=" as we use them for delimiters */
			value = g_strdelimit (value, ";", ',');
			value = g_strdelimit (value, "=", '-');
			value = g_strstrip (value);
			meta_name = g_strdup (get_metadata_name (meta_type, keywords->keywordType));
			if (meta_name) {
				char *st = g_hash_table_lookup (meta_table, meta_name);
				if (st == NULL) {
					g_hash_table_insert (meta_table, g_strdup (meta_name), g_strdup (value));
				}
				g_free (meta_name);
			} else {
				if (other_values) {
					tmp = g_strdup (other_values);
					g_free (other_values);
					other_values = g_strconcat (tmp," ", value, NULL);
					g_free (tmp);
				} else {
					other_values = g_strdup (value);
				}
			}
			g_free (value);
		}
		
		keywords = keywords->next;
	}

	if (other_values && strlen (other_values) > 2) {
		g_hash_table_insert (meta_table, g_strdup ("File.Other"), g_strdup (other_values));
		g_free (other_values);
	}


  	EXTRACTOR_freeKeywords (keywords);

	/* post process metadata as some of libextractors metadata needs splitting up */
	process_metadata (meta_table);

	g_free (mime);

	return meta_table;
}

static void
get_meta_table_data (gpointer key, gpointer value, gpointer user_data)
{

	g_return_if_fail (key || value);
	
	g_print ("%s=%s;\n", (char *)key, (char *)value);
	
}

int
main (int argc, char **argv) 
{

	GHashTable *meta;


	if (!argv[1]) {
		g_print ("usage: tracker-extract file [mimetype]\n");
		return 0;
	}
	
	/* initialise metadata plugins */
	plugins = EXTRACTOR_loadDefaultLibraries();

	if (argv[2]) {
		meta = tracker_get_file_metadata (argv[1], g_strdup (argv[2]));
	} else {
		meta = tracker_get_file_metadata (argv[1], NULL);
	}

	if (meta) {
		g_hash_table_foreach (meta, get_meta_table_data, NULL);
		g_hash_table_destroy (meta);
	}

	EXTRACTOR_removeAll (plugins);
}

