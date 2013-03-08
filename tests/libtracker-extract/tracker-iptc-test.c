/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include <jpeglib.h>

#include <glib.h>
#include <gio/gio.h>
#include <libtracker-extract/tracker-iptc.h>

#define PS3_NAMESPACE           "Photoshop 3.0\0"
#define PS3_NAMESPACE_LENGTH    14
#include <libiptcdata/iptc-jpeg.h>

struct tej_error_mgr {
	struct jpeg_error_mgr jpeg;
	jmp_buf setjmp_buffer;
};


static void
extract_jpeg_error_exit (j_common_ptr cinfo)
{
	struct tej_error_mgr *h = (struct tej_error_mgr *) cinfo->err;
	(*cinfo->err->output_message)(cinfo);
	longjmp (h->setjmp_buffer, 1);
}

/*
 * libiptcdata doesn't scan the file until find the IPTC blob.
 * We need to find the blob ourselves. This code comes from tracker-extract-jpeg
 */
static TrackerIptcData *
load_iptc_blob (const gchar *filename)
{
	struct jpeg_decompress_struct cinfo;
	struct tej_error_mgr tejerr;
	struct jpeg_marker_struct *marker;
	TrackerIptcData *id = NULL;
        FILE *f;
        gchar *uri;
        GFile *file;

        file = g_file_new_for_path (filename);
        uri = g_file_get_uri (file);
        g_object_unref (file);

        f = fopen (filename, "r");

	cinfo.err = jpeg_std_error (&tejerr.jpeg);
	tejerr.jpeg.error_exit = extract_jpeg_error_exit;
	if (setjmp (tejerr.setjmp_buffer)) {
                fclose (f);
                g_free (uri);
                return NULL;
	}

	jpeg_create_decompress (&cinfo);

	jpeg_save_markers (&cinfo, JPEG_COM, 0xFFFF);
	jpeg_save_markers (&cinfo, JPEG_APP0 + 1, 0xFFFF);
	jpeg_save_markers (&cinfo, JPEG_APP0 + 13, 0xFFFF);

	jpeg_stdio_src (&cinfo, f);

	jpeg_read_header (&cinfo, TRUE);

	marker = (struct jpeg_marker_struct *) &cinfo.marker_list;

	while (marker) {
		gchar *str;
		gsize len;
		gint offset;
		guint sublen;

		switch (marker->marker) {
		case JPEG_COM:
			break;

		case JPEG_APP0 + 1:
			break;

		case JPEG_APP0 + 13:
			str = (gchar*) marker->data;
			len = marker->data_length;
			if (len > 0 && strncmp (PS3_NAMESPACE, str, PS3_NAMESPACE_LENGTH) == 0) {
				offset = iptc_jpeg_ps3_find_iptc ((guchar *)str, len, &sublen);
				if (offset > 0 && sublen > 0) {
					id = tracker_iptc_new ((const guchar *)str + offset, sublen, uri);
				}
			}
			break;

		default:
			marker = marker->next;
			continue;
		}

		marker = marker->next;
	}

        g_free (uri);
        fclose (f);

        return id;
}

static void
test_iptc_extraction (void)
{
        TrackerIptcData *data;

        data = load_iptc_blob (TOP_SRCDIR "/tests/libtracker-extract/iptc-img.jpg");
        g_assert (data);

        g_assert_cmpstr (data->keywords, ==, "Coverage, test");
        g_assert (g_str_has_prefix (data->date_created, "2011-10-22"));
        g_assert_cmpstr (data->byline, ==, "BylineValue");
        g_assert_cmpstr (data->byline_title, ==, "BylineTitleValue");
        g_assert_cmpstr (data->credit, ==, "CreditValue");
        g_assert_cmpstr (data->copyright_notice, ==, "IptcToolAuthors");
        g_assert_cmpstr (data->image_orientation, ==, "nfo:orientation-left");
        g_assert_cmpstr (data->city, ==, "Helsinki");
        g_assert_cmpstr (data->state, ==, "N/A");
        g_assert_cmpstr (data->sublocation, ==, "Ruoholahti");
        g_assert_cmpstr (data->country_name, ==, "Finland");
        g_assert_cmpstr (data->contact, ==, "Dilbert");

        tracker_iptc_free (data);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-extract/iptc/extraction",
                         test_iptc_extraction);
        return g_test_run ();
}
