/* Tracker
 * io priority
 * Copyright (C) 2006, Anders Aagaard
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



#ifdef HAVE_LINUX_UNISTD_H
#include <linux/unistd.h>
#endif

#ifndef TRACKER_IOPRIO_H
#define TRACKER_IOPRIO_H

#ifdef IOPRIO_SUPPORT
	void ioprio();
#endif

#endif
