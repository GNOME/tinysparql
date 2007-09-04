/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
 
#include "config.h"

#ifdef IOPRIO_SUPPORT

#include <stdio.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <tracker-utils.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifdef HAVE_LINUX_UNISTD_H
#include <linux/unistd.h>
#endif

#include "tracker-ioprio.h"


#if defined(__i386__)
#define __NR_ioprio_set         289
#define __NR_ioprio_get         290
#elif defined(__powerpc__) || defined(__powerpc64__)
#define __NR_ioprio_set         273
#define __NR_ioprio_get         274
#elif defined(__x86_64__)
#define __NR_ioprio_set         251
#define __NR_ioprio_get         252
#elif defined(__ia64__)
#define __NR_ioprio_set         1274
#define __NR_ioprio_get         1275
#elif defined(__alpha__)
#define __NR_ioprio_set         442
#define __NR_ioprio_get         443
#elif defined(__s390x__) || defined(__s390__)
#define __NR_ioprio_set         282
#define __NR_ioprio_get         283
#elif defined(__SH4__)
#define __NR_ioprio_set         288
#define __NR_ioprio_get         289
#elif defined(__SH5__)
#define __NR_ioprio_set         316
#define __NR_ioprio_get         317
#elif defined(__sparc__) || defined(__sparc64__)
#define __NR_ioprio_set         196
#define __NR_ioprio_get         218
#elif defined(__arm__)
#define __NR_ioprio_set         314
#define __NR_ioprio_get         315
#else
#error "Unsupported architecture!"
#endif


enum {
	IOPRIO_CLASS_NONE,
	IOPRIO_CLASS_RT,
	IOPRIO_CLASS_BE,
	IOPRIO_CLASS_IDLE,
};


enum {
	IOPRIO_WHO_PROCESS = 1,
	IOPRIO_WHO_PGRP,
	IOPRIO_WHO_USER,
};


#define IOPRIO_CLASS_SHIFT  13


static inline int
ioprio_set (int which, int who, int ioprio_val)
{
	return syscall (__NR_ioprio_set, which, who, ioprio_val);
}


int set_io_priority_idle (void)
{
        int ioprio, ioclass;

        ioprio = 7; /* priority is ignored with idle class */
        ioclass = IOPRIO_CLASS_IDLE << IOPRIO_CLASS_SHIFT;

        return ioprio_set (IOPRIO_WHO_PROCESS, 0, ioprio | ioclass);
}

int set_io_priority_best_effort (int ioprio_val)
{
        int ioclass;

        ioclass = IOPRIO_CLASS_BE << IOPRIO_CLASS_SHIFT;

        return ioprio_set (IOPRIO_WHO_PROCESS, 0, ioprio_val | ioclass);
}



void
ioprio (void)
{
	

	tracker_log ("Setting ioprio...");

	if (set_io_priority_idle () == -1) {
		if (set_io_priority_best_effort (7) == -1) {
			tracker_error ("ERROR: ioprio_set failed");
		}
	}
	
}

#endif
