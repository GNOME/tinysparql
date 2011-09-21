/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <errno.h>
#include <string.h>

#include <glib.h>

#include <libtracker-common/tracker-file-utils.h>

#include <gsf/gsf.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-infile-zip.h>

#include "tracker-gsf.h"

/* Size of the buffer to use */
#define XML_BUFFER_SIZE            8192         /* bytes */
/* Note: 20 MBytes of max size is really assumed to be a safe limit. */
#define XML_MAX_BYTES_READ         (20u << 20)  /* bytes */

/**
 * based on find_member() from vsd_utils.c:
 * http://vsdump.sourcearchive.com/documentation/0.0.44/vsd__utils_8c-source.html
 */
static GsfInput *
find_member (GsfInfile *arch,
             gchar const *name)
{
	gchar const *slash;

	slash = strchr (name, '/');

	if (slash) {
		gchar *dirname;
		GsfInput *member;

		dirname = g_strndup (name, slash - name);

		if ((member = gsf_infile_child_by_name (arch, dirname)) != NULL) {
			GsfInfile *dir;

			dir = GSF_INFILE (member);
			member = find_member (dir, slash + 1);
			g_object_unref (dir);
		}

		g_free (dirname);
		return member;
	} else {
		return gsf_infile_child_by_name (arch, name);
	}
}

/**
 * tracker_gsf_parse_xml_in_zip:
 * @zip_file_uri: URI of the ZIP archive
 * @xml_filename: Name of the XML file stored inside the ZIP archive
 * @context: Markup context to be used when parsing the XML
 *
 * This function reads and parses the contents of an XML file stored
 *  inside a ZIP compressed archive. Reading and parsing is done buffered, and
 *  maximum size of the uncompressed XML file is limited to be to 20MBytes.
 */
void
tracker_gsf_parse_xml_in_zip (const gchar          *zip_file_uri,
                              const gchar          *xml_filename,
                              GMarkupParseContext  *context,
                              GError              **err)
{
	gchar *filename;
	GError *error = NULL;
	GsfInfile *infile = NULL;
	GsfInput *src = NULL;
	GsfInput *member = NULL;
	FILE *file;

	g_debug ("Parsing '%s' XML file from '%s' zip archive...",
	         xml_filename, zip_file_uri);

	/* Get filename from the given URI */
	if ((filename = g_filename_from_uri (zip_file_uri,
	                                     NULL, &error)) == NULL) {
		g_warning ("Can't get filename from uri '%s': %s",
		           zip_file_uri, error ? error->message : "no error given");
	} else { /* Create a new Input GSF object for the given file */

		file = tracker_file_open (filename, "rb", FALSE);
		if (!file) {
			g_warning ("Can't open file from uri '%s': %s",
			           zip_file_uri, g_strerror (errno));
		} else if ((src = gsf_input_stdio_new_FILE (filename, file, TRUE)) == NULL) {
			g_warning ("Failed creating a GSF Input object for '%s': %s",
			           zip_file_uri, error ? error->message : "no error given");
		}
		/* Input object is a Zip file */
		else if ((infile = gsf_infile_zip_new (src, &error)) == NULL) {
			g_warning ("'%s' Not a zip file: %s",
			           zip_file_uri, error ? error->message : "no error given");
		}
		/* Look for requested filename inside the ZIP file */
		else if ((member = find_member (infile, xml_filename)) == NULL) {
			g_warning ("No member '%s' in zip file '%s'",
			           xml_filename, zip_file_uri);
		}
		/* Load whole contents of the internal file in the xml buffer */
		else {
			guint8 buf[XML_BUFFER_SIZE];
			size_t remaining_size, chunk_size, accum;

			/* Get whole size of the contents to read */
			remaining_size = (size_t) gsf_input_size (GSF_INPUT (member));

			/* Note that gsf_input_read() needs to be able to read ALL specified
			 *  number of bytes, or it will fail */
			chunk_size = MIN (remaining_size, XML_BUFFER_SIZE);

			accum = 0;
			while (!error &&
			       accum  <= XML_MAX_BYTES_READ &&
			       chunk_size > 0 &&
			       gsf_input_read (GSF_INPUT (member), chunk_size, buf) != NULL) {

				/* update accumulated count */
				accum += chunk_size;

				/* Pass the read stream to the context parser... */
				g_markup_parse_context_parse (context, buf, chunk_size, &error);

				/* update bytes to be read */
				remaining_size -= chunk_size;
				chunk_size = MIN (remaining_size, XML_BUFFER_SIZE);
			}
		}

		if (file) {
			tracker_file_close (file, FALSE);
		}
	}

	g_free (filename);

	if (error)
		g_propagate_error (err, error);
	if (infile)
		g_object_unref (infile);
	if (src)
		g_object_unref (src);
	if (member)
		g_object_unref (member);
}

