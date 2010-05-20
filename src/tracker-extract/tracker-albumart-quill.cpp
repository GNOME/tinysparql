/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 *
 * Authors:
 * Philip Van Hoof <philip@codeminded.be>
 */

#include <quill.h>
#include <quillfile.h>

#include <QFile>
#include <QBuffer>
#include <QImageReader>
#include <QuillImageFilterFactory>

#include <glib.h>
#include <gio/gio.h>

#include "tracker-albumart-generic.h"


G_BEGIN_DECLS

static gboolean init = FALSE;

gboolean
tracker_albumart_file_to_jpeg (const gchar *filename,
                               const gchar *target)
{
	if (!init) {
		QuillImageFilter::registerAll();
		init = TRUE;
	}

	QFile file(filename);
	if (file.open(QIODevice::ReadOnly)) {

		QByteArray array = file.readAll();
		QBuffer buffer(&array);
		buffer.open(QIODevice::ReadOnly);

		QImageReader reader(&buffer);

		if (reader.canRead()) {
			QImage image = reader.read();
			QuillImageFilter *filter =
			    QuillImageFilterFactory::createImageFilter("Save");
			filter->setOption(QuillImageFilter::FileFormat, QVariant(QString("jpeg")));
			filter->setOption(QuillImageFilter::FileName, QVariant(QString(target)));
			filter->apply(image);
			delete filter;
		}
	}
	return TRUE;
}

static const gchar*
convert_content_type_to_qt (const gchar *content_type)
{
  guint i;
  static const gchar *conversions[12][2]  = { { "image/jpeg", "jpg" },
                                              { "image/png",  "png" },
                                              { "image/ppm",  "ppm" },
                                              { "image/xbm",  "xbm" },
                                              { "image/xpm",  "xpm" },
                                              { "image/mng",  "mng" },
                                              { "image/tiff", "tif" },
                                              { "image/bmp",  "bmp" },
                                              { "image/gif",  "gif" },
                                              { "image/pgm",  "pgm" },
                                              { "image/svg",  "svg" },
                                              { NULL, NULL }
                                            };

  for (i = 0; conversions[i][0] != NULL; i++)
    {
      if (g_strcmp0 (conversions[i][0], content_type) == 0)
        return conversions[i][1];
    }

  return NULL;
}

gboolean
tracker_albumart_buffer_to_jpeg (const unsigned char *buffer,
                                 size_t               len,
                                 const gchar         *buffer_mime,
                                 const gchar         *target)
{
	const gchar *qt_format;

	if (!init) {
		QuillImageFilter::registerAll();
		init = TRUE;
	}
	QImageReader *reader = NULL;
	QByteArray array = QByteArray ((const char *) buffer, (int) len);
	QBuffer qbuffer(&array);
	qbuffer.open(QIODevice::ReadOnly);
	qt_format = convert_content_type_to_qt (buffer_mime);
	if (qt_format != NULL) {
		reader = new QImageReader::QImageReader (&qbuffer, QByteArray(qt_format));
	} else {
		QByteArray format = QImageReader::imageFormat(&qbuffer);
		if (!format.isEmpty ())
			reader = new QImageReader::QImageReader (&qbuffer, format);
	}
	if (reader != NULL) {
		QImage image = reader->read();
		QuillImageFilter *filter = QuillImageFilterFactory::createImageFilter("Save");
		filter->setOption(QuillImageFilter::FileFormat, QVariant(QString("jpeg")));
		filter->setOption(QuillImageFilter::FileName, QVariant(QString(target)));
		filter->apply(image);
		delete reader;
		delete filter;
	}

	return TRUE;
}

G_END_DECLS
