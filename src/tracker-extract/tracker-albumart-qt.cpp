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
#include <QImageWriter>
#include <QApplication>

#include <glib.h>

#include "tracker-albumart-generic.h"

G_BEGIN_DECLS

static QApplication *app = NULL;

void
tracker_albumart_plugin_init (void)
{
	int argc = 0;
	char **argv = NULL;

	app = new QApplication (argc, argv);
}

void
tracker_albumart_plugin_shutdown (void)
{
	delete app;
}

gboolean
tracker_albumart_file_to_jpeg (const gchar *filename,
                               const gchar *target)
{
	QFile file (filename);

	if (!file.open (QIODevice::ReadOnly)) {
		g_message ("Could not get QFile from file: '%s'", filename);
		return FALSE;
	}

	QByteArray array = file.readAll ();
	QBuffer buffer (&array);

	buffer.open (QIODevice::ReadOnly);

	QImageReader reader (&buffer);

	if (!reader.canRead ()) {
		g_message ("Could not get QImageReader from file: '%s', reader.canRead was FALSE",
		           filename);
		return FALSE;
	}

	QImage image;

	image = reader.read ();
	image.save (QString (target), "jpeg");

	return TRUE;
}

gboolean
tracker_albumart_buffer_to_jpeg (const unsigned char *buffer,
                                 size_t               len,
                                 const gchar         *buffer_mime,
                                 const gchar         *target)
{
	if (g_strcmp0 (buffer_mime, "image/jpeg") == 0 ||
	    g_strcmp0 (buffer_mime, "JPG") == 0) {

		g_debug ("Saving album art using raw data as uri:'%s'",
		         target);

		g_file_set_contents (target, (const gchar*) buffer, (gssize) len, NULL);
	} else {
		QImageReader *reader = NULL;
		QByteArray array;

		array = QByteArray ((const char *) buffer, (int) len);

		QBuffer qbuffer (&array);
		qbuffer.open (QIODevice::ReadOnly);

		if (buffer_mime != NULL) {
			reader = new QImageReader (&qbuffer, QByteArray (buffer_mime));
		} else {
			QByteArray format = QImageReader::imageFormat (&qbuffer);

			if (!format.isEmpty ()) {
				reader = new QImageReader (&qbuffer, format);
			}
		}

		if (!reader) {
			g_message ("Could not get QImageReader from buffer");
			return FALSE;
		}

		QImage image;

		image = reader->read ();
		image.save (QString (target), "jpeg");

		delete reader;
	}

	return TRUE;
}

G_END_DECLS
