/*
 * Copyright (C) 2013 Carlos Garnacho <carlos@lanedo.com>
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

#include "unicode/utypes.h"
#include "unicode/localpointer.h"
#include "unicode/uenum.h"
#include "unicode/ucsdet.h"

#include <glib.h>
#include "tracker-encoding-libicu.h"

gchar *
tracker_encoding_guess_icu (const gchar *buffer,
			    gsize        size)
{
	UCharsetDetector *detector = NULL;
	const UCharsetMatch *match;
	gchar *charset = NULL;
	UErrorCode status;
	const char *p_match = NULL;

	detector = ucsdet_open (&status);

	if (U_FAILURE (status))
		goto failure;

	if (size >= G_MAXINT32)
		goto failure;

	ucsdet_setText (detector, buffer, (int32_t) size, &status);

	if (U_FAILURE (status))
		goto failure;

	match = ucsdet_detect (detector, &status);

	if (match == NULL || U_FAILURE (status))
		goto failure;

	p_match = ucsdet_getName (match, &status);

	if (p_match == NULL || U_FAILURE (status))
		goto failure;

        charset = g_strdup ((const gchar *) p_match);

	if (charset)
		g_debug ("Guessing charset as '%s'", charset);

failure:
	if (detector)
		ucsdet_close (detector);

	return charset;
}
