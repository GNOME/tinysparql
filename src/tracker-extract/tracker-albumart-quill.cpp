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
			    QuillImageFilterFactory::createImageFilter("org.maemo.save");
			filter->setOption(QuillImageFilter::FileFormat, QVariant(QString("jpeg")));
			filter->setOption(QuillImageFilter::FileName, QVariant(QString(target)));
			filter->apply(image);
			delete filter;
		}
	}
	return TRUE;
}

gboolean
tracker_albumart_buffer_to_jpeg (const unsigned char *buffer,
                                 size_t               len,
                                 const gchar         *buffer_mime,
                                 const gchar         *target)
{
	if (!init) {
		init = TRUE;
	}
	QImageReader *reader = NULL;
	QByteArray array = QByteArray ((const char *) buffer, (int) len);
	QBuffer qbuffer(&array);
	qbuffer.open(QIODevice::ReadOnly);

	if (buffer_mime != NULL) {
		reader = new QImageReader::QImageReader (&qbuffer, QByteArray(buffer_mime));
	} else {
		QByteArray format = QImageReader::imageFormat(&qbuffer);
		if (!format.isEmpty ())
			reader = new QImageReader::QImageReader (&qbuffer, format);
	}
	if (reader != NULL) {
		QImage image = reader->read();
		QuillImageFilter *filter = QuillImageFilterFactory::createImageFilter("org.maemo.save");
		filter->setOption(QuillImageFilter::FileFormat, QVariant(QString("jpeg")));
		filter->setOption(QuillImageFilter::FileName, QVariant(QString(target)));
		filter->apply(image);
		delete reader;
		delete filter;
	}

	return TRUE;
}

G_END_DECLS
