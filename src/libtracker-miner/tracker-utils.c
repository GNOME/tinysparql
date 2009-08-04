/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include "tracker-utils.h"

void
tracker_throttle (gint multiplier)
{
	gint throttle;

	/* Get the throttle, add 5 (minimum value) so we don't do
	 * nothing and then multiply it by the factor given
	 */
#ifdef FIX
	throttle  = tracker_config_get_throttle (config);
#else 
	throttle  = 0;
#endif
	/* throttle += 5; */
	throttle *= multiplier;

	if (throttle > 0) {
		g_usleep (throttle);
	}
}
