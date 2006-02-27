/* Tracker 
 * utility routines
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 */ 
 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tracker-metadata.h"
#include "tracker-utils.h"

typedef enum {
	IGNORE_METADATA,
	NO_METADATA,
	DOC_METADATA,
	IMAGE_METADATA,
	VIDEO_METADATA,
	AUDIO_METADATA,
	TEXT_METADATA
} MetadataFileType;


/* document mime type specific metadata groups - NB mime types below may be prefixes */
char *doc_mime_types[] = {"application/msword","application/pdf", "application/postscript","application/x-dvi",
				  "application/vnd.ms-excel", "vnd.ms-powerpoint", "application/vnd.oasis",
				  "application/vnd.sun.xml",  "application/vnd.stardivision", "application/x-abiword",
				  "text/html","text/sgml", "text/xml" }; 

char *text_mime_types[] = {"text/plain","text/x-h", "text/x-c","text/x-script" , "text/css" , "text/x-java-source" ,
			   "text/x-pascal", "text/pascal"};

static MetadataFileType
tracker_get_metadata_type (const char *mime)
{
	int i;
	int num_elements;
	
	if (g_str_has_prefix (mime, "image")) {
		return IMAGE_METADATA;		

	} else 	if (g_str_has_prefix (mime, "video")) {
			/* don't bother with video metadata - its really really slow to extract + there's bugger all metadata anyhow! */
			return IGNORE_METADATA;		

	} else 	if (g_str_has_prefix (mime, "audio")) {
			return AUDIO_METADATA;	
	
	} else { 
		num_elements = sizeof (doc_mime_types) / sizeof (char *);
		for (i =0; i < num_elements; i++ ) {
			if (g_str_has_prefix (mime, doc_mime_types [i] )) {
				return DOC_METADATA;
			}
		}
	}
		
	num_elements = sizeof (text_mime_types) / sizeof (char *);
	for (i =0; i < num_elements; i++ ) {
		if (g_str_has_prefix (mime, text_mime_types [i] )) {
			return TEXT_METADATA;	
		}
	}
	return NO_METADATA;
}


char *
tracker_metadata_get_text_file (const char *uri, const char *mime)
{
	char *tmp, *text_filter_file;

	/* no need to filter text based files - index em directly */
	if (tracker_get_metadata_type (mime) == TEXT_METADATA) {

		return g_strdup (uri);
	
	} else {

		tmp = g_strdup (DATADIR "/tracker/filters/");
	
		text_filter_file = g_strconcat (tmp, mime, "_filter", NULL);

		g_free (tmp);

	}
	
	if (g_file_test (text_filter_file, G_FILE_TEST_EXISTS)) {

		char *argv[4];
		char *temp_file_name;
		int fd;

		fd = g_file_open_tmp (NULL, &temp_file_name, NULL);

  		if (fd == -1) {
			g_warning ("make thumb file %s failed", temp_file_name);
			return NULL;
      		} else {
			close (fd);
		}

		argv[0] = g_strdup (text_filter_file);
		argv[1] = g_strdup (uri);
		argv[2] = g_strdup (temp_file_name);
		argv[3] = NULL;


		g_free (text_filter_file);

		tracker_log ("extracting text for %s using filter %s", argv[1], argv[0]);
	
		if (g_spawn_sync (NULL,
				  argv,
				  NULL, 
				  G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL)) {

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
			return temp_file_name;

		} else {
			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
			return NULL;
		}

	} else {
		g_free (text_filter_file);
	}
	
	return NULL;
}

char *
tracker_metadata_get_thumbnail (const char *uri, const char *mime, const char *max_size)
{
	char *tmp, *tmp_file,  *thumbnailer;

	tmp = g_strdup (DATADIR "/tracker/thumbnailers/");

	thumbnailer = g_strconcat (tmp, mime, "_thumbnailer", NULL);

	g_free (tmp);

	if (g_file_test (thumbnailer, G_FILE_TEST_EXISTS)) {

		char *argv[5];
		int fd;
	
		tmp = g_build_filename ( g_get_home_dir (), ".Tracker", "thumbs", NULL);
		
		g_mkdir_with_parents (tmp, 0700);
		
		tmp_file = g_strconcat (tmp, "/", "thumb_XXXXXX", NULL);
		g_free (tmp);

		

		fd = g_mkstemp (tmp_file);
  		if (fd == -1) {
			g_warning ("make thumb file %s failed", tmp_file);
			g_free (thumbnailer);
			g_free (tmp_file);
			return NULL;
      		} else {
			close (fd);
		}

		argv[0] = g_strdup (thumbnailer);
		argv[1] = g_strdup (uri);
		argv[2] = g_strdup (tmp_file);
		argv[3] = g_strdup (max_size);
		argv[4] = NULL;
		
		tracker_log ("Extracting thumbnail for %s using %s", argv[1], argv[0] );

		if (g_spawn_sync (NULL,
				  argv,
				  NULL,
				  G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL)) {

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
			g_free (argv[3]);
			g_free (thumbnailer);
			return tmp_file;

		} else {
			g_free (tmp_file);
			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
			g_free (argv[3]);
		}
	}
	g_free (thumbnailer);
	return NULL;	
}


void
tracker_metadata_get_embedded (const char *uri, const char *mime, GHashTable *table)
{
	MetadataFileType meta_type; 

	if (!uri || !mime || !table) {
		return;
	} 

	meta_type = tracker_get_metadata_type (mime);

	if (meta_type == DOC_METADATA || meta_type == IMAGE_METADATA || meta_type == AUDIO_METADATA) {
		
		char *argv[4];
		char *value;

		/* we extract metadata out of process using pipes */

		argv[0] = g_strdup ("tracker-extract");
		argv[1] = g_strdup (uri);
		argv[2] = g_strdup (mime);
		argv[3] = NULL;

		if (g_spawn_sync (NULL,
				  argv,
				  NULL,
				  G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
				  NULL,
				  NULL,
				  &value,
				  NULL,
				  NULL,
				  NULL)) {
			
			/* parse returned stdout (value) and extract keys and associated metadata values */

			if (value && strchr (value, '=') && strchr (value, ';') ) {
				
				char *meta_name = NULL;
				char *meta_value = NULL;
				char *sep;
				char **values, **values_p;
				
				values = g_strsplit_set (value, ";", -1);

				for (values_p = values; *values_p; values_p++) {

					if (*values_p) {
						
						meta_name = g_strdup (g_strstrip(*values_p));
						
						sep = strchr (meta_name, '=');
      						
						if (sep) {
							*sep = '\0';
							meta_value = g_strdup (sep + 1);
								
							if (meta_value) {

								char *st = NULL;
					
								st = g_hash_table_lookup (table, meta_name);
						
								if (st == NULL) {
									tracker_log ("%s = %s", meta_name, meta_value);
									g_hash_table_insert (table, g_strdup (meta_name), g_strdup (meta_value));
								}
						
								g_free (meta_value);
							}
						
						}
						g_free (meta_name);
					}
				}

				g_strfreev (values);
			}



		if (value) { 
			g_free (value);		
		}

		g_free (argv[0]);
		g_free (argv[1]);
		g_free (argv[2]);
		}
		
	}
}
