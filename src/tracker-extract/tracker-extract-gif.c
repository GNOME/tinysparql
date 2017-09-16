/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gif_lib.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

#define XMP_MAGIC_TRAILER_LENGTH 256
#define EXTENSION_RECORD_COMMENT_BLOCK_CODE 0xFE

typedef struct {
	const gchar *title;
	const gchar *date;
	const gchar *artist;
} MergeData;

typedef struct {
	gchar *width;
	gchar *height;
	gchar *comment;
} GifData;

typedef struct {
	unsigned int   byteCount;
	char          *bytes;
} ExtBlock;

static int
ext_block_append(ExtBlock *extBlock,
		 unsigned int len,
		 unsigned char extData[])
{
	extBlock->bytes = realloc(extBlock->bytes,extBlock->byteCount+len);
	if (extBlock->bytes == NULL) {
		return (GIF_ERROR);
	}

	memcpy(&(extBlock->bytes[extBlock->byteCount]), &extData[0], len);
	extBlock->byteCount += len;

	return (GIF_OK);
}

#if GIFLIB_MAJOR >= 5
static inline void
gif_error (const gchar *action, int err)
{
	const char *str = GifErrorString (err);
	if (str != NULL) {
		g_message ("%s, error: '%s'", action, str);
	} else {
		g_message ("%s, undefined error %d", action, err);
	}
}
#else /* GIFLIB_MAJOR >= 5 */
static inline void print_gif_error()
{
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && ((GIFLIB_MAJOR == 4 && GIFLIB_MINOR >= 2) || GIFLIB_MAJOR > 4)
	const char *str = GifErrorString ();
	if (str != NULL) {
		g_message ("GIF, error: '%s'", str);
	} else {
		g_message ("GIF, undefined error");
	}
#else
	PrintGifError();
#endif
}
#endif /* GIFLIB_MAJOR >= 5 */

/* giflib 5.1 changed the API of DGifCloseFile to take two arguments */
#if !defined(GIFLIB_MAJOR) || \
    !(GIFLIB_MAJOR > 5 || (GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1))
#define DGifCloseFile(a, b) DGifCloseFile(a)
#endif

static TrackerResource *
read_metadata (GifFileType          *gifFile,
               const gchar          *uri)
{
	TrackerResource *metadata;
	GifRecordType RecordType;
	int frameheight;
	int framewidth;
	unsigned char *framedata = NULL;
	GPtrArray *keywords;
	guint i;
	gint h;
	int status;
	MergeData md = { 0 };
	GifData   gd = { 0 };
	TrackerXmpData *xd = NULL;

	do {
		GifByteType *ExtData;
		int ExtCode;
		ExtBlock extBlock;

		if (DGifGetRecordType(gifFile, &RecordType) == GIF_ERROR) {
#if GIFLIB_MAJOR < 5
			print_gif_error ();
#else  /* GIFLIB_MAJOR < 5 */
			gif_error ("Could not read next GIF record type", gifFile->Error);
#endif /* GIFLIB_MAJOR < 5 */
			return NULL;
		}

		switch (RecordType) {
			case IMAGE_DESC_RECORD_TYPE:
			if (DGifGetImageDesc(gifFile) == GIF_ERROR) {
#if GIFLIB_MAJOR < 5
				print_gif_error();
#else  /* GIFLIB_MAJOR < 5 */
				gif_error ("Could not get GIF record information", gifFile->Error);
#endif /* GIFLIB_MAJOR < 5 */
				return NULL;
			}

			framewidth  = gifFile->Image.Width;
			frameheight = gifFile->Image.Height;

			framedata = g_malloc_n (framewidth, sizeof(GifPixelType));
			for (h = 0; h < frameheight; h++)
			{
				if (DGifGetLine(gifFile, framedata, framewidth)==GIF_ERROR) {
					g_free (framedata);
#if GIFLIB_MAJOR < 5
					print_gif_error();
#else  /* GIFLIB_MAJOR < 5 */
					gif_error ("Could not load a block of GIF pixes", gifFile->Error);
#endif /* GIFLIB_MAJOR < 5 */
					return NULL;
				}
			}

			gd.width  = g_strdup_printf ("%d", framewidth);
			gd.height = g_strdup_printf ("%d", frameheight);


			g_free (framedata);

		break;
		case EXTENSION_RECORD_TYPE:
			extBlock.bytes = NULL;
			extBlock.byteCount = 0;

			if ((status = DGifGetExtension (gifFile, &ExtCode, &ExtData)) != GIF_OK) {
				g_warning ("Problem getting the extension");
				return NULL;
			}
#if defined(HAVE_EXEMPI)
			if (ExtData && *ExtData &&
			    strncmp (&ExtData[1],"XMP Data",8) == 0) {
				while (ExtData != NULL && status == GIF_OK ) {
					if ((status = DGifGetExtensionNext (gifFile, &ExtData)) == GIF_OK) {
						if (ExtData != NULL) {
							if (ext_block_append (&extBlock, ExtData[0]+1, (char *) &(ExtData[0])) != GIF_OK) {
								g_warning ("Problem with extension data");
								return NULL;
							}
						}
					}
				}

				xd = tracker_xmp_new (extBlock.bytes,
				                      extBlock.byteCount-XMP_MAGIC_TRAILER_LENGTH,
				                      uri);

				g_free (extBlock.bytes);
			} else
#endif
			/* See Section 24. Comment Extension. in the GIF format definition */
			if (ExtCode == EXTENSION_RECORD_COMMENT_BLOCK_CODE &&
			    ExtData && *ExtData) {
				guint block_count = 0;

				/* Merge all blocks */
				do {
					block_count++;

					g_debug ("Comment Extension block found (#%u, %u bytes)",
					         block_count,
					         ExtData[0]);
					if (ext_block_append (&extBlock, ExtData[0], (char *) &(ExtData[1])) != GIF_OK) {
						g_warning ("Problem with Comment extension data");
						return NULL;
					}
				} while (((status = DGifGetExtensionNext(gifFile, &ExtData)) == GIF_OK) &&
				         ExtData != NULL);

				/* Add last NUL byte */
				g_debug ("Comment Extension blocks found (%u) with %u bytes",
				         block_count,
				         extBlock.byteCount);
				extBlock.bytes = g_realloc (extBlock.bytes, extBlock.byteCount + 1);
				extBlock.bytes[extBlock.byteCount] = '\0';

				/* Set commentt */
				gd.comment = extBlock.bytes;
			} else {
				do {
					status = DGifGetExtensionNext(gifFile, &ExtData);
				} while ( status == GIF_OK && ExtData != NULL);
			}
		break;
		case TERMINATE_RECORD_TYPE:
			break;
		default:
			break;
		}
	} while (RecordType != TERMINATE_RECORD_TYPE);


	if (!xd) {
		xd = g_new0 (TrackerXmpData, 1);
	}

	md.title = tracker_coalesce_strip (3, xd->title, xd->title2, xd->pdf_title);
	md.date = tracker_coalesce_strip (2, xd->date, xd->time_original);
	md.artist = tracker_coalesce_strip (2, xd->artist, xd->contributor);

	metadata = tracker_resource_new (NULL);
	tracker_resource_add_uri (metadata, "rdf:type", "nfo:Image");
	tracker_resource_add_uri (metadata, "rdf:type", "nmm:Photo");

	if (xd->license) {
		tracker_resource_set_string (metadata, "nie:license", xd->license);
	}

	if (xd->creator) {
		TrackerResource *creator = tracker_extract_new_contact (xd->creator);

		tracker_resource_set_relation (metadata, "nco:creator", creator);

		g_object_unref (creator);
	}

	tracker_guarantee_resource_date_from_file_mtime (metadata,
	                                                 "nie:contentCreated",
	                                                 md.date,
	                                                 uri);

	if (xd->description) {
		tracker_resource_set_string (metadata, "nie:description", xd->description);
	}

	if (xd->copyright) {
		tracker_resource_set_string (metadata, "nie:copyright", xd->copyright);
	}

	if (xd->make || xd->model) {
		TrackerResource *equipment = tracker_extract_new_equipment (xd->make, xd->model);

		tracker_resource_set_relation (metadata, "nfo:equipment", equipment);

		g_object_unref (equipment);
	}

	tracker_guarantee_resource_title_from_file (metadata,
	                                            "nie:title",
	                                            md.title,
	                                            uri,
	                                            NULL);

	if (md.artist) {
		TrackerResource *artist = tracker_extract_new_contact (xd->creator);

		tracker_resource_add_relation (metadata, "nco:contributor", artist);

		g_object_unref (artist);
	}

	if (xd->orientation) {
		TrackerResource *orientation;

		orientation = tracker_resource_new (xd->orientation);
		tracker_resource_set_relation (metadata, "nfo:orientation", orientation);
		g_object_unref (orientation);
	}

	if (xd->exposure_time) {
		tracker_resource_set_string (metadata, "nmm:exposureTime", xd->exposure_time);
	}

	if (xd->iso_speed_ratings) {
		tracker_resource_set_string (metadata, "nmm:isoSpeed", xd->iso_speed_ratings);
	}

	if (xd->white_balance) {
		TrackerResource *white_balance;

		white_balance = tracker_resource_new (xd->white_balance);
		tracker_resource_set_relation (metadata, "nmm:whiteBalance", white_balance);
		g_object_unref (white_balance);
	}

	if (xd->fnumber) {
		tracker_resource_set_string (metadata, "nmm:fnumber", xd->fnumber);
	}

	if (xd->flash) {
		TrackerResource *flash;

		flash = tracker_resource_new (xd->flash);
		tracker_resource_set_relation (metadata, "nmm:flash", flash);
		g_object_unref (flash);
	}

	if (xd->focal_length) {
		tracker_resource_set_string (metadata, "nmm:focalLength", xd->focal_length);
	}

	if (xd->metering_mode) {
		TrackerResource *metering;

		metering = tracker_resource_new (xd->metering_mode);
		tracker_resource_set_relation (metadata, "nmm:meteringMode", metering);
		g_object_unref (metering);
	}

	keywords = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	if (xd->keywords) {
		tracker_keywords_parse (keywords, xd->keywords);
	}

	if (xd->pdf_keywords) {
		tracker_keywords_parse (keywords, xd->pdf_keywords);
	}

	if (xd->rating) {
		tracker_resource_set_string (metadata, "nao:numericRating", xd->rating);
	}

	if (xd->subject) {
		tracker_keywords_parse (keywords, xd->subject);
	}

        if (xd->regions) {
                tracker_xmp_apply_regions_to_resource (metadata, xd);
        }

	for (i = 0; i < keywords->len; i++) {
		TrackerResource *tag;
		const gchar *p;

		p = g_ptr_array_index (keywords, i);
		tag = tracker_extract_new_tag (p);

		tracker_resource_set_relation (metadata, "nao:hasTag", tag);

		g_object_unref (tag);
	}
	g_ptr_array_free (keywords, TRUE);

	if (xd->publisher) {
		TrackerResource *publisher = tracker_extract_new_contact (xd->creator);

		tracker_resource_add_relation (metadata, "nco:creator", publisher);

		g_object_unref (publisher);
	}

	if (xd->type) {
		tracker_resource_set_string (metadata, "dc:type", xd->type);
	}

	if (xd->format) {
		tracker_resource_set_string (metadata, "dc:format", xd->format);
	}

	if (xd->identifier) {
		tracker_resource_set_string (metadata, "dc:identifier", xd->identifier);
	}

	if (xd->source) {
		tracker_resource_set_string (metadata, "dc:source", xd->source);
	}

	if (xd->language) {
		tracker_resource_set_string (metadata, "dc:language", xd->language);
	}

	if (xd->relation) {
		tracker_resource_set_string (metadata, "dc:relation", xd->relation);
	}

	if (xd->coverage) {
		tracker_resource_set_string (metadata, "dc:coverage", xd->coverage);
	}

	if (xd->address || xd->state || xd->country || xd->city ||
	    xd->gps_altitude || xd->gps_latitude || xd-> gps_longitude) {

		TrackerResource *location = tracker_extract_new_location (xd->address,
		        xd->state, xd->city, xd->country, xd->gps_altitude,
		        xd->gps_latitude, xd->gps_longitude);

		tracker_resource_set_relation (metadata, "slo:location", location);

		g_object_unref (location);
	}

	if (xd->gps_direction) {
		tracker_resource_set_string (metadata, "nfo:heading", xd->gps_direction);
	}

	if (gd.width) {
		tracker_resource_set_string (metadata, "nfo:width", gd.width);
		g_free (gd.width);
	}

	if (gd.height) {
		tracker_resource_set_string (metadata, "nfo:height", gd.height);
		g_free (gd.height);
	}

	if (gd.comment) {
		tracker_guarantee_resource_utf8_string (metadata, "nie:comment", gd.comment);
		g_free (gd.comment);
	}

	tracker_xmp_free (xd);

	return metadata;
}


G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerResource *metadata;
	goffset size;
	GifFileType *gifFile = NULL;
	gchar *filename, *uri;
	GFile *file;
	int fd;
#if GIFLIB_MAJOR >= 5
	int err;
#endif

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);
	size = tracker_file_get_size (filename);

	if (size < 64) {
		g_free (filename);
		return FALSE;
	}

	fd = tracker_file_open_fd (filename);

	if (fd == -1) {
		g_warning ("Could not open GIF file '%s': %s\n",
		           filename,
		           g_strerror (errno));
		g_free (filename);
		return FALSE;
	}	

#if GIFLIB_MAJOR < 5
	if ((gifFile = DGifOpenFileHandle (fd)) == NULL) {
		print_gif_error ();
#else   /* GIFLIB_MAJOR < 5 */
	if ((gifFile = DGifOpenFileHandle (fd, &err)) == NULL) {
		gif_error ("Could not open GIF file with handle", err);
#endif /* GIFLIB_MAJOR < 5 */
		g_free (filename);
		close (fd);
		return FALSE;
	}

	g_free (filename);

	uri = g_file_get_uri (file);

	metadata = read_metadata (gifFile, uri);

	g_free (uri);

	if (DGifCloseFile (gifFile, NULL) != GIF_OK) {
#if GIFLIB_MAJOR < 5
		print_gif_error ();
#else  /* GIFLIB_MAJOR < 5 */
		gif_error ("Could not close GIF file", gifFile->Error);
#endif /* GIFLIB_MAJOR < 5 */
	}

	if (metadata) {
		tracker_extract_info_set_resource (info, metadata);
		g_object_unref (metadata);
	}

	close (fd);

	return TRUE;
}
