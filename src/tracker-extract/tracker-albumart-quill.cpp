/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2010, Nokia
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


#include <QFile>
#include <QBuffer>
#include <QImageReader>
#include <QuillImageFilterFactory>

#include <glib.h>
#include <gio/gio.h>

#include "tracker-albumart-generic.h"

#define TRACKER_ALBUMART_QUILL_SAVE "org.maemo.save"

G_BEGIN_DECLS

gboolean
tracker_albumart_file_to_jpeg (const gchar *filename,
                               const gchar *target)
{
	QFile file (filename);

	if (!file.open (QIODevice::ReadOnly)) {
		g_message ("Could not get QFile from JPEG file:'%s'",
		           filename);
		return FALSE;
	}

	QByteArray array = file.readAll ();
	QBuffer buffer (&array);

	buffer.open (QIODevice::ReadOnly);

	QImageReader reader (&buffer);

	if (!reader.canRead ()) {
		g_message ("Could not get QImageReader from JPEG file, canRead was FALSE");
		return FALSE;
	}

	QImage image;
	QuillImageFilter *filter;

	image = reader.read ();
	filter = QuillImageFilterFactory::createImageFilter (TRACKER_ALBUMART_QUILL_SAVE);

	if (!filter) {
		g_message ("Could not get QuillImageFilter from JPEG file using:'%s'",
		           TRACKER_ALBUMART_QUILL_SAVE);

		return FALSE;
	}

	filter->setOption (QuillImageFilter::FileFormat, QVariant (QString ("jpeg")));
	filter->setOption (QuillImageFilter::FileName, QVariant (QString (target)));
	filter->apply (image);

	delete filter;

	return TRUE;
}

gboolean
tracker_albumart_buffer_to_jpeg (const unsigned char *buffer,
                                 size_t               len,
                                 const gchar         *buffer_mime,
                                 const gchar         *target)
{
	QImageReader *reader = NULL;
	QByteArray array;

	array = QByteArray ((const char *) buffer, (int) len);

	QBuffer qbuffer (&array);
	qbuffer.open (QIODevice::ReadOnly);

	if (buffer_mime != NULL) {
		reader = new QImageReader::QImageReader (&qbuffer, QByteArray (buffer_mime));
	} else {
		QByteArray format = QImageReader::imageFormat (&qbuffer);

		if (!format.isEmpty ()) {
			reader = new QImageReader::QImageReader (&qbuffer, format);
		}
	}

	if (!reader) {
		g_message ("Could not get QImageReader from JPEG buffer");
		return FALSE;
	}

	QImage image;
	QuillImageFilter *filter;

	image = reader->read ();
	filter = QuillImageFilterFactory::createImageFilter (TRACKER_ALBUMART_QUILL_SAVE);

	if (!filter) {
		g_message ("Could not get QuillImageFilter from JPEG buffer using:'%s'",
		           TRACKER_ALBUMART_QUILL_SAVE);
		delete reader;

		return FALSE;
	}

	filter->setOption (QuillImageFilter::FileFormat, QVariant (QString ("jpeg")));
	filter->setOption (QuillImageFilter::FileName, QVariant (QString (target)));
	filter->apply (image);

	delete reader;
	delete filter;

	return TRUE;
}

G_END_DECLS
