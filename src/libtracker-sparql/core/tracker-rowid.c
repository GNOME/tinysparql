/*
 * Copyright (C) 2022 Red Hat Inc
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "tracker-rowid.h"

G_DEFINE_BOXED_TYPE (TrackerRowid, tracker_rowid,
                     tracker_rowid_copy, tracker_rowid_free)

TrackerRowid *
tracker_rowid_copy (TrackerRowid *rowid)
{
	return g_slice_dup (TrackerRowid, rowid);
}

void
tracker_rowid_free (TrackerRowid *rowid)
{
	g_slice_free (TrackerRowid, rowid);
}
