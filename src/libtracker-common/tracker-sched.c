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

#ifdef __linux__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <sched.h>

#include "tracker-sched.h"

gboolean
tracker_sched_idle (void)
{
	struct sched_param sp;

	/* Set process scheduling parameters:
	 * This is used so we don't steal scheduling priority from
	 * the most important applications - like the phone
	 * application which has a real time requirement here. This
	 * is detailed in Nokia bug #95573
	 */
	g_message ("Setting scheduler policy to SCHED_IDLE");

	if (sched_getparam (0, &sp) == 0) {
		if (sched_setscheduler (0, SCHED_IDLE, &sp) != 0) {
//LCOV_EXCL_START
			const gchar *str = g_strerror (errno);

			g_warning ("Could not set scheduler policy, %s",
			           str ? str : "no error given");

			return FALSE;
		}
	} else {
		const gchar *str = g_strerror (errno);

		g_warning ("Could not get scheduler policy, %s",
		           str ? str : "no error given");

		return FALSE;
	}
//LCOV_EXCL_END

	return TRUE;
}

#else /* __linux__ */

#include <glib.h>

#include "tracker-sched.h"

gboolean
tracker_sched_idle (void)
{
	return TRUE;
}

#endif /* __linux__ */
