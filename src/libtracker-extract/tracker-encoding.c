/*
 * Copyright (C) 2011 Nokia <ivan.frade@nokia.com>
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

#include <glib.h>
#include "tracker-encoding.h"

#ifdef HAVE_ENCA
#include "tracker-encoding-enca.h"
#endif

#ifdef HAVE_MEEGOTOUCH
#include "tracker-encoding-meegotouch.h"
#endif

gboolean
tracker_encoding_can_guess (void)
{
#if defined (HAVE_ENCA) || defined (HAVE_MEEGOTOUCH)
	return TRUE;
#else
	return FALSE;
#endif
}

gchar *
tracker_encoding_guess (const gchar *buffer,
                        gsize        size)
{
	gchar *encoding = NULL;

#ifdef HAVE_MEEGOTOUCH
	encoding = tracker_encoding_guess_meegotouch (buffer, size);
#endif /* HAVE_MEEGOTOUCH */

#ifdef HAVE_ENCA
	if (!encoding)
		encoding = tracker_encoding_guess_enca (buffer, size);
#endif /* HAVE_ENCA */

	return encoding;
}
